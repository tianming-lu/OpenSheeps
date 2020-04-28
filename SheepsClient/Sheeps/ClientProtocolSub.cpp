#include "pch.h"
#include "io.h"
#include "direct.h"
#include "ClientProtocolSub.h"
#include "./../common/TaskManager.h"
#include "./../common/log.h"
#include "./../common/mycrypto.h"
using namespace std;

BaseFactory* subfactory = NULL;
int clogId = -1;

map<string, t_file_info> updatefile, allfile, localfile;

int ClientLogInit(const char *configfile)
{
	char file[128] = { 0x0 };
	GetPrivateProfileStringA("LOG", "client_file", "./log/client.log", file, sizeof(file), configfile);
	int loglevel = GetPrivateProfileIntA("LOG", "client_level", 0, configfile);
	int maxsize = GetPrivateProfileIntA("LOG", "client_size", 20, configfile);
	int timesplit = GetPrivateProfileIntA("LOG", "client_time", 3600, configfile);
	clogId = RegisterLog(file, loglevel, maxsize, timesplit, 2);
	return 0;
}

static int MsgResponse(HSOCKET sock, int cmdNo, int retCode, const char* msg)
{
	char buf[128] = { 0x0 };
	int len = snprintf(buf, sizeof(buf), "{\"retCode\":%d,\"retMsg\":\"%s\"}", retCode, msg);
	t_stress_protocol_head head;
	head.cmdNo = cmdNo;
	head.msgLen = len + sizeof(t_stress_protocol_head);
	
	char sockbuf[256] = { 0x0 };
	memcpy(sockbuf, &head, sizeof(t_stress_protocol_head));
	memcpy(sockbuf + sizeof(t_stress_protocol_head), buf, len);
	IOCPPostSendEx(sock, sockbuf, head.msgLen);
	return 0;
}

static int MsgSend(HSOCKET sock, int cmdNo, char* data, int len)
{
	t_stress_protocol_head head;
	head.cmdNo = cmdNo;
	head.msgLen = len + sizeof(t_stress_protocol_head);

	char sockbuf[256] = { 0x0 };
	memcpy(sockbuf, (char*)&head, sizeof(t_stress_protocol_head));
	memcpy(sockbuf + sizeof(t_stress_protocol_head), data, len);
	IOCPPostSendEx(sock, sockbuf, head.msgLen);
	return 0;
}

static void ReInit(UserProtocol* proto)
{
	proto->SelfDead = false;
	proto->MsgPointer = { 0x0 };
	proto->ReInit();
}

static void Loop(UserProtocol* proto)
{
	if (proto->PlayPause == true)
	{
		proto->TimeOut();
		return;
	}

	HMESSAGE message = TaskGetNextMessage(proto);  /*获取下一步用户需要处理的事件消息*/
	if (message == NULL)
	{
		proto->TimeOut();
		return;
	}
	switch (message->type)
	{
	case TYPE_CONNECT: /*连接事件*/
	case TYPE_CLOSE:	/*关闭连接事件*/
	case TYPE_SEND:	/*向连接发送消息事件*/
		proto->Event(message->type, message->ip, message->port, message->content);
		break;
	case TYPE_REINIT:	/*用户重置事件，关闭所有连接，初始化用户资源*/
		ReInit(proto);
		break;
	default:
		break;
	}
	return;
}

DWORD WINAPI taskWorkerThread(LPVOID pParam)
{
	t_task_config* task = (t_task_config*)pParam;
	changTask_work_thread(task, 1);
	if (task->dll.threadinit)
		task->dll.threadinit(task);

	t_handle_user* user = NULL;
	UserProtocol* proto = NULL;
	size_t userAllSize = 0;
	while (task->status < 4)   //任务运行中
	{
		for (int i = 0; i < 10; i++)
		{
			user = (t_handle_user*)get_userAll_front(task, &userAllSize);
			if (user == NULL)
				break;
			proto = user->proto;
			if (task->userCount < userAllSize)
			{
				user->protolock->lock();
				proto->Destroy();
				user->protolock->unlock();
				add_to_userDes_tail(task, user);
				continue;
			}
			
			user->protolock->lock();
			if (proto->SelfDead == TRUE)
			{
				if (task->ignoreErr)
				{
					ReInit(user->proto);
					user->protolock->unlock();
					add_to_userAll_tail(task, user);
				}
				else
				{
					proto->Destroy();
					user->protolock->unlock();
					LOG(clogId, LOG_DEBUG, "user destroy\r\n");
					add_to_userDes_tail(task, user);
				}
				continue;
			}
			Loop(proto);
			user->protolock->unlock();
			add_to_userAll_tail(task, user);
		}
		Sleep(1);
	}

	while (task->status == 4)  //清理资源
	{
		user = (t_handle_user*)get_userAll_front(task, &userAllSize);
		if (user == NULL)
			break;
		proto = user->proto;
		user->protolock->lock();

		proto->Destroy();

		user->protolock->unlock();
		add_to_userDes_tail(task, user);
		Sleep(1);
	}

	if (task->dll.threaduninit)
		task->dll.threaduninit(task);
	changTask_work_thread(task, -1);
	return 0;
}

