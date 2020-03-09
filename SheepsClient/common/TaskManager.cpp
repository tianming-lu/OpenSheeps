#include "TaskManager.h"
#include "./../common/log.h"
#include "mycrypto.h"

map<int, t_task_config*> taskAll;
map<int, t_task_config*> taskDel;
//map<time_t, list<t_handle_user*>*> userRubbish;

list<t_task_error*> taskError;
mutex taskErrLock;

HLOG_Context gHlog = NULL;

bool init_task_log(char* configfile)
{
	char file[128] = { 0x0 };
	GetPrivateProfileStringA("LOG", "task_file", "./cicadalog/task.log", file, sizeof(file), configfile);
	int loglevel = GetPrivateProfileIntA("LOG", "task_level", 0, "./stresstest/config.ini");
	int maxsize = GetPrivateProfileIntA("LOG", "task_size", 20, "./stresstest/config.ini");
	int timesplit = GetPrivateProfileIntA("LOG", "task_time", 3600, "./stresstest/config.ini");
	gHlog = RegTaskLog(file, loglevel, maxsize, timesplit, 2);
	return true;
}

t_task_config* getTask_by_taskId(int taskID)
{
	map<int, t_task_config*>::iterator iter;
	iter = taskAll.find(taskID);
	if (iter != taskAll.end())
	{
		return iter->second;
	}
	else
	{
		t_task_config* task = (t_task_config*)GlobalAlloc(GPTR, sizeof(t_task_config));
		if (task == NULL)
		{
			return NULL;
		}
		task->hlog = gHlog;
		task->workThereaLock = new mutex;
		task->messageList = new vector<t_message_data*>;
		//task->userPointer = new list<t_handle_user*>;
		task->userAll = new list<t_handle_user*>;
		task->userAllLock = new mutex;
		task->userDes = new list<t_handle_user*>;
		task->userDesLock = new mutex;
		task->taskErr = &taskError;
		task->taskErrlock = &taskErrLock;
		taskAll.insert(pair<int, t_task_config*>(taskID, task));
		return task;
	}
}

bool stopTaskAll(int taskID)
{
	t_task_config* task;
	map<int, t_task_config*>::iterator iter;
	for (iter = taskAll.begin(); iter != taskAll.end(); iter++)
	{
		task = iter->second;
		taskAll.erase(iter);
		taskDel.insert(pair<int, t_task_config*>(taskID, task));
		task->status = 4;
		iter = taskAll.begin();
		return true;
	}
	return false;
}

bool stopTask_by_taskId(int taskID)
{
	t_task_config* task;
	map<int, t_task_config*>::iterator iter;
	iter = taskAll.find(taskID);
	if (iter != taskAll.end())
	{
		task = iter->second;
		taskAll.erase(iter);
		taskDel.insert(pair<int, t_task_config*>(taskID, task));
		task->status = 4;
		return true;
	}
	return false;
}

void destroyTask()
{
	time_t ctime = time(NULL);
	t_task_config* task = NULL;
	map<int, t_task_config*>::iterator iter;
	iter = taskDel.begin();
	while (iter != taskDel.end())
	{
		task = iter->second;

		if (task->workThreadCount == 0 )
		{
			LOG(clogId, LOG_DEBUG, "%s:%d Clean Task !", __func__, __LINE__);
			vector<t_message_data*>::iterator it;
			for (it = task->messageList->begin(); it != task->messageList->end(); it++)
			{
				GlobalFree(*it);
				*it = NULL;
			}
			task->messageList->clear();
			delete task->messageList;

			t_handle_user* user = NULL;
			bool userClean = true;
			while (user = get_userDes_front(task))
			{
				if (user->proto->sockCount == 0)
				{ 
					task->dll.destory(user->proto);		//若连接未关闭，可能导致崩溃
				}
				else
				{
					LOG(clogId, LOG_ERROR," %d 用户连接未完全关闭，导致内存泄漏！\r\n", __LINE__);
					userClean = false;
				}
				GlobalFree(user);
			}
			delete task->userDes;
			delete task->userDesLock;

			while (user = get_userAll_front(task))
			{
				if (user->proto->sockCount == 0)
				{
					task->dll.destory(user->proto);		//若连接未关闭，可能导致崩溃
				}
				else
				{
					LOG(clogId, LOG_ERROR, " %d 用户连接未完全关闭，导致内存泄漏！\r\n", __LINE__);
					userClean = false;
				}
				GlobalFree(user);
			}
			delete task->userAll;
			delete task->userAllLock;
			
			delete task->workThereaLock;

			if (userClean == true && task->dll.dllHandle != NULL)
			{
				task->dll.uninit(task);
				FreeLibrary((HMODULE)task->dll.dllHandle);
			}
			else
			{
				LOG(clogId, LOG_ERROR, " %d 用户连接未完全关闭，导致内存泄漏！\r\n", __LINE__);
			}

			//userRubbish.insert(pair<time_t, list<t_handle_user*>*>(ctime, task->userPointer));

			GlobalFree(task);
			taskDel.erase(iter);
			iter = taskDel.end();
		}

		iter++;
	}

	/*map<time_t, list<t_handle_user*>*>::iterator ite;
	for (ite = userRubbish.begin(); ite != userRubbish.end(); ite++)
	{
		if (ctime - ite->first > 30)
		{
			ite->second->clear();
			delete ite->second;
			userRubbish.erase(ite);
			ite = userRubbish.end();
		}
	}*/
}

