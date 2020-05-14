#pragma once
#include <vector>
#include <string>
#include <list>
#include <queue>
#include <map>
#include <mutex>
#include "IOCPReactor.h"
#include "log.h"

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define Task_API __declspec(dllexport)
#else
#define Task_API __declspec(dllimport)
#endif // COMMON_LIB

extern int clogId;

enum{
	TYPE_CONNECT = 0,
	TYPE_SEND,
	TYPE_CLOSE,
	TYPE_REINIT = 99
};

typedef struct {
	uint32_t	index;
	uint64_t	start_record;		//微秒
	uint64_t	start_real;			//微秒
	uint64_t	last;
}MsgPointer, *HMsgPointer;

class ReplayProtocol;

typedef struct {
	uint8_t		type;
	std::string		ip;
	union {
		uint32_t port;
		uint32_t isloop;
	};
	uint64_t	recordtime;		//微秒
	std::string		content;
}t_message_data, *HMESSAGE;

typedef ReplayProtocol* (*CREATEAPI)();
typedef void(*DESTORYAPI)(ReplayProtocol*);
typedef int(*INIT)(void*);
typedef struct dllAPI
{
	void*		dllHandle;
	CREATEAPI   create;
	DESTORYAPI  destory;
	INIT		init;
	INIT		uninit;
	INIT		threadinit;
	INIT		threaduninit;
}t_replay_dll;

typedef struct
{
	uint8_t		taskID;
	uint8_t		taskErrId;
	uint32_t	timestamp;
	char		errMsg[64];

}t_task_error;

typedef struct
{
	std::mutex*			protolock;
	ReplayProtocol*	proto;
}t_handle_user;

typedef struct {
	HLOG_Context hlog;
	uint8_t		status;    //0 未开始 1初始化中 2初始化完成 3任务运行中 4任务中止清理资源过程中

	uint8_t		taskID;
	uint8_t		projectID;
	uint8_t		envID;
	uint8_t		machineID;
	uint16_t	userCount;
	bool		ignoreErr;

	uint8_t		workThreadCount;  //任务工作线程，所有线程退出后开始销毁任务
	std::mutex*		workThereaLock;

	std::vector<t_message_data*> *messageList;    //任务消息缓存
	bool		stopMessageCache;

	//list<t_handle_user*>  *userPointer;
	std::list<t_handle_user*> *userAll;			//运行任务中用户列表
	std::mutex*		userAllLock;
	std::list<t_handle_user*> *userDes;			//运行结束用户列表
	std::mutex*		userDesLock;

	t_replay_dll dll;

	std::list<t_task_error*> *taskErr;
	std::mutex*		taskErrlock;
}t_task_config, *HTASKCFG;

class ReplayProtocol :
	public BaseProtocol
{
public:
	ReplayProtocol() {};
	virtual ~ReplayProtocol() {};

public:
	HTASKCFG	Task = NULL;
	uint16_t	UserNumber = 0;
	bool		SelfDead = false;
	bool		PlayPause = false;
	MsgPointer	MsgPointer = { 0x0 };

public:
	virtual void ProtoInit() = 0;
	virtual bool ConnectionMade(HSOCKET hsock, const char* ip, int port) = 0;
	virtual bool ConnectionFailed(HSOCKET, const char* ip, int port) = 0;
	virtual bool ConnectionClosed(HSOCKET hsock, const char* ip, int port) = 0;
	virtual int	 Recv(HSOCKET hsock, const char* ip, int port, const char* data, int len) = 0;
	virtual int  TimeOut() = 0;
	virtual int	 Event(uint8_t event_type, const char* ip, int port, const char* content, int clen) = 0;
	virtual int	 ReInit() = 0;
	virtual int  Destroy() = 0;
};

//受控端逻辑API

#ifdef __cplusplus
extern "C"
{
#endif

bool			init_task_log(char* configfile);
void			set_task_log_level(uint8_t level);
t_task_config*	getTask_by_taskId(uint8_t taskID);
bool			stopTask_by_taskId(uint8_t taskID);
void			destroyTask();
bool			changTask_work_thread(t_task_config* task, uint8_t count);
bool			insertMessage_by_taskId(uint8_t taskID, uint8_t type, char* ip, uint32_t port, char* content, uint64_t timestamp, uint32_t microsecond);
bool			dll_init(t_task_config* task, char* rootpath);

t_handle_user*	get_userAll_front(t_task_config* task, size_t* size);
bool			add_to_userAll_tail(t_task_config* task, t_handle_user* user);
t_handle_user*	get_userDes_front(t_task_config* task);
bool			add_to_userDes_tail(t_task_config* task, t_handle_user* user);

t_task_error*	get_task_error_front();
void			delete_task_error(t_task_error* error);
void			update_user_time_clock(ReplayProtocol* proto);
HMESSAGE		task_get_next_message(ReplayProtocol* proto);

//项目业务逻辑API
Task_API bool		TaskUserDead(ReplayProtocol* proto, const char* fmt, ...);
Task_API bool		TaskUserSocketClose(HSOCKET hsock);
Task_API void		TaskUserLog(ReplayProtocol* proto, uint8_t level, const char* fmt, ...);
Task_API void		TaskLog(t_task_config* task, uint8_t level, const char* fmt, ...);
#define		TaskUserSocketConnet(ip, port, proto, iotype)	IOCPConnectEx(ip, port, proto, iotype)
#define		TaskUserSocketSend(hsock, data, len)	IOCPPostSendEx(hsock, data, len)

#ifdef __cplusplus
}
#endif