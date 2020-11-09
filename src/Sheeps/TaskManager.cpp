#include "pch.h"
#include "TaskManager.h"
#include "SheepsMemory.h"
#include "SheepsMain.h"
#include <thread>

bool TaskManagerRuning = false;

std::map<int, t_task_config*> taskAll;
std::map<int, t_task_config*> taskDel;

t_replay_dll default_api = { 0x0 };

static void update_user_time_clock(ReplayProtocol* proto)
{
	HMSGPOINTER pointer = &proto->MsgPointer;

	if (pointer->last > 0)
	{
		uint64_t nowtime = GetSysTimeMicros();
		proto->MsgPointer.start_real += nowtime - proto->MsgPointer.last;
		proto->MsgPointer.last = nowtime;
	}
}

static void get_userAll_splice(HTASKCFG task, std::list<ReplayProtocol*>* dst, int count, int &cut_count)
{
	std::list<ReplayProtocol*>* src = task->userAll;
	task->userAllLock->lock();

	if (src->empty())
	{
		task->userAllLock->unlock();
		return;
	}
	int usize = (int)src->size();
	count = count > usize ? usize : count;
	std::list<ReplayProtocol*>::iterator it = src->begin();
	advance(it, count);
	
	dst->splice(dst->end(), *src, src->begin(), it);
	if (task->userCount < task->aliveCount)
	{
		int temp = task->aliveCount - task->userCount;
		cut_count = temp > count ? count : temp;
		task->aliveCount -= cut_count;
	}
	task->userAllLock->unlock();
	
}

static void add_to_userAll(HTASKCFG task, std::list<ReplayProtocol*>* src, int cut_count)
{
	std::list<ReplayProtocol*>* dst = task->userAll;
	uint16_t src_size = (uint16_t)src->size();
	task->userAllLock->lock();
	if (src_size)
		dst->splice(dst->end(), *src);
	if (cut_count < 0)
	{
		task->aliveCount += src_size;
	}
	else 
	{
		task->aliveCount -= cut_count;
	}
	task->userAllLock->unlock();
}

static ReplayProtocol* get_userDes_splice(HTASKCFG task, std::list<ReplayProtocol*>* dst)
{
	std::list<ReplayProtocol*>* src = task->userDes;
	std::list<ReplayProtocol*>::iterator iter;
	ReplayProtocol* user;
	task->userDesLock->lock();
	if (src->empty())
	{
		task->userDesLock->unlock();
		return NULL;
	}
	iter = src->begin();
	user = *iter;
	dst->splice(dst->end(), *src, iter);
	task->userDesLock->unlock();
	return user;
}

static void add_to_userDes(HTASKCFG task, std::list<ReplayProtocol*>* src)
{
	if (src->empty()) return;
	std::list<ReplayProtocol*>* dst = task->userDes;
	task->userDesLock->lock();
	dst->splice(dst->end(), *src);
	task->userDesLock->unlock();
}

static void destroy_task(HTASKCFG task)
{
	LOG(clogId, LOG_DEBUG, "%s:%d Clean Task !\r\n", __func__, __LINE__);
	std::vector<t_cache_message*>::iterator it;
	for (it = task->messageList->begin(); it != task->messageList->end(); ++it)
	{
		t_cache_message* data = *it;
		if (data->content)
			sheeps_free(data->content);
		sheeps_free(data);
		*it = NULL;
	}
	task->messageList->clear();
	delete task->messageList;

	std::list<ReplayProtocol*>::iterator iter;
	ReplayProtocol* user = NULL;
	bool userClean = true;
	for (iter = task->userAll->begin(); iter != task->userAll->end(); ++iter)
	{
		user = *iter;
		if (user->sockCount == 0 && default_api.destory)
		{
			default_api.destory(user);		//若连接未关闭，可能导致崩溃
		}
		else
		{
			TaskUserLog(user, LOG_ERROR, "userlist用户连接未完全关闭，导致内存泄漏！");
			userClean = false;
		}
	}
	for (iter = task->userDes->begin(); iter != task->userDes->end(); ++iter)
	{
		user = *iter;
		if (user->sockCount == 0 && default_api.destory)
		{
			default_api.destory(user);		//若连接未关闭，可能导致崩溃
		}
		else
		{
			TaskUserLog(user, LOG_ERROR, "userlist用户连接未完全关闭，导致内存泄漏！");
			userClean = false;
		}
	}

	delete task->userDes;
	delete task->userDesLock;
	delete task->userAll;
	delete task->userAllLock;

	if (userClean == true && default_api.taskstop)
	{
		default_api.taskstop(task);
	}
	else
	{
		TaskLog(task, LOG_ERROR, "尚有用户连接未完全关闭，导致内存泄漏！");
	}
	CloseLog(task->logfd);
	sheeps_free(task);
	taskDel.erase(task->taskID);
	LOG(clogId, LOG_DEBUG, "%s:%d Clean Task Over!\r\n", __func__, __LINE__);
}

