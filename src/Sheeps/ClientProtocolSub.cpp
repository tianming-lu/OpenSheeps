/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/


#include "pch.h"
#include "ClientProtocolSub.h"
#include "TaskManager.h"
#ifdef __WINDOWS__
#include "io.h"
#include "direct.h"
#endif // __WINDOWS__


int clogId = -1;

static std::map<std::string, t_file_info> updatefile, allfile, localfile;

int ClientLogInit(const char *configfile)
{
	const char* file = config_get_string_value("LOG", "client_file", "client");
	char temp[24] = { 0x0 };
	GetTimeString(time(NULL), temp, sizeof(temp));
	char fullpath[256] = { 0x0 };
	snprintf(fullpath, sizeof(fullpath), "%s%s_%s.log", LogPath, file, temp);
	int loglevel = config_get_int_value("LOG", "client_level", 0);
	int maxsize = config_get_int_value("LOG", "client_size", 20);
	int timesplit = config_get_int_value("LOG", "client_time", 3600);
	clogId = RegisterLog(fullpath, loglevel, maxsize, timesplit, 5);
	return 0;
}

static int MsgResponse(HSOCKET hsock, int cmdNo, int retCode, const char* msg)
{
	char buf[128] = { 0x0 };
	int len = snprintf(buf, sizeof(buf), "{\"retCode\":%d,\"retMsg\":\"%s\"}", retCode, msg);
	t_stress_protocol_head head = {0x0};
	head.cmdNo = cmdNo;
	head.msgLen = len + sizeof(t_stress_protocol_head);
	
	char sockbuf[256] = { 0x0 };
	memcpy(sockbuf, &head, sizeof(t_stress_protocol_head));
	memcpy(sockbuf + sizeof(t_stress_protocol_head), buf, len);
	HsocketSend(hsock, sockbuf, head.msgLen);
	return 0;
}

static int MsgSend(HSOCKET hsock, int cmdNo, char* data, int len)
{
	t_stress_protocol_head head = {0x0};
	head.cmdNo = cmdNo;
	head.msgLen = len + sizeof(t_stress_protocol_head);

	char sockbuf[256] = { 0x0 };
	memcpy(sockbuf, (char*)&head, sizeof(t_stress_protocol_head));
	memcpy(sockbuf + sizeof(t_stress_protocol_head), data, len);
	HsocketSend(hsock, sockbuf, head.msgLen);
	return 0;
}

int client_cmd_2_task_init(HSOCKET hsock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* projectId = cJSON_GetObjectItem(root, "projectID");
	cJSON* userCount = cJSON_GetObjectItem(root, "UserCount");
	cJSON* machineId = cJSON_GetObjectItem(root, "MachineID");
	cJSON* ignoreErr = cJSON_GetObjectItem(root, "IgnoreErr");
	cJSON* logLevel = cJSON_GetObjectItem(root, "LogLevel");
	cJSON* parms = cJSON_GetObjectItem(root, "Parms");
	if (taskId == NULL || projectId == NULL || userCount == NULL || machineId == NULL || logLevel == NULL || parms == NULL ||
		taskId->type != cJSON_Number || projectId->type != cJSON_Number ||
		userCount->type != cJSON_Number || machineId->type != cJSON_Number ||
		logLevel->type != cJSON_Number || parms->type != cJSON_String)
	{
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}

	int taskid = taskId->valueint;
	int projectid = projectId->valueint;
	int machineid = machineId->valueint;
	bool ignorerr = false;
	if (ignoreErr != NULL && ignoreErr->type == cJSON_True)
		ignorerr = true;
	int usercount = userCount->valueint;
	int loglevel = logLevel->valueint;
	char* sparms = parms->valuestring;

	create_new_task(taskid, projectid, machineid, ignorerr, usercount, loglevel, sparms, hsock->factory);
	return 0;
}