bool changTask_work_thread(t_task_config* task, short count)
{
	task->workThereaLock->lock();
	task->workThreadCount += count;
	task->workThereaLock->unlock();
	return true;
}

bool insertMessage_by_taskId(int taskID, short type, char* ip, int port, char* content, UINT64 timestamp, int microsecond)
{
	t_task_config* task;
	task = getTask_by_taskId(taskID);
	if (task->stopMessageCache)
		return true;

	t_message_data* message = (t_message_data*)GlobalAlloc(GPTR, sizeof(t_message_data));
	if (message == NULL)
	{
		return false;
	}
	message->type = type;
	if (ip)
		message->ip = string(ip);
	message->port = port;
	if (content)
	{
		Base64_Context str;
		str.data = (u_char*)content;
		str.len = strlen(content);

		Base64_Context temp;
		temp.len = (str.len) * 6 / 8;
		temp.data = (u_char*)GlobalAlloc(GPTR, temp.len);
		if (temp.data == NULL)
		{
			return 0;
		}
		decode_base64(&temp, &str);

		message->content = string((char*)temp.data, temp.len);
		GlobalFree(temp.data);
	}
	message->recordtime = timestamp * 1000000 + microsecond;

	task->messageList->push_back(message);

	if (type == TYPE_REINIT)
		task->stopMessageCache = true;
	return true;
}

bool dll_init(t_task_config* task, char* rootpath)
{
	char dllname[128] = { 0x0 };
	int n = snprintf(dllname, sizeof(dllname), "%s/%d/RePlay.dll", rootpath, task->projectID);

	task->dll.dllHandle = LoadLibraryA(dllname);
	if (task->dll.dllHandle == NULL)
	{
		return false;
	}

	task->dll.create = (CREATEAPI)GetProcAddress((HMODULE)task->dll.dllHandle, "CreateUser");
	task->dll.destory = (DESTORYAPI)GetProcAddress((HMODULE)task->dll.dllHandle, "DestoryUser");
	task->dll.init = (INIT)GetProcAddress((HMODULE)task->dll.dllHandle, "Init");
	task->dll.uninit = (INIT)GetProcAddress((HMODULE)task->dll.dllHandle, "UnInit");
	task->dll.threadinit = (INIT)GetProcAddress((HMODULE)task->dll.dllHandle, "ThreadInit");
	task->dll.threaduninit = (INIT)GetProcAddress((HMODULE)task->dll.dllHandle, "ThreadUnInit");

	task->dll.init(task);
	return true;
}

t_handle_user* get_userAll_front(t_task_config* task)
{
	t_handle_user* user = NULL;
	task->userAllLock->lock();
	if (task->userAll->empty())
	{
		user = NULL;
	}
	else 
	{
		user = task->userAll->front();
		task->userAll->pop_front();
	}
	task->userAllLock->unlock();
	return user;
}

bool add_to_userAll_tail(t_task_config* task, t_handle_user* user) 
{
	task->userAllLock->lock();
	task->userAll->push_back(user);
	task->userAllLock->unlock();
	return true;
}

t_handle_user* get_userDes_front(t_task_config* task)
{
	t_handle_user* user = NULL;
	task->userDesLock->lock();
	if (task->userDes->empty())
	{
		user = NULL;
	}
	else
	{
		user = task->userDes->front();
		task->userDes->pop_front();
	}
	task->userDesLock->unlock();
	return user;
}

bool add_to_userDes_tail(t_task_config* task, t_handle_user* user)
{
	task->userDesLock->lock();
	task->userDes->push_back(user);
	task->userDesLock->unlock();
	return true;
}