static void ReInit(ReplayProtocol* proto, bool loop)
{
	if (!loop)
	{
		proto->SelfDead = true;
	}
	else
	{
		proto->SelfDead = false;
		proto->PlayState = PLAY_NORMAL;
		proto->MsgPointer = { 0x0 };
		proto->ReInit();
	}
}

static HMESSAGE task_get_next_message(ReplayProtocol* proto, bool fast)
{
	HMSGPOINTER pointer = &(proto->MsgPointer);
	HTASKCFG task = proto->Task;
	if (pointer->index < task->messageList->size())
	{
		t_cache_message* msg = (*(task->messageList))[pointer->index];
		uint64_t nowtime = GetSysTimeMicros();
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
			int64_t shouldtime = msg->recordtime - pointer->start_record;
			int64_t realtime = nowtime - pointer->start_real;
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

static inline long task_add_workThead(t_task_config* task)
{
#ifdef __WINDOWS__   //原子操作线程计数
	return InterlockedIncrement(&task->workThreadCount);
#else
	return __sync_add_and_fetch(&task->workThreadCount, 1);
#endif // __WINDOWS__
}

static inline long task_sub_workThead(t_task_config* task)
{
#ifdef __WINDOWS__
	return InterlockedDecrement(&task->workThreadCount);
#else
	return (__sync_sub_and_fetch(&task->workThreadCount, 1);
#endif // __WINDOWS__
}

#ifdef __WINDOWS__
DWORD WINAPI taskWorkerThread(LPVOID pParam)
#else
int taskWorkerThread(void* pParam)
#endif // __WINDOWS__
{
	t_task_config* task = (t_task_config*)pParam;
	task_add_workThead(task);
	
	std::list<ReplayProtocol*>* userlist = NULL;
	std::list<ReplayProtocol*>* dellist = NULL;
	std::list<ReplayProtocol*>::iterator iter;
	ReplayProtocol* user = NULL;
	int need_cut, cuted;  //需要剪切的用户数，被剪切的用户数
	userlist = new(std::nothrow) std::list<ReplayProtocol*>;
	dellist = new(std::nothrow) std::list<ReplayProtocol*>;
	if (userlist == NULL || dellist == NULL)
	{
		LOG(clogId, LOG_ERROR, "%s:%d malloc error\r\n", __func__, __LINE__);
		goto do_clean;
	}
	
	while (task->status < 4)   //任务运行中
	{
		TimeSleep(1);
		need_cut = 0;
		cuted = 0;
		get_userAll_splice(task, userlist, 20, need_cut);
		if (userlist->empty()) {continue; }
		iter = userlist->begin();
		for ( ; iter != userlist->end();)
		{
			user = *iter;
			if (need_cut > 0)
			{
				user->protolock->lock();
				if (user->LastError[0] == 0x0)
					snprintf(user->LastError, sizeof(user->LastError), "%s", "user cuted");
				user->Destroy();
				user->LastError[0] = 0x0;
				user->protolock->unlock();

				dellist->splice(dellist->end(), *userlist, iter++);
				need_cut--;
				continue;
			}

			user->protolock->lock();
			if (user->SelfDead == true)
			{
				if (task->ignoreErr)
				{
					ReInit(user, true);
					user->LastError[0] = 0x0;
					user->protolock->unlock();
				}
				else
				{
					user->Destroy();
					user->LastError[0] = 0x0;
					user->protolock->unlock();

					dellist->splice(dellist->end(), *userlist, iter++);
					cuted++;
				}
				continue;
			}
			Loop(user);
			user->protolock->unlock();
			++iter;
		}
		add_to_userAll(task, userlist, cuted);
		add_to_userDes(task, dellist);
	}

	while (task->status == 4)  //清理资源
	{
		get_userAll_splice(task, userlist, 10, need_cut);
		if (userlist->empty())
			break;
		iter = userlist->begin();
		for (; iter != userlist->end();)
		{
			user = *iter;
			if (user == NULL)
				break;
			user->protolock->lock();
			user->Destroy();
			user->protolock->unlock();

			dellist->splice(dellist->end(), *userlist, iter++);
		}
		add_to_userDes(task, dellist);
		TimeSleep(1);
	}

	do_clean:
	if (task_sub_workThead(task) == 0)
		destroy_task(task);
	if (userlist)
		delete userlist;
	if (dellist)
		delete dellist;
	return 0;
}

static bool run_task_thread(HTASKCFG task)
{
	for (int i = 0; i < GetCpuCount()*2; i++)
	{
#ifdef __WINDOWS__
		HANDLE ThreadHandle;
		ThreadHandle = CreateThread(NULL, 1024*1024*16, taskWorkerThread, task, 0, NULL);
		if (NULL == ThreadHandle) {
			return false;
		}
		CloseHandle(ThreadHandle);
#else
		pthread_attr_t attr;
   		pthread_t tid;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 1024*1024*16);
		int rc;

		if((rc = pthread_create(&tid, &attr, (void*(*)(void*))taskWorkerThread, task)) != 0)
		{
			return false;
		}
#endif // __WINDOWS__
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
		snprintf(path, sizeof(path), "%stask%03d.log", LogPath, taskID);
		task->logfd = RegisterLog(path, LOG_TRACE, 20, 86400, 2);
		task->messageList = new(std::nothrow) std::vector<t_cache_message*>;
		task->userAll = new(std::nothrow) std::list<ReplayProtocol*>;
		task->userAllLock = new(std::nothrow) std::mutex;
		task->userDes = new(std::nothrow) std::list<ReplayProtocol*>;
		task->userDesLock = new(std::nothrow) std::mutex;
		if (task->messageList == NULL || task->userAll == NULL || task->userAllLock == NULL ||
			task->userDes == NULL || task->userDesLock == NULL)
		{
			LOG(clogId, LOG_ERROR, "%s:%d malloc error\r\n", __func__, __LINE__);
			if (task->messageList) delete task->messageList;
			if (task->userAll) delete task->userAll;
			if (task->userAllLock) delete task->userAllLock;
			if (task->userDes) delete task->userDes;
			if (task->userDesLock) delete task->userDesLock;
			sheeps_free(task);
			return NULL;
		}
		taskAll.insert(std::pair<int, HTASKCFG>(taskID, task));
		return task;
	}
}

static int task_add_user(HTASKCFG task, int userCount, BaseFactory* factory)
{
	if (userCount <= 0)
	{
		task->userCount += userCount;
		if (task->userCount < 0)
			task->userCount = 0;
		return 0;
	}

	std::list<ReplayProtocol*>* userlist = new(std::nothrow) std::list<ReplayProtocol*>;
	if (userlist == NULL)
	{
		LOG(clogId, LOG_ERROR, "%s:%d malloc error\r\n", __func__, __LINE__);
		return 0;
	}
	ReplayProtocol* user = NULL;
	bool isNew = false;
	for (int i = 0; i < userCount; i++)
	{
		isNew = false;
		user = get_userDes_splice(task, userlist);
		if (user == NULL && default_api.create)
		{
			isNew = true;
			user = default_api.create();
			if (user == NULL)
			{
				LOG(clogId, LOG_ERROR, "%s:%d create user error\r\n", __func__, __LINE__);
				break;
			}
			user->SetFactory(factory, CLIENT_PROTOCOL);
			user->Task = task;
			user->UserNumber = task->userNumber++;
			userlist->push_back(user);
		}
		if (user)
		{
			if (isNew)
				user->Init();
			else
				ReInit(user, true);
		}
	}
	task->userCount += (uint16_t)userlist->size();
	add_to_userAll(task, userlist, -1);
	delete userlist;
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
	task->machineID = machineid;
	task->ignoreErr = ignorerr;
	
	if (default_api.taskstart)
		default_api.taskstart(task);

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

bool TaskUserSocketClose(HSOCKET &hsock)
{
#ifdef __WINDOWS__
	if (hsock == NULL) return false;
	SOCKET fd = InterlockedExchange(&hsock->fd, INVALID_SOCKET);
	if (fd != INVALID_SOCKET && fd != NULL)
	{
		InterlockedDecrement(&hsock->_user->sockCount);
		CancelIo((HANDLE)fd);	//取消等待执行的异步操作
		closesocket(fd);
	}
	hsock = NULL;
#else
	if (hsock == NULL || hsock->_is_close == 1)
		return false;
	hsock->_is_close = 1;
	__sync_sub_and_fetch(&hsock->_user->sockCount, 1);
	shutdown(hsock->fd, SHUT_RD);
	hsock = NULL;
#endif // __WINDOWS__
	return true;
}

bool TaskUserDead(ReplayProtocol* proto, const char* fmt, ...)
{
	proto->SelfDead = true;
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(proto->LastError, sizeof(proto->LastError) - 1, fmt, ap);
	va_end(ap);
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
#ifdef __WINDOWS__
	localtime_s(&tmm, &now);
#else
	localtime_r(&now, &tmm);
#endif // __WINDOWS__
	char* slog = GetLogStr(level);
	l = snprintf(buf, MAX_LOG_LEN - 1, "[%s:%ld]:[%04d-%02d-%02d %02d:%02d:%02d]", slog, (long)THREAD_ID, tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);
	l += snprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, "\r\n");

#ifdef __WINDOWS__
	DWORD written;
	WriteFile(GetLogFileHandle(logfd), buf, l, &written, NULL);
#else
	write(GetLogFileHandle(logfd), buf, l);
#endif // __WINDOWS__
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
#ifdef __WINDOWS__
	localtime_s(&tmm, &now);
#else
	localtime_r(&now, &tmm);
#endif // __WINDOWS__
	char* slog = GetLogStr(level);
	l = snprintf(buf, MAX_LOG_LEN - 1, "[%s:%ld]:[NO.%d]:[%04d-%02d-%02d %02d:%02d:%02d]", slog, (long)THREAD_ID, proto->UserNumber, tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);

	va_list ap;
	va_start(ap, fmt);
	l += vsnprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, fmt, ap);
	va_end(ap);
	l += snprintf(buf + l, (size_t)MAX_LOG_LEN - l - 1, "\r\n");

#ifdef __WINDOWS__
	DWORD written;
	WriteFile(GetLogFileHandle(logfd), buf, l, &written, NULL);
#else
	write(GetLogFileHandle(logfd), buf, l);
#endif // __WINDOWS__
}

static void TaskManagerForever(int projectid) 
{
#ifdef __WINDOWS__
	system("chcp 65001");
#define clear_screen "CLS"
#else
#define clear_screen "clear"
#endif
	system(clear_screen);
	printf("正在启动...");
	TimeSleep(1000);
	printf("...");
	TimeSleep(1000);
	printf("...");
	char in[4] = { 0x0 };
	while (true)
	{
		system(clear_screen);
		printf("\nSheeps负载端正在运行，项目ID[%d]:\n\n", projectid);
		if (TaskManagerRuning)
			printf("状态：连接成功\n");
		else
			printf("状态：未连接\n");
		printf("\n选择:【Q退出】\n操作：");
		fgets(in, 3, stdin);
		if (in[0] == 'Q')
			break;
	}
}

void TaskManagerRun(int projectid, CREATEAPI create, DESTORYAPI destory, INIT taskstart, INIT taskstop)
{
	config_init(ConfigFile);
	default_api.create = create;
	default_api.destory = destory;
	default_api.taskstart = taskstart;
	default_api.taskstop = taskstop;

	const char* ip = config_get_string_value("agent", "srv_ip", "127.0.0.1");
	int port = config_get_int_value("agent", "srv_port", 1080);
	SheepsClientRun(ip, port, projectid);
	TaskManagerForever(projectid);
}