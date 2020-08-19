#include "pch.h"
#include "TaskManager.h"
#include "SheepsMemory.h"
#include "SheepsMain.h"
#include "cstdio"

bool TaskManagerRuning = false;

std::map<int, t_task_config*> taskAll;
std::map<int, t_task_config*> taskDel;

std::list<t_task_error*> taskError;
std::mutex taskErrLock;

t_replay_dll default_api = { 0x0 };

static int changTask_work_thread(HTASKCFG task, uint8_t count)
{
	task->workThereaLock->lock();
	task->workThreadCount += count;
	uint8_t left = task->workThreadCount;
	task->workThereaLock->unlock();
	return left;
}

static int64_t GetSysTimeMicros()
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

static void update_user_time_clock(ReplayProtocol* proto)
{
	HMSGPOINTER pointer = &proto->MsgPointer;

	if (pointer->last > 0)
	{
		UINT64 nowtime = GetSysTimeMicros();
		proto->MsgPointer.start_real += nowtime - proto->MsgPointer.last;
		proto->MsgPointer.last = nowtime;
	}
}

static HUSER get_userAll_front(HTASKCFG task, size_t* usersize)
{
	HUSER user = NULL;
	task->userAllLock->lock();
	if (task->userAll->empty())
	{
		user = NULL;
	}
	else
	{
		*usersize = task->userAll->size();
		user = task->userAll->front();
		task->userAll->pop_front();
	}
	task->userAllLock->unlock();
	return user;
}

static bool add_to_userAll_tail(HTASKCFG task, HUSER user)
{
	task->userAllLock->lock();
	task->userAll->push_back(user);
	task->userAllLock->unlock();
	return true;
}