int64_t GetSysTimeMicros()
{
	// 从1601年1月1日0:0:0:000到1970年1月1日0:0:0:000的时间(单位100ns)
#define EPOCHFILETIME   (116444736000000000UL)
	FILETIME ft;
	LARGE_INTEGER li;
	int64_t tt = 0;
	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	// 从1970年1月1日0:0:0:000到现在的微秒数(UTC时间)
	tt = (li.QuadPart - EPOCHFILETIME) / 10;
	return tt;
}

HMESSAGE TaskGetNextMessage(UserProtocol* proto)
{
	HMsgPointer pointer = &(proto->MsgPointer);
	t_task_config* task = proto->Task;
	if (pointer->index < task->messageList->size())
	{
		t_message_data* msg = (*(task->messageList))[pointer->index];
		if (msg->type == TYPE_REINIT)
		{
			memset(pointer, 0, sizeof(MsgPointer));
			if (msg->isloop == FALSE)
				proto->SelfDead = TRUE;
			return msg;
		}
		UINT64 nowtime = GetSysTimeMicros();
		if (pointer->start_real == 0)
		{
			pointer->start_record = msg->recordtime;
			pointer->start_real = nowtime;
		}
		UINT64 shouldtime = msg->recordtime - pointer->start_record;
		UINT64 realtime = nowtime - pointer->start_real;
		if (realtime < shouldtime)
			return NULL;
		pointer->index += 1;
		return msg;
	}
	return NULL;
}

bool TaskUserSocketClose(HSOCKET hsock)
{
	if (hsock == NULL || hsock->sock == INVALID_SOCKET)
		return false;
	SOCKET sock = hsock->sock;
	hsock->sock = INVALID_SOCKET;
	(*(hsock->user))->sockCount -= 1;
	CancelIo((HANDLE)sock);
	closesocket(sock);
	return true;
}

bool TaskUserDead(UserProtocol* proto, const char* fmt, ...)
{
	proto->SelfDead = TRUE;

	t_task_error* err = (t_task_error*)GlobalAlloc(GPTR, sizeof(t_task_error));
	if (err == NULL)
		return false;
	t_task_config* task = proto->Task;
	err->taskID = task->taskID;
	err->taskErrId = 1;
	int l = 0;
#define REASON_LEN 512
	char reason[REASON_LEN];
	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(reason + l, (size_t)REASON_LEN - l - 1, fmt, ap);
	va_end(ap);

	memcpy(err->errMsg, reason, (l < sizeof(err->errMsg)) ? l : sizeof(err->errMsg));
	task->taskErrlock->lock();
	task->taskErr->push_back(err);
	task->taskErrlock->unlock();
	return true;
}

t_task_error* get_task_error_front()
{
	t_task_error* err = NULL;
	taskErrLock.lock();
	if (!taskError.empty())
	{
		err = taskError.front();
		taskError.pop_front();
	}
	taskErrLock.unlock();
	return err;
}

void delete_task_error(t_task_error* error)
{
	GlobalFree(error);
}

void TaskLog(t_task_config* task, int level, const char* fmt, ...)
{
	LOG_Context* nle = (LOG_Context*)task->hlog;
	if (nle->LogLevel > level)
		return;
	int l;
	char buf[MAX_LOG_LEN];
	struct tm tmm;
	time_t now = time(NULL);
	localtime_s(&tmm, &now);
	char* slog = GetLogStr(level);
	l = snprintf(buf, MAX_LOG_LEN - 1, "[%s:%ld]:[%04d-%02d-%02d %02d:%02d:%02d]", slog, GetCurrentThreadId(), tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);

	DWORD written;
	WriteFile(nle->File, buf, l, &written, NULL);
}

void TaskUserLog(UserProtocol* proto, int level, const char* fmt, ...)
{
	LOG_Context* nle = (LOG_Context*)proto->Task->hlog;
	if (nle->LogLevel > level)
		return;
	int l;
	char buf[MAX_LOG_LEN];
	struct tm tmm;
	time_t now = time(NULL);
	localtime_s(&tmm, &now);
	char* slog = GetLogStr(level);
	l = snprintf(buf, MAX_LOG_LEN - 1, "[%s:%ld]:[%d]:[%04d-%02d-%02d %02d:%02d:%02d]", slog, GetCurrentThreadId(), proto->UserNumber, tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);

	DWORD written;
	WriteFile(nle->File, buf, l, &written, NULL);
}