int client_cmd_2_task_init(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* projectId = cJSON_GetObjectItem(root, "projectID");
	cJSON* envid = cJSON_GetObjectItem(root, "EnvID");
	cJSON* userCount = cJSON_GetObjectItem(root, "UserCount");
	cJSON* machineId = cJSON_GetObjectItem(root, "MachineID");
	cJSON* ignoreErr = cJSON_GetObjectItem(root, "IgnoreErr");
	if (taskId == NULL || projectId == NULL ||envid == NULL || userCount == NULL || machineId == NULL ||
		taskId->type != cJSON_Number || projectId->type != cJSON_Number || envid->type != cJSON_Number||
		userCount->type != cJSON_Number || machineId->type != cJSON_Number)
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}

	int realTaskID = taskId->valueint;

	t_task_config* task;
	task = getTask_by_taskId(realTaskID);
	if (task == NULL)
	{
		MsgResponse(sock, cmdNO, 2, "内存错误");
		return -2;
	}
	
	task->taskID = realTaskID;
	task->projectID = projectId->valueint;
	task->envID = envid->valueint;
	task->machineID = machineId->valueint;
	if (ignoreErr != NULL && ignoreErr->type == cJSON_True)
	task->ignoreErr = true;

	bool ret = dll_init(task, DllPath);
	if (ret == false)
	{
		MsgResponse(sock, cmdNO, 3, "dll初始化错误");
		LOG(clogId, LOG_DEBUG, "%s:%d project[%d] dll init filed!\r\n", __func__, __LINE__, task->projectID);
		return -3;
	}
	//LOG("user size %lld\n", task->userAll->size());

	uint16_t StartCount = task->userCount;
	task->userCount += userCount->valueint;
	t_handle_user* user = NULL;
	for (int i = 0; i < userCount->valueint; i++)
	{
		user = get_userDes_front(task);
		if (user == NULL)
		{
			user = (t_handle_user*)GlobalAlloc(GPTR, sizeof(t_handle_user));
			if (user == NULL)
				continue;
			user->proto = task->dll.create();
			if (user->proto == NULL)
				continue;
			user->proto->Task = task;
			user->proto->UserNumber = StartCount + i;
			user->proto->self = user->proto;
			user->proto->factory = subfactory;
			user->protolock = user->proto->protolock;

			user->proto->ProtoInit();
			/*task->userPointer->push_back(user);
			user->proto->self = task->userPointer->back()->proto;*/
		}
		else
		{
			ReInit(user->proto);
		}

		add_to_userAll_tail(task, user);
	}

	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	for (unsigned int i = 0; i < sysInfor.dwNumberOfProcessors*2; i++)
	//for (unsigned int i = 0; i < 1; i++)
	{
		HANDLE ThreadHandle;
		ThreadHandle = CreateThread(NULL, 0, taskWorkerThread, task, 0, NULL);
		if (NULL == ThreadHandle) {
			LOG(clogId, LOG_ERROR, "%s-%d create thread failed!", __func__, __LINE__);
			return false;
		}
		CloseHandle(ThreadHandle);
	}
	LOG(clogId, LOG_DEBUG, "task init taskid[%d], user[%d]  all[%d]\r\n", task->taskID, userCount->valueint, task->userCount);
	return 0;
}

int client_cmd_3_task_connection_create(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* ip = cJSON_GetObjectItem(root, "Host");
	cJSON* port = cJSON_GetObjectItem(root, "Port");
	cJSON* timestamp = cJSON_GetObjectItem(root, "Timestamp");
	cJSON* microsecond = cJSON_GetObjectItem(root, "Microsecond");
	if (taskId == NULL || ip == NULL || port == NULL ||timestamp == NULL || microsecond == NULL ||
		taskId->type != cJSON_Number || ip->type != cJSON_String || port->type != cJSON_Number ||
		timestamp->type != cJSON_Number || microsecond->type != cJSON_Number)
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	insertMessage_by_taskId(realTaskID, TYPE_CONNECT, ip->valuestring, port->valueint, NULL, timestamp->valueint, microsecond->valueint);

	return 0;
}