int client_cmd_3_task_connection_create(HSOCKET hsock, int cmdNO, cJSON* root)
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
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	insert_message_by_taskId(realTaskID, TYPE_CONNECT, ip->valuestring, port->valueint, NULL, timestamp->valueint, microsecond->valueint, false);

	return 0;
}

int client_cmd_4_task_connection_send(HSOCKET hsock, int cmdNO, cJSON* root)
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
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}

	bool isudp = false;
	cJSON* udp = cJSON_GetObjectItem(root, "Iotype");
	if (udp != NULL)
		isudp = true;

	int realTaskID = taskId->valueint;
	insert_message_by_taskId(realTaskID, TYPE_SEND, ip->valuestring, port->valueint, content->valuestring, timestamp->valueint, microsecond->valueint, isudp);

	return 0;
}

int client_cmd_5_task_connection_close(HSOCKET hsock, int cmdNO, cJSON* root)
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
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	insert_message_by_taskId(realTaskID, TYPE_CLOSE, ip->valuestring, port->valueint, NULL, timestamp->valueint, microsecond->valueint, false);

	return 0;
}

int client_cmd_6_task_finish(HSOCKET hsock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	if (taskId == NULL || taskId->type != cJSON_Number)
	{
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	stop_task_by_id(realTaskID);
	LOG(clogId, LOG_DEBUG, "task finish taskid[%d]\r\n", realTaskID);
	return 0;
}

int client_cmd_8_task_reinit(HSOCKET hsock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* loop = cJSON_GetObjectItem(root, "Loop");
	if (taskId == NULL || loop == NULL || taskId->type != cJSON_Number || (loop->type != cJSON_True && loop->type != cJSON_False))
	{
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}
	int realTaskID = taskId->valueint;
	insert_message_by_taskId(realTaskID, TYPE_REINIT, NULL, loop->valueint, NULL, 0, 0, false);
	LOG(clogId, LOG_DEBUG, "task reinit task[%d] flag[%d]\r\n", realTaskID, loop->valueint);
	return 0;
}

#ifdef __WINDOWS__
void getFiles(char* path, std::map<std::string, t_file_info>* files)
{
	intptr_t hFile = 0;
	struct _finddata_t fileinfo;
	
	char allfile[256] = { 0x0 };
	snprintf(allfile, sizeof(allfile), "%s\\*", path);

	char fullpath[256] = { 0x0 };

	if ((hFile = _findfirst(allfile, &fileinfo)) != -1)
	{
		do
		{
			memset(fullpath, 0, sizeof(fullpath));
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, fileinfo.name);
				if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
					getFiles(fullpath, files);
			}
			else
			{
				snprintf(fullpath, sizeof(fullpath), "%s\\%s", path, fileinfo.name);
				t_file_info info = { 0x0 };
				getfilemd5view(fullpath, info.fmd5, sizeof(info.fmd5));

				files->insert(std::pair<std::string, t_file_info>(fullpath, info));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}
#endif

int sendUpfileMsg(HSOCKET hsock)
{
	if (updatefile.empty())
	{
		MsgResponse(hsock, 11, 0, "OK");
		LOG(clogId, LOG_DEBUG, "%s-%d files sync done!\r\n", __func__, __LINE__);
		return 1;
	}
	std::map<std::string, t_file_info>::iterator itt;
	itt = updatefile.begin();
	char buff[512] = { 0x0 };
	size_t size = 5120;
	if (itt->second.size - itt->second.offset < size)
		size = itt->second.size - itt->second.offset;
	int n = snprintf(buff, sizeof(buff), "{\"File\":\"%s\",\"Offset\":%zd,\"Size\":%zd}", itt->first.c_str(), itt->second.offset, size);
	MsgSend(hsock, 10, buff, n);
	return 0;
}

int client_cmd_9_sync_files(HSOCKET hsock, int cmdNO, cJSON* root)
{
	cJSON* files = cJSON_GetObjectItem(root, "filelist");
	if (files == NULL || files->type != cJSON_Array)
	{
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}

	int dev = config_get_int_value("mode", "dev", 0);
	if (dev == 1)
	{
		LOG(clogId, LOG_FAULT, "dev mode, give up file sync!\r\n");
		sendUpfileMsg(hsock);
		return 0;
	}

	updatefile.clear();
	allfile.clear();
	localfile.clear();
#ifdef __WINDOWS__
	getFiles(ProjectPath, &localfile);

	std::map<std::string, t_file_info>::iterator it, iter;

	char fullpath[256] = { 0x0 };
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
		
		t_file_info finfo = { 0x0 };
		memset(&finfo, 0, sizeof(t_file_info));
		snprintf(finfo.fmd5, sizeof(finfo.fmd5), fmd5->valuestring);
		finfo.size = size->valueint;

		char* p = strchr(file->valuestring, '\\');
		if (p == NULL)
			continue;
		
		memset(fullpath, 0, sizeof(fullpath));
		snprintf(fullpath, sizeof(fullpath), "%s%s", EXE_Path, p + 1);

		allfile.insert(std::pair<std::string, t_file_info>(fullpath, finfo));

		it = localfile.find(fullpath);
		if (it != localfile.end())
		{
			if (strcmp(it->second.fmd5, fmd5->valuestring) == 0)
			{
				continue;
			}
		}
		updatefile.insert(std::pair<std::string, t_file_info>(std::string(file->valuestring), finfo));
	}
	
	for (it = localfile.begin(); it != localfile.end(); ++it)
	{
		iter = allfile.find(it->first.c_str());
		if (iter == allfile.end())
			DeleteFileA(it->first.c_str());
	}
#endif // __WINDOWS__
	MsgResponse(hsock, cmdNO, 0, "OK");
	LOG(clogId, LOG_DEBUG, "%s-%d files sync start!\r\n", __func__, __LINE__);
	sendUpfileMsg(hsock);
	return 0;
}

#ifdef __WINDOWS__
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

	for (size_t i = 0; i < nPathLen; ++i)
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
#endif // __WINDOWS__

int client_cmd_10_download_file(HSOCKET hsock, int cmdNO, cJSON* root)
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
	std::map<std::string, t_file_info>::iterator itt;
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

	char fullpath[256] = { 0x0 };
	snprintf(fullpath, sizeof(fullpath), "%s%s", EXE_Path, p + 1);

#ifdef __WINDOWS__
	if (itt->second.file == NULL)
	{
		CreateFileDirectory(fullpath);
		int ret = fopen_s(&itt->second.file, fullpath, "wb");
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
#endif // __WINDOWS__
	sendUpfileMsg(hsock);
	return 0;
}

int client_cmd_12_task_change_user_count(HSOCKET hsock, int cmdNO, cJSON* root)
{
	cJSON* taskId = cJSON_GetObjectItem(root, "TaskID");
	cJSON* change = cJSON_GetObjectItem(root, "Change");
	if (taskId == NULL || change == NULL || taskId->type != cJSON_Number || change->type != cJSON_Number )
	{
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}

	task_add_user_by_taskid(taskId->valueint, change->valueint, hsock->factory);
	
	MsgResponse(hsock, cmdNO, 0, "OK");
	return 0;
}

int client_cmd_14_charge_task_loglevel(HSOCKET hsock, int cmdNO, cJSON* root)
{
	cJSON* loglevel = cJSON_GetObjectItem(root, "LogLevel");
	cJSON* taskid = cJSON_GetObjectItem(root, "TaskID");
	if (loglevel == NULL || taskid == NULL || loglevel->type != cJSON_Number || taskid->type != cJSON_Number)
	{
		MsgResponse(hsock, cmdNO, 1, "参数错误");
		return -1;
	}
	set_task_log_level(loglevel->valueint, taskid->valueint);
	MsgResponse(hsock, cmdNO, 0, "OK");
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