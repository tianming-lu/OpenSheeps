#ifndef _TASK_MANAGER_H_
#define _TASK_MANAGER_H_
#include <vector>
#include <list>
#include <map>
#include <mutex>
#include "IOCPReactor.h"

#if defined STRESS_EXPORTS
#define Task_API __declspec(dllexport)
#else
#define Task_API __declspec(dllimport)
#endif

extern int clogId;
extern bool TaskManagerRuning;
#ifndef _LOG_H_
enum loglevel { LOG_TRACE = 0, LOG_DEBUG, LOG_NORMAL, LOG_ERROR, LOG_FAULT, LOG_NONE };
#endif

enum {
	//����¼��Э��ʱ����ȷ�Ϸ������ݰ����߶������ݰ���Ĭ����DOSEND
	DOSEND = 0x01,
	GIVEUP = 0x02,
	//���϶���,�ӳ���Ϣ����ͣ
	DELAY = 0x04,
	//ȷ���û�����״̬
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

typedef struct {
	uint32_t	index;
	uint64_t	start_record;		//΢��
	uint64_t	start_real;			//΢��
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
	uint64_t	recordtime;		//΢��
	char*		content;
	uint32_t	contentlen;
	bool		udp;
}t_cache_message, *HMESSAGE;

typedef struct
{
	uint8_t		taskID;
	uint32_t	userID;
	time_t		timeStamp;
	char		errMsg[64];
}t_task_error;

typedef struct
{
	std::mutex*		protolock;
	ReplayProtocol*	proto;
}t_handle_user, *HUSER;

typedef struct {
	int			logfd;
	uint8_t		status;    //0 δ��ʼ 1��ʼ���� 2��ʼ����� 3���������� 4������ֹ������Դ������

	uint8_t		taskID;
	uint8_t		projectID;
	uint8_t		envID;
	uint8_t		machineID;
	uint16_t	userCount;
	bool		ignoreErr;

	uint8_t			workThreadCount;  //�������̣߳������߳��˳���ʼ��������
	std::mutex*		workThereaLock;

	std::vector<t_cache_message*>*	messageList;    //������Ϣ����
	bool							stopMessageCache;

	std::list<HUSER>*	userAll;			//�����������û��б�
	std::mutex*			userAllLock;
	std::list<HUSER>*	userDes;			//���н����û��б�
	std::mutex*			userDesLock;

	std::list<t_task_error*>*	taskErr;
	std::mutex*					taskErrlock;
}t_task_config, *HTASKCFG;

typedef ReplayProtocol* (*CREATEAPI)();
typedef void(*DESTORYAPI)(ReplayProtocol*);
typedef int(*INIT)(HTASKCFG);
typedef int(*UNPACK)(const char*, int, int, const char*, int);
typedef struct dllAPI
{
	CREATEAPI   create;
	DESTORYAPI  destory;
	INIT		init;
	INIT		uninit;
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
	uint16_t	UserNumber = 0;
	bool		SelfDead = false;
	uint8_t		PlayState = PLAY_NORMAL;
	MSGPointer	MsgPointer = { 0x0 };

public:
	virtual void Init() = 0;
	virtual void ConnectionMade(HSOCKET hsock, const char* ip, int port) = 0;
	virtual void ConnectionFailed(HSOCKET, const char* ip, int port) = 0;
	virtual void ConnectionClosed(HSOCKET hsock, const char* ip, int port) = 0;
	virtual void Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len) = 0;
	virtual void Event(uint8_t event_type, const char* ip, int port, const char* content, int clen, bool udp) = 0;
	virtual void TimeOut() = 0;
	virtual void ReInit() = 0;
	virtual void Destroy() = 0;
};

//�ܿض��߼�API

#ifdef __cplusplus
extern "C"
{
#endif

int				create_new_task(uint8_t taskid, uint8_t projectid, uint8_t machineid, bool ignorerr, int userconut, BaseFactory* factory);
bool			insert_message_by_taskId(uint8_t taskID, uint8_t type, char* ip, uint32_t port, char* content, uint64_t timestamp, uint32_t microsecond, bool udp);
bool			stop_task_by_id(uint8_t taskID);
int				task_add_user_by_taskid(uint8_t taskid, int userCount, BaseFactory* factory);
void			set_task_log_level(uint8_t level, uint8_t taskID);

t_task_error*	get_task_error_front();
void			delete_task_error(t_task_error* error);


//��Ŀҵ���߼�API
Task_API void		TaskManagerRun(int projectid, CREATEAPI create, DESTORYAPI destory, INIT init, INIT uninit);
Task_API bool		TaskUserDead(ReplayProtocol* proto, const char* fmt, ...);
Task_API bool		TaskUserSocketClose(HSOCKET hsock);
Task_API void		TaskUserLog(ReplayProtocol* proto, uint8_t level, const char* fmt, ...);
Task_API void		TaskLog(HTASKCFG task, uint8_t level, const char* fmt, ...);
#define		TaskUserSocketConnet(ip, port, proto, iotype)	IOCPConnectEx(ip, port, proto, iotype)
#define		TaskUserSocketSend(hsock, data, len)			IOCPPostSendEx(hsock, data, len)
#define		TaskUserSocketSkipBuf(hsock, len)				IOCPSkipHsocketBuf(hsock, len)

#ifdef __cplusplus
}
#endif

#endif // !_TASK_MANAGER_H_