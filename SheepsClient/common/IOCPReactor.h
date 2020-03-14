#pragma once
#include "framework.h"
#include "windows.h"
#include <Winsock2.h>
#include <mswsock.h>    //微软扩展的类库
#include "stdio.h"
#include <map>
#include <mutex>

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define Iocp_API __declspec(dllexport)
#else
#define Iocp_API __declspec(dllimport)
#endif // COMMON_LIB

#define DATA_BUFSIZE 20480
#define READ	0
#define WRITE	1
#define ACCEPT	2
#define CONNECT 3
#define WRITEEX 4

#define RECV		0
#define SEND		1
#define SEND_CLOSE	2
#define CLOSE		3

using namespace std;

class BaseFactory;
class BaseProtocol;

//扩展的输入输出结构
typedef struct _io_operation_data
{
	OVERLAPPED	overlapped;
	WSABUF		databuf;
	CHAR		recv_buffer[DATA_BUFSIZE];
	CHAR*		send_buffer;
	DWORD		buffer_len;
	BYTE		type;
	SOCKET		sock;
	bool		close;
}fd_operation_data;

//完成键
typedef struct _completion_key
{
	SOCKET				sock;
	char				IP[16];
	int					PORT;
	BaseFactory*		factory;
	mutex*				userlock;
	BaseProtocol**		user;
	fd_operation_data*	lpIOoperData;
	time_t				timeout;
}t_completion_fd, *HSOCKET;	//完成端口句柄

class Reactor 
{
public:
	Reactor() {};
	virtual ~Reactor() {};

public:
	HANDLE g_hComPort = NULL;
	bool   g_Run = TRUE;

	DWORD CPU_COUNT = 1;
	map<short, BaseFactory*> FactoryAll;
};

class BaseProtocol
{
public:
	BaseProtocol() {};
	virtual ~BaseProtocol() {
		delete this->protolock;
	};

public:
	BaseFactory* factory = NULL;
	BaseProtocol* self = NULL;
	mutex* protolock = new mutex;
	uint8_t sockCount = 0;

public:
	virtual bool ConnectionMade(HSOCKET hsock, char *ip, int port) = 0;
	virtual bool ConnectionFailed(HSOCKET, char *ip, int port) = 0;
	virtual bool ConnectionClosed(HSOCKET hsock, char *ip, int port) = 0;
	//virtual int  Send(HSOCKET hsock, char* ip, int port, char* data, int len) = 0;
	virtual int	 Recv(HSOCKET hsock, char* ip, int port, char* data, int len) = 0;
};

class BaseFactory
{
public:   //publice as private
	LPFN_ACCEPTEX lpfnAcceptEx = NULL;					 //AcceptEx函数指针
	LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;  //加载GetAcceptExSockaddrs函数指针

public:
	BaseFactory() {};
	virtual ~BaseFactory() {};

public:
	Reactor* reactor = NULL;
	int			ServerPort = 0;
	SOCKET		sListen = NULL;
	virtual bool	FactoryInit() = 0;
	virtual bool	FactoryLoop() = 0;
	virtual bool	FactoryClose() = 0;
	virtual bool	TcpConnect() = 0;
	virtual BaseProtocol* CreateProtocol() = 0;
	virtual bool	DeleteProtocol(BaseProtocol* proto) = 0;
};

#ifdef __cplusplus
extern "C"
{
#endif

Iocp_API int		IOCPServerStart(Reactor* reactor);
Iocp_API void		IOCPServerStop(Reactor* reactor);
Iocp_API int		IOCPFactoryRun(BaseFactory* fc);
Iocp_API int		IOCPFactoryStop(BaseFactory* fc);
//Iocp_API SOCKET		IOCPConnect(const char* ip, int port, BaseProtocol** proto);
Iocp_API HSOCKET	IOCPConnectEx(const char* ip, int port, BaseProtocol* proto);
Iocp_API bool		IOCPPostSendEx(t_completion_fd* pComKey, char* data, int len);
Iocp_API bool		IOCPCloseHsocket(t_completion_fd* pComKey);
Iocp_API bool		IOCPDestroyProto(BaseProtocol* proto);

#ifdef __cplusplus
}
#endif