int client_cmd_4_task_connection_send(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* ip = cJSON_GetObjectItem(root, "Host");
	cJSON* port = cJSON_GetObjectItem(root, "Port");
	cJSON* content = cJSON_GetObjectItem(root, "Msg");
	cJSON* timestamp = cJSON_GetObjectItem(root, "Timestamp");
	cJSON* microsecond = cJSON_GetObjectItem(root, "Microsecond");
	if (taskId == NULL || ip == NULL || port == NULL || content == NULL || timestamp == NULL || microsecond == NULL ||
		taskId->type != cJSON_Number || ip->type != cJSON_String || port->type != cJSON_Number ||
		content->type != cJSON_String || timestamp->type != cJSON_Number || microsecond->type != cJSON_Number)
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	insertMessage_by_taskId(realTaskID, TYPE_SEND, ip->valuestring, port->valueint, content->valuestring, timestamp->valueint, microsecond->valueint);

	return 0;
}

int client_cmd_5_task_connection_close(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* ip = cJSON_GetObjectItem(root, "Host");
	cJSON* port = cJSON_GetObjectItem(root, "Port");
	cJSON* timestamp = cJSON_GetObjectItem(root, "Timestamp");
	cJSON* microsecond = cJSON_GetObjectItem(root, "Microsecond");
	if (taskId == NULL || ip == NULL || port == NULL || timestamp == NULL || microsecond == NULL ||
		taskId->type != cJSON_Number || ip->type != cJSON_String || port->type != cJSON_Number ||
		timestamp->type != cJSON_Number || microsecond->type != cJSON_Number)
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	insertMessage_by_taskId(realTaskID, TYPE_CLOSE, ip->valuestring, port->valueint, NULL, timestamp->valueint, microsecond->valueint);

	return 0;
}

int client_cmd_6_task_finish(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	if (taskId == NULL || taskId->type != cJSON_Number)
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	stopTask_by_taskId(realTaskID);
	LOG(clogId, LOG_DEBUG, "task finish taskid[%d]\r\n", realTaskID);
	return 0;
}

int client_cmd_8_task_reinit(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* loop = cJSON_GetObjectItem(root, "Loop");
	if (taskId == NULL || loop == NULL || taskId->type != cJSON_Number || (loop->type != cJSON_True && loop->type != cJSON_False))
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	insertMessage_by_taskId(realTaskID, TYPE_REINIT, NULL, loop->valueint, NULL, 0, 0);
	LOG(clogId, LOG_DEBUG, "task reinit task[%d] flag[%d]\r\n", realTaskID, loop->valueint);
	return 0;
}

