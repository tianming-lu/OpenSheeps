#pragma once
#include "framework.h"
#include "windows.h"
#include <Winsock2.h>
#include <mswsock.h>    //微软扩展的类库
#include "stdio.h"
#include <map>
#include <mutex>
#include <stack>

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

#define IOCP_TCP 0
#define IOCP_UDP 1

using namespace std;

class BaseFactory;
class BaseProtocol;

//扩展的输入输出结构
typedef struct _IOCP_BUFF
{
	OVERLAPPED	overlapped;
	WSABUF		databuf;
	CHAR		recv_buffer[DATA_BUFSIZE];
	CHAR*		send_buffer;
	DWORD		buffer_len;
	BYTE		type;
	SOCKET		sock;
	bool		close;
}IOCP_BUFF;

//完成键
typedef struct _IOCP_SOCKET
{
	SOCKET				sock;
	uint8_t				iotype;
	CHAR				IP[16];
	int32_t				PORT;
	SOCKADDR_IN			SAddr;
	BaseFactory*		factory;
	mutex*				userlock;
	BaseProtocol**		user;
	IOCP_BUFF*			IocpBuff;
	time_t				timeout;
}IOCP_SOCKET, *HSOCKET;	//完成端口句柄

class Reactor 
{
public:
	Reactor() {};
	virtual ~Reactor();

public:
	HANDLE	ComPort = NULL;
	bool	Run = true;
	DWORD	CPU_COUNT = 1;

	LPFN_ACCEPTEX				lpfnAcceptEx = NULL;					 //AcceptEx函数指针
	LPFN_GETACCEPTEXSOCKADDRS	lpfnGetAcceptExSockaddrs = NULL;  //加载GetAcceptExSockaddrs函数指针

	map<short, BaseFactory*>	FactoryAll;
	stack<IOCP_SOCKET*>			HsocketPool;
	mutex						HsocketPoolLock;
	stack<IOCP_BUFF*>			HbuffPool;
	mutex						HbuffPoolLock;
};

class BaseProtocol
{
public:
	BaseProtocol() {};
	virtual ~BaseProtocol() {delete this->protolock;};

public:
	BaseFactory*	factory = NULL;
	BaseProtocol*	self = NULL;
	mutex*			protolock = new mutex;
	uint8_t			sockCount = 0;

public:
	virtual bool ConnectionMade(HSOCKET hsock, char *ip, int port) = 0;
	virtual bool ConnectionFailed(HSOCKET, char *ip, int port) = 0;
	virtual bool ConnectionClosed(HSOCKET hsock, char *ip, int port) = 0;
	virtual int	 Recv(HSOCKET hsock, char* ip, int port, char* data, int len) = 0;
};

class BaseFactory
{
public:
	BaseFactory() {};
	virtual ~BaseFactory() {};

public:
	Reactor*		reactor = NULL;
	uint32_t		ServerPort = 0;
	SOCKET			sListen = NULL;
	virtual bool	FactoryInit() = 0;
	virtual bool	FactoryLoop() = 0;
	virtual bool	FactoryClose() = 0;
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
Iocp_API HSOCKET	IOCPConnectEx(const char* ip, int port, BaseProtocol* proto, uint8_t iotype);
Iocp_API bool		IOCPPostSendEx(IOCP_SOCKET* IocpSock, char* data, int len);
Iocp_API bool		IOCPCloseHsocket(IOCP_SOCKET* IocpSock);
Iocp_API bool		IOCPDestroyProto(BaseProtocol* proto);

#ifdef __cplusplus
}
#endif