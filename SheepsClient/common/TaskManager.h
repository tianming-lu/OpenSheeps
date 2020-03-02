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

using namespace std;

extern int clogId;

enum{
	TYPE_CONNECT = 0,
	TYPE_SEND,
	TYPE_CLOSE,
	TYPE_REINIT = 99
};

typedef struct {
	int	   index;
	UINT64 start_record;
	UINT64 start_real;
}MsgPointer, *HMsgPointer;

class UserProtocol;

typedef struct {
	short	type;
	string ip;
	union {
		int port;
		int isloop;
	};
	//int port;
	UINT64 recordtime;
	string	content;
}t_message_data, *HMESSAGE;

typedef UserProtocol* (*CREATEAPI)();
typedef void(*DESTORYAPI)(UserProtocol*);
typedef int(*INIT)(void*);
typedef struct dllAPI
{
	void* dllHandle;
	CREATEAPI   create;
	DESTORYAPI  destory;
	INIT		init;
	INIT		uninit;
}t_replay_dll;

typedef struct
{
	int taskID;
	int taskErrId;
	char errMsg[64];
	int timestamp;

}t_task_error;

typedef struct
{
	mutex* protolock;
	UserProtocol* proto;
}t_handle_user;

typedef struct {
	HLOG_Context hlog;
	int status;    //0 δ��ʼ 1��ʼ���� 2��ʼ����� 3���������� 4������ֹ������Դ������

	int taskID;
	int projectID;
	int envID;
	int userCount;
	int machineID;
	bool ignoreErr;

	short workThreadCount;  //�������̣߳������߳��˳���ʼ��������
	mutex *workThereaLock;

	vector<t_message_data*> *messageList;    //������Ϣ����
	bool stopMessageCache;

	//list<t_handle_user*>  *userPointer;
	list<t_handle_user*> *userAll;			//�����������û��б�
	mutex *userAllLock;
	list<t_handle_user*> *userDes;			//���н����û��б�
	mutex* userDesLock;

	t_replay_dll dll;

	list<t_task_error*> *taskErr;
	mutex* taskErrlock;
}t_task_config, *HTASKCFG;

class UserProtocol :
	public BaseProtocol
{
public:
	UserProtocol() {};
	virtual ~UserProtocol() {};

public:
	HTASKCFG	Task = NULL;
	uint32_t	UserNumber = 0;
	bool		SelfDead = FALSE;
	MsgPointer	MsgPointer = { 0x0 };

public:
	virtual void ProtoInit(void* parm, int index) = 0;
	virtual bool ConnectionMade(HSOCKET hsock, char* ip, int port) = 0;
	virtual bool ConnectionFailed(HSOCKET, char* ip, int port) = 0;
	virtual bool ConnectionClosed(HSOCKET hsock, char* ip, int port) = 0;
	virtual int  Send(HSOCKET hsock, char* ip, int port, char* data, int len) = 0;
	virtual int	 Recv(HSOCKET hsock, char* ip, int port, char* data, int len) = 0;
	virtual int	 Loop() = 0;
	virtual int	 ReInit() = 0;
	virtual int  Destroy() = 0;
};

//�ܿض��߼�API

#ifdef __cplusplus
extern "C"
{
#endif

bool init_task_log(char* configfile);
t_task_config* getTask_by_taskId(int taskID);
bool stopTask_by_taskId(int taskID);
void destroyTask();
bool changTask_work_thread(t_task_config* task, short count);
bool insertMessage_by_taskId(int taskID, short type, char* ip, int port, char* content, UINT64 timestamp, int microsecond);
bool dll_init(t_task_config* task, char* rootpath);

t_handle_user* get_userAll_front(t_task_config* task);
bool add_to_userAll_tail(t_task_config* task, t_handle_user* user);
t_handle_user* get_userDes_front(t_task_config* task);
bool add_to_userDes_tail(t_task_config* task, t_handle_user* user);

t_task_error* get_task_error_front();
void delete_task_error(t_task_error* error);

//��Ŀҵ���߼�API
Task_API HMESSAGE	TaskGetNextMessage(UserProtocol* proto);
Task_API bool		TaskUserDead(UserProtocol* proto, const char* fmt, ...);
Task_API bool		TaskUserSocketClose(HSOCKET hsock);
Task_API void		TaskUserLog(UserProtocol* proto, int level, const char* fmt, ...);
Task_API void		TaskLog(t_task_config* task, int level, const char* fmt, ...);
#define		TaskUserSocketConnet(ip, port, proto)	IOCPConnectEx(ip, port, proto)
#define		TaskUserSocketSend(hsock, data, len)	IOCPPostSendEx(hsock, data, len)

#ifdef __cplusplus
}
#endif