static HUSER get_userDes_front(HTASKCFG task)
{
	HUSER user = NULL;
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

static bool add_to_userDes_tail(HTASKCFG task, HUSER user)
{
	task->userDesLock->lock();
	task->userDes->push_back(user);
	task->userDesLock->unlock();
	return true;
}

static void destroy_task(HTASKCFG task)
{
	if (task->workThreadCount == 0)
	{
		LOG(clogId, LOG_DEBUG, "%s:%d Clean Task !", __func__, __LINE__);
		std::vector<t_cache_message*>::iterator it;
		for (it = task->messageList->begin(); it != task->messageList->end(); it++)
		{
			t_cache_message* data = *it;
			if (data->content)
				sheeps_free(data->content);
			sheeps_free(data);
			*it = NULL;
		}
		task->messageList->clear();
		delete task->messageList;

		HUSER user = NULL;
		size_t userAllSize = 0;
		bool userClean = true;
		while (user = get_userDes_front(task))
		{
			if (user->proto->_sockCount == 0)
			{
				default_api.destory(user->proto);		//若连接未关闭，可能导致崩溃
			}
			else
			{
				LOG(clogId, LOG_ERROR, " %d 用户连接未完全关闭，导致内存泄漏！\r\n", __LINE__);
				userClean = false;
			}
			sheeps_free(user);
		}
		delete task->userDes;
		delete task->userDesLock;

		while (user = get_userAll_front(task, &userAllSize))
		{
			if (user->proto->_sockCount == 0)
			{
				default_api.destory(user->proto);		//若连接未关闭，可能导致崩溃
			}
			else
			{
				LOG(clogId, LOG_ERROR, " %d 用户连接未完全关闭，导致内存泄漏！\r\n", __LINE__);
				userClean = false;
			}
			sheeps_free(user);
		}
		delete task->userAll;
		delete task->userAllLock;

		delete task->workThereaLock;

		if (userClean == true)
		{
			default_api.uninit(task);
		}
		else
		{
			LOG(clogId, LOG_ERROR, " %d 用户连接未完全关闭，导致内存泄漏！\r\n", __LINE__);
		}
		CloseLog(task->logfd);
		sheeps_free(task);
		taskDel.erase(task->taskID);
	}
}

static void ReInit(ReplayProtocol* proto, bool loop)
{
	if (loop)
		proto->SelfDead = false;
	else
		proto->SelfDead = true;
	proto->PlayState = PLAY_NORMAL;
	proto->MsgPointer = { 0x0 };
	proto->ReInit();
}

static HMESSAGE task_get_next_message(ReplayProtocol* proto, bool fast)
{
	HMSGPOINTER pointer = &(proto->MsgPointer);
	HTASKCFG task = proto->Task;
	if (pointer->index < task->messageList->size())
	{
		t_cache_message* msg = (*(task->messageList))[pointer->index];
		UINT64 nowtime = GetSysTimeMicros();
		if (pointer->start_real == 0)
		{
			pointer->start_record = msg->recordtime;
			pointer->start_real = nowtime;
		}
		if (fast)
		{
			pointer->last = nowtime;
			pointer->start_real = nowtime - (msg->recordtime - pointer->start_record);
		}
		else
		{
			INT64 shouldtime = msg->recordtime - pointer->start_record;
			INT64 realtime = nowtime - pointer->start_real;
			if (realtime < shouldtime)
				return NULL;
			pointer->last = nowtime;
		}
		pointer->index += 1;
		return msg;
	}
	return NULL;
}

static void Loop(ReplayProtocol* proto)
{
	if (proto->PlayState == PLAY_PAUSE)		//播放暂停
	{
		proto->TimeOut();
		update_user_time_clock(proto);
		return;
	}
	HMESSAGE message = NULL;
	if (proto->PlayState == PLAY_FAST)		//快进模式 or 正常播放模式
		message = task_get_next_message(proto, true);	/*获取下一步用户需要处理的事件消息*/
	else
		message = task_get_next_message(proto, false);
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
		proto->Event(message->type, message->ip, message->port, message->content, (int)message->contentlen, message->udp);
		break;
	case TYPE_REINIT:	/*用户重置事件，关闭所有连接，初始化用户资源*/
		ReInit(proto, message->isloop);
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

	t_handle_user* user = NULL;
	ReplayProtocol* proto = NULL;
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
					ReInit(user->proto, true);
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

	if (changTask_work_thread(task, -1) == 0)
		destroy_task(task);
	return 0;
}

static bool run_task_thread(HTASKCFG task)
{
	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	for (unsigned int i = 0; i < sysInfor.dwNumberOfProcessors * 2; i++)
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
	return true;
}

static HTASKCFG getTask_by_taskId(uint8_t taskID, bool create)
{
	std::map<int, HTASKCFG>::iterator iter;
	iter = taskAll.find(taskID);
	if (iter != taskAll.end())
	{
		return iter->second;
	}
	else
	{
		if (!create)
			return NULL;
		HTASKCFG task = (HTASKCFG)sheeps_malloc(sizeof(t_task_config));
		if (task == NULL)
		{
			return NULL;
		}
		char path[256] = { 0x0 };
		snprintf(path, sizeof(path), "%s\\task%03d.log", LogPath, taskID);
		task->logfd = RegisterLog(path, LOG_TRACE, 20, 86400, 2);
		task->workThereaLock = new std::mutex;
		task->messageList = new std::vector<t_cache_message*>;
		task->userAll = new std::list<HUSER>;
		task->userAllLock = new std::mutex;
		task->userDes = new std::list<HUSER>;
		task->userDesLock = new std::mutex;
		task->taskErr = &taskError;
		task->taskErrlock = &taskErrLock;
		taskAll.insert(std::pair<int, HTASKCFG>(taskID, task));
		return task;
	}
}

static HUSER create_new_user(HTASKCFG task, int userNumber, BaseFactory* factory)
{
	bool isNew = false;
	HUSER user = get_userDes_front(task);
	if (user == NULL)
	{
		isNew = true;
		user = (HUSER)sheeps_malloc(sizeof(t_handle_user));
		if (user == NULL)
			return NULL;
		user->proto = default_api.create();
		if (user->proto == NULL)
		{
			sheeps_free(user);
			return NULL;
		}
		user->proto->SetFactory(factory, CLIENT_PROTOCOL);
		user->proto->Task = task;
		user->proto->UserNumber = userNumber;
		user->protolock = user->proto->_protolock;
	}
	if (isNew)
		user->proto->Init();
	else
		ReInit(user->proto, true);
	return user;
}

static int task_add_user(HTASKCFG task, int userCount, BaseFactory* factory)
{
	uint16_t StartCount = task->userCount;
	task->userCount += userCount;
	t_handle_user* user = NULL;
	for (int i = 0; i < userCount; i++)
	{
		user = create_new_user(task, StartCount + i, factory);
		if (user)
			add_to_userAll_tail(task, user);
	}
	return 0;
}

int create_new_task(uint8_t taskid, uint8_t projectid, uint8_t machineid, bool ignorerr, int userconut, BaseFactory* factory)
{
	t_task_config* task;
	task = getTask_by_taskId(taskid, true);
	if (task == NULL)
		return -1;

	task->taskID = taskid;
	task->projectID = projectid;
	task->envID = 0;
	task->machineID = machineid;
	task->ignoreErr = ignorerr;

	default_api.init(task);

	task_add_user(task, userconut, factory);
	run_task_thread(task);
	return 0;
}

void set_task_log_level(uint8_t level, uint8_t taskID)
{
	HTASKCFG task = getTask_by_taskId(taskID, false);
	if (task != NULL)
	{
		SetLogLevel(task->logfd, level);
	}
}

/*bool stopTaskAll(uint8_t taskID)
{
	HTASKCFG task;
	std::map<int, HTASKCFG>::iterator iter;
	for (iter = taskAll.begin(); iter != taskAll.end(); iter++)
	{
		task = iter->second;
		taskAll.erase(iter);
		taskDel.insert(std::pair<int, HTASKCFG>(taskID, task));
		task->status = 4;
		iter = taskAll.begin();
		return true;
	}
	return false;
}*/

bool stop_task_by_id(uint8_t taskID)
{
	HTASKCFG task;
	std::map<int, HTASKCFG>::iterator iter;
	iter = taskAll.find(taskID);
	if (iter != taskAll.end())
	{
		task = iter->second;
		taskAll.erase(iter);
		taskDel.insert(std::pair<int, HTASKCFG>(taskID, task));
		task->status = 4;
		return true;
	}
	return false;
}

bool insert_message_by_taskId(uint8_t taskID, uint8_t type, char* ip, uint32_t port, char* content, uint64_t timestamp, uint32_t microsecond, bool udp)
{
	HTASKCFG task;
	task = getTask_by_taskId(taskID, false);
	if (task == NULL)
		return false;
	if (task->stopMessageCache)
		return true;

	t_cache_message* message = (t_cache_message*)sheeps_malloc(sizeof(t_cache_message));
	if (message == NULL)
	{
		return false;
	}
	message->udp = udp;
	message->type = type;
	if (ip)
	{
		memcpy(message->ip, ip, strlen(ip));
	}
	message->port = port;
	if (content)
	{
		Base64_Context str;
		str.data = (u_char*)content;
		str.len = strlen(content);

		Base64_Context temp;
		temp.len = (str.len) * 6 / 8;

		message->content = (char*)sheeps_malloc(temp.len);
		if (message->content == NULL)
			return false;
		temp.data = (u_char*)message->content;

		decode_base64(&temp, &str);
		message->contentlen = (uint32_t)temp.len;
	}
	message->recordtime = timestamp * 1000000 + microsecond;

	task->messageList->push_back(message);

	if (type == TYPE_REINIT)
		task->stopMessageCache = true;
	return true;
}

int task_add_user_by_taskid(uint8_t taskid, int userCount, BaseFactory* factory)
{
	t_task_config* task;
	task = getTask_by_taskId(taskid, false);
	if (task == NULL)
		return -1;
	task_add_user(task, userCount, factory);
	return 0;
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
	sheeps_free(error);
}

bool TaskUserSocketClose(HSOCKET hsock)
{
	if (hsock == NULL || hsock->sock == INVALID_SOCKET)
		return false;
	SOCKET sock = hsock->sock;
	hsock->sock = INVALID_SOCKET;
	(*(hsock->_user))->_sockCount -= 1;
	CancelIo((HANDLE)sock);
	closesocket(sock);
	return true;
}

bool TaskUserDead(ReplayProtocol* proto, const char* fmt, ...)
{
	proto->SelfDead = TRUE;

	t_task_error* err = (t_task_error*)sheeps_malloc(sizeof(t_task_error));
	if (err == NULL)
		return false;
	t_task_config* task = proto->Task;
	err->taskID = task->taskID;
	err->userID = proto->UserNumber;
	err->timeStamp = time(NULL);
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

void TaskLog(HTASKCFG task, uint8_t level, const char* fmt, ...)
{
	int logfd = task->logfd;
	if (logfd < 0 || CheckLogLevel(logfd, level))
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
	l += snprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, "\r\n");

	DWORD written;
	WriteFile(GetLogFileHandle(logfd), buf, l, &written, NULL);
}

void TaskUserLog(ReplayProtocol* proto, uint8_t level, const char* fmt, ...)
{
	int logfd = proto->Task->logfd;
	if (logfd < 0 || CheckLogLevel(logfd, level))
		return;

	int l;
	char buf[MAX_LOG_LEN];
	struct tm tmm;
	time_t now = time(NULL);
	localtime_s(&tmm, &now);
	char* slog = GetLogStr(level);
	l = snprintf(buf, MAX_LOG_LEN - 1, "[%s:%ld]:[NO.%d]:[%04d-%02d-%02d %02d:%02d:%02d]", slog, GetCurrentThreadId(), proto->UserNumber, tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);
	l += snprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, "\r\n");

	DWORD written;
	WriteFile(GetLogFileHandle(logfd), buf, l, &written, NULL);
}

static void TaskManagerForever(int projectid) 
{
	printf("正在启动...");
	Sleep(1000);
	printf("...");
	Sleep(1000);
	printf("...");
	char in[4] = { 0x0 };
	while (true)
	{
		system("CLS");
		printf("\nSheeps负载端正在运行，项目ID[%d]:\n\n", projectid);
		if (TaskManagerRuning)
			printf("状态：连接成功\n");
		else
			printf("状态：未连接\n");
		printf("\n选择:【Q退出】\n操作：");
		gets_s(in, 3);
		if (in[0] == 'Q')
			break;
	}
}

void TaskManagerRun(int projectid, CREATEAPI create, DESTORYAPI destory, INIT init, INIT uninit)
{
	default_api.create = create;
	default_api.destory = destory;
	default_api.init = init;
	default_api.uninit = uninit;

	char ip[32] = { 0x0 };
	GetPrivateProfileStringA("agent", "srv_ip", "127.0.0.1", ip, sizeof(ip), ConfigFile);
	int port = GetPrivateProfileIntA("agent", "srv_port", 1080, ConfigFile);
	SheepsClientRun(ip, port, projectid);
	TaskManagerForever(projectid);
}