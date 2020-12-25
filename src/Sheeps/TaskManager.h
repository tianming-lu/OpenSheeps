#ifndef _TASK_MANAGER_H_
#define _TASK_MANAGER_H_
#include <vector>
#include <list>
#include <map>
#include <mutex>
#include "Reactor.h"

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#if defined STRESS_EXPORTS
#define Task_API __declspec(dllexport)
#else
#define Task_API __declspec(dllimport)
#endif
#else
#define Task_API
#endif // __WINDOWS__

extern int clogId;
extern bool TaskManagerRuning;
#ifndef _LOG_H_
enum loglevel { LOG_TRACE = 0, LOG_DEBUG, LOG_NORMAL, LOG_ERROR, LOG_FAULT, LOG_NONE };
#endif

enum {
	//发送录制协议时返回确认发送数据包或者丢弃数据包，默认是DOSEND
	DOSEND = 0x01,
	GIVEUP = 0x02,
	//复合动作,延迟消息并暂停
	DELAY = 0x04,
	//确定用户播放状态
	NOTHING = 0x10,
	NORMAL = 0x20,
	PAUSE = 0x40,
	FAST = 0x80
};

enum{
	TYPE_CONNECT = 0,
	TYPE_SEND,
	TYPE_CLOSE,
	TYPE_REINIT = 99
};

enum {
	PLAY_NORMAL = 0,
	PLAY_PAUSE,
	PLAY_FAST
};

enum {
	TASK_RUNNING = 0,
	TASK_CLEANING,
};

typedef struct {
	uint32_t	index;
	uint64_t	start_record;		//微秒
	uint64_t	start_real;			//微秒
	uint64_t	last;
}MSGPointer, *HMSGPOINTER;

class ReplayProtocol;

typedef struct {
	uint8_t		type;
	char		ip[16];
	union {
		uint16_t port;
		uint16_t isloop;
	};
	uint64_t	recordtime;		//微秒
	char*		content;
	uint32_t	contentlen;
	bool		udp;
}t_cache_message, *HMESSAGE;

#ifdef __WINDOWS__
typedef struct _UserEvent {
	HANDLE			timer;
	ReplayProtocol* user;
	//HTASKCFG		task;
}UserEvent, * HUserEvent;
#else
typedef struct _UserEvent {
	time_t			timer;
	ReplayProtocol* user;
}UserEvent, * HUserEvent;
#endif // __WINDOWS__

typedef struct {
	int			logfd;
	uint8_t		status;    //默认TASK_RUNNING

	uint8_t		taskID;
	uint8_t		projectID;
	uint8_t		machineID;
	uint16_t	userCount;
	bool		ignoreErr;

	std::vector<t_cache_message*>*	messageList;    //任务消息缓存
	bool							stopMessageCache;
#ifdef __WINDOWS__
	HANDLE hTimerQueue;
#else
	int hTimerQueue;
#endif

	uint16_t	userNumber;
	std::list<HUserEvent>* userAll;
	std::list<HUserEvent>*	userDes;			//运行结束用户列表
	std::mutex*			userDesLock;

}t_task_config, *HTASKCFG;

typedef ReplayProtocol* (*CREATEAPI)();
typedef void(*DESTORYAPI)(ReplayProtocol*);
typedef int(*INIT)(HTASKCFG);
typedef int(*UNPACK)(const char*, int, int, const char*, int);
typedef struct dllAPI
{
	CREATEAPI   create;
	DESTORYAPI  destory;
	INIT		taskstart;
	INIT		taskstop;
	INIT		unpack;
}t_replay_dll;

class ReplayProtocol :
	public BaseProtocol
{
public:
	ReplayProtocol() {};
	virtual ~ReplayProtocol() {};

public:
	HTASKCFG	Task = NULL;
	int			UserNumber = 0;
	bool		SelfDead = false;
	uint8_t		PlayState = PLAY_NORMAL;
	MSGPointer	MsgPointer = { 0x0 };
	char		LastError[128] = {0x0};

public:
	virtual void Init() = 0;
	virtual void ConnectionMade(HSOCKET hsock) = 0;
	virtual void ConnectionFailed(HSOCKET hsock) = 0;
	virtual void ConnectionClosed(HSOCKET hsock) = 0;
	virtual void Recved(HSOCKET hsock, const char* data, int len) = 0;
	virtual void Event(uint8_t event_type, const char* ip, int port, const char* content, int clen, bool udp) = 0;
	virtual void TimeOut() = 0;
	virtual void ReInit() = 0;
	virtual void Destroy() = 0;
};

//受控端逻辑API

#ifdef __cplusplus
extern "C"
{
#endif

int		__stdcall	create_new_task(uint8_t taskid, uint8_t projectid, uint8_t machineid, bool ignorerr, int userconut, BaseFactory* factory);
bool	__stdcall	insert_message_by_taskId(uint8_t taskID, uint8_t type, char* ip, uint32_t port, char* content, uint64_t timestamp, uint32_t microsecond, bool udp);
bool	__stdcall	stop_task_by_id(uint8_t taskID);
int		__stdcall	task_add_user_by_taskid(uint8_t taskid, int userCount, BaseFactory* factory);
void	__stdcall	set_task_log_level(uint8_t level, uint8_t taskID);

//项目业务逻辑API
Task_API void	__stdcall	TaskManagerRun(int projectid, CREATEAPI create, DESTORYAPI destory, INIT taskstart, INIT taskstop, bool server);
Task_API bool	__cdecl		TaskUserDead(ReplayProtocol* proto, const char* fmt, ...);
Task_API bool	__stdcall	TaskUserSocketClose(HSOCKET hsock);
Task_API void	__cdecl		TaskUserLog(ReplayProtocol* proto, uint8_t level, const char* fmt, ...);
Task_API void	__cdecl		TaskLog(HTASKCFG task, uint8_t level, const char* fmt, ...);
#define		TaskUserSocketConnet(proto, ip, port, iotype)	HsocketConnect(proto, ip, port, iotype)
#define		TaskUserSocketSend(hsock, data, len)			HsocketSend(hsock, data, len)
#define		TaskUserSocketSkipBuf(hsock, len)				HsocketSkipBuf(hsock, len)

#ifdef __cplusplus
}
#endif

#endif // !_TASK_MANAGER_H_