void getFiles(string path, map<string, t_file_info>* files)
{
	intptr_t hFile = 0;
	struct _finddata_t fileinfo;
	string p;
	if ((hFile = _findfirst(p.assign(path).append("\\*").c_str(), &fileinfo)) != -1)
	{
		do
		{
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
					getFiles(p.assign(path).append(fileinfo.name), files);
			}
			else
			{
				if (string(path).compare(string(DllPath)) != 0 && path.find("log") == string::npos)
				{
					string fullpath = string(path + "\\" + fileinfo.name);
					char md5[64] = { 0x0 };
					getfilemd5view(fullpath.c_str(), (unsigned char*)md5, sizeof(md5));
					t_file_info info;
					info.fmd5 = string(md5);
					files->insert(pair<string, t_file_info>(fullpath, info));
					//LOG(clogId, LOG_DEBUG, "%s \r\n", fullpath.c_str());
				}
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}

int sendUpfileMsg(HSOCKET sock)
{
	if (updatefile.empty())
	{
		MsgResponse(sock, 11, 0, "OK");
		LOG(clogId, LOG_DEBUG, "%s-%d files sync done!\r\n", __func__, __LINE__);
		return 1;
	}
	map<string, t_file_info>::iterator itt;
	itt = updatefile.begin();
	char buff[512] = { 0x0 };
	size_t size = 5120;
	if (itt->second.size - itt->second.offset < size)
		size = itt->second.size - itt->second.offset;
	int n = snprintf(buff, sizeof(buff), "{\"File\":\"%s\",\"Offset\":%lld,\"Size\":%lld}", itt->first.c_str(), itt->second.offset, size);
	MsgSend(sock, 10, buff, n);
	return 0;
}

int client_cmd_9_sync_files(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* files = cJSON_GetObjectItem(root, "filelist");
	if (files == NULL || files->type != cJSON_Array)
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}

	int dev = GetPrivateProfileIntA("mode", "dev", 0, ConfigFile);
	if (dev == 1)
	{
		LOG(clogId, LOG_FAULT, "dev mode, give up file sync!\r\n");
		sendUpfileMsg(sock);
		return 0;
	}

	updatefile.clear();
	allfile.clear();
	localfile.clear();
	
	getFiles(DllPath, &localfile);

	map<string, t_file_info>::iterator it, iter;
	int size = cJSON_GetArraySize(files);
	for (int i = 0; i < size; i++)
	{
		cJSON* item = cJSON_GetArrayItem(files, i);
		cJSON* file = cJSON_GetObjectItem(item, "File");
		cJSON* size = cJSON_GetObjectItem(item, "Size");
		cJSON* fmd5 = cJSON_GetObjectItem(item, "Md5");
		if (file == NULL || size == NULL || fmd5 == NULL ||
			file->type != cJSON_String || size->type != cJSON_Number || fmd5->type != cJSON_String)
		{
			LOG(clogId, LOG_DEBUG, "%s:%d pram error", __func__, __LINE__);
			continue;
		}
		
		t_file_info finfo;
		memset(&finfo, 0, sizeof(t_file_info));
		finfo.fmd5 = string(fmd5->valuestring);
		finfo.size = size->valueint;

		char* p = strchr(file->valuestring, '\\');
		if (p == NULL)
			continue;
		/*p = strchr(p + 1, '\\');
		if (p == NULL)
			continue;*/
		string fullpath = string(DllPath) + string(p+1);

		allfile.insert(pair<string, t_file_info>(string(fullpath), finfo));

		it = localfile.find(fullpath);
		if (it != localfile.end())
		{
			if (it->second.fmd5.compare(string(fmd5->valuestring)) == 0)
			{
				continue;
			}
		}
		updatefile.insert(pair<string, t_file_info>(string(file->valuestring), finfo));
	}
	
	for (it = localfile.begin(); it != localfile.end(); it++)
	{
		iter = allfile.find(it->first.c_str());
		if (iter == allfile.end())
			DeleteFileA(it->first.c_str());
	}

	MsgResponse(sock, cmdNO, 0, "OK");
	LOG(clogId, LOG_DEBUG, "%s-%d files sync start!\r\n", __func__, __LINE__);
	sendUpfileMsg(sock);
	return 0;
}


bool IsDirExist(const char* pszDir)
{
	if (pszDir == NULL)
		return false;

	return (_access(pszDir, 0) == 0);
}

bool CreateFileDirectory(const char* dir)
{
	if (NULL == dir)
		return false;

	char path[MAX_PATH];
	size_t nPathLen = 0;

	strcpy_s(path, sizeof(path), dir);
	if ((nPathLen = strlen(path)) < 1)
		return false;

	for (int i = 0; i < nPathLen; ++i)
	{
		if (path[i] == '\\')
		{
			path[i] = '\0';
			if (IsDirExist(path) == FALSE)
				if (0 != _mkdir(path))
					break;
			path[i] = '\\';
		}
	}

	return 0;//(0 == _mkdir(path) || EEXIST == errno); // EEXIST => errno.h    errmo => stdlib.h
}

int client_cmd_10_download_file(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* file = cJSON_GetObjectItem(root, "File");
	cJSON* offset = cJSON_GetObjectItem(root, "Offset");
	cJSON* order = cJSON_GetObjectItem(root, "Order");
	cJSON* size = cJSON_GetObjectItem(root, "Size");
	cJSON* data = cJSON_GetObjectItem(root, "Data");
	if (file == NULL || offset == NULL || order == NULL || size == NULL || data == NULL
		|| file->type != cJSON_String ||offset->type != cJSON_Number || order->type != cJSON_Number
		|| size->type != cJSON_Number ||data->type != cJSON_String)
	{
		return -1;
	}
	map<string, t_file_info>::iterator itt;
	itt = updatefile.find(file->valuestring);
	if (itt == updatefile.end())
	{
		LOG(clogId, LOG_ERROR, "%s:%d file not found\r\n", __func__, __LINE__);
		return -1;
	}

	char* p = strchr(file->valuestring, '\\');
	if (p == NULL)
	{
		LOG(clogId, LOG_ERROR, "%s:%d file path error\r\n", __func__, __LINE__);
		return -1;
	}
	/*p = strchr(p + 1, '\\');
	if (p == NULL)
	{
		LOG(clogId, LOG_ERROR, "%s:%d file path error\r\n", __func__, __LINE__);
		return -1;
	}*/
	string fullpath = string(DllPath) + string(p + 1);

	if (itt->second.file == NULL)
	{
		CreateFileDirectory(fullpath.c_str());
		int ret = fopen_s(&itt->second.file, fullpath.c_str(), "wb");
		if (ret != 0)
		{
			LOG(clogId, LOG_ERROR, "%s:%d open file error\r\n", __func__, __LINE__);
			return -1;
		}
	}
	char buf[10240] = { 0x0 };
	size_t n = base64_decode(data->valuestring, strlen(data->valuestring), buf);
	fwrite(buf, n, 1, itt->second.file);

	itt->second.offset += size->valueint;
	if (itt->second.offset == itt->second.size)
	{
		fclose(itt->second.file);
		updatefile.erase(itt);
	}
	sendUpfileMsg(sock);
	return 0;
}

int client_cmd_12_task_change_user_count(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* change = cJSON_GetObjectItem(root, "Change");
	if (taskId == NULL || change == NULL || taskId->type != cJSON_Number || change->type != cJSON_Number )
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}

	t_task_config* task;
	task = getTask_by_taskId(taskId->valueint);
	if (task == NULL)
	{
		MsgResponse(sock, cmdNO, 2, "内存错误");
		return -2;
	}

	uint16_t StartCount = task->userCount;
	task->userCount += change->valueint;
	t_handle_user* user = NULL;
	for (int i = 0; i < change->valueint; i++)
	{
		user = get_userDes_front(task);
		if (user == NULL)
		{
			user = (t_handle_user*)GlobalAlloc(GPTR, sizeof(t_handle_user));
			if (user == NULL)
				continue;
			if (task->dll.dllHandle == NULL)
			{
				MsgResponse(sock, cmdNO, 1, "dll file not found");
				LOG(clogId, LOG_ERROR, "dll Handle is NULL!\r\n");
				return 0;
			}
			user->proto = task->dll.create();
			if (user->proto == NULL)
				continue;
			user->proto->Task = task;
			user->proto->UserNumber = StartCount + i;
			user->proto->self = user->proto;
			user->proto->factory = subfactory;
			user->protolock = user->proto->protolock;

			user->proto->ProtoInit();
			/*task->userPointer->push_back(user);
			user->proto->self = task->userPointer->back()->proto;*/
		}
		else
		{
			ReInit(user->proto);
		}
		
		add_to_userAll_tail(task, user);
	}
	
	MsgResponse(sock, cmdNO, 0, "OK");
	LOG(clogId, LOG_DEBUG, "task add taskid[%d], user[%d]  all[%d]\r\n", task->taskID, change->valueint, task->userCount);
	return 0;
}

int client_cmd_14_charge_task_loglevel(HSOCKET sock, int cmdNO, cJSON* root)
{
	cJSON* loglevel = cJSON_GetObjectItem(root, "LogLevel");
	if (loglevel == NULL || loglevel->type != cJSON_Number)
	{
		MsgResponse(sock, cmdNO, 1, "参数错误");
		return -1;
	}
	set_task_log_level(loglevel->valueint);
	MsgResponse(sock, cmdNO, 0, "OK");
	return 0;
}

std::map<int, client_cmd_cb> ClientFunc{
	{2,		client_cmd_2_task_init},
	{3,		client_cmd_3_task_connection_create},
	{4,		client_cmd_4_task_connection_send},
	{5,		client_cmd_5_task_connection_close},
	{6,		client_cmd_6_task_finish},
	{8,		client_cmd_8_task_reinit},
	{9,		client_cmd_9_sync_files},
	{10,	client_cmd_10_download_file},
	{12,	client_cmd_12_task_change_user_count},
	{14,	client_cmd_14_charge_task_loglevel}
};

void do_client_func_by_cmd(HSOCKET hsock, int cmdNO, cJSON* root)
{
	std::map<int, client_cmd_cb>::iterator iter = ClientFunc.find(cmdNO);
	if (iter != ClientFunc.end())
		iter->second(hsock, cmdNO, root);
}