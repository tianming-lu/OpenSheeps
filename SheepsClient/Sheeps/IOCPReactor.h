#ifndef _IOCP_REACTOR_H_
#define _IOCP_REACTOR_H_
#define WIN32_LEAN_AND_MEAN   
#include "windows.h"
#include <Winsock2.h>
#include <mswsock.h>    //微软扩展的类库
#include "stdio.h"
#include <map>
#include <mutex>

#if defined STRESS_EXPORTS
#define Iocp_API __declspec(dllexport)
#else
#define Iocp_API __declspec(dllimport)
#endif

#pragma warning(disable:26812)

enum CONN_TYPE {
	TCP_CONN = 0,
	UDP_CONN = 1
};

enum PROTOCOL_TPYE{
	CLIENT_PROTOCOL = 0,
	SERVER_PROTOCOL = 1
};

class BaseFactory;
class BaseProtocol;

//扩展的输入输出结构
typedef struct _IOCP_BUFF
{
	OVERLAPPED	overlapped;
	WSABUF		databuf;
	uint32_t	offset;
	uint32_t	size;
	BYTE		type;
	SOCKET		fd;
}IOCP_BUFF;

//完成键
typedef struct _IOCP_SOCKET
{
	SOCKET				fd;
	char*				recv_buf;
	CHAR				peer_ip[16];
	uint16_t			peer_port;
	SOCKADDR_IN			peer_addr;
	time_t				heartbeat;
	BaseFactory*		factory;

	uint8_t				_iotype;
	BaseProtocol*		_user;
	IOCP_BUFF*			_IocpBuff;
}IOCP_SOCKET, *HSOCKET;	//完成端口句柄

class Reactor 
{
public:
	Reactor() {};
	~Reactor() {};

public:
	HANDLE	ComPort = NULL;
	bool	Run = true;
	DWORD	CPU_COUNT = 1;

	LPFN_ACCEPTEX				lpfnAcceptEx = NULL;					 //AcceptEx函数指针
	std::map<uint16_t, BaseFactory*>	FactoryAll;
};

class BaseProtocol
{
public:
	BaseProtocol() {this->_protolock = new std::mutex; this->_protoType = SERVER_PROTOCOL; };
	virtual ~BaseProtocol() {delete this->_protolock;};
	void SetFactory(BaseFactory* pfc, PROTOCOL_TPYE prototype) { this->_factory = pfc; this->_protoType = prototype; };

public:
	BaseFactory*	_factory = NULL;
	PROTOCOL_TPYE	_protoType = SERVER_PROTOCOL;
	std::mutex*		_protolock = NULL;
	uint8_t			_sockCount = 0;

public:
	virtual void ConnectionMade(HSOCKET hsock, const char *ip, int port) = 0;
	virtual void ConnectionFailed(HSOCKET, const char *ip, int port) = 0;
	virtual void ConnectionClosed(HSOCKET hsock, const char *ip, int port) = 0;
	virtual void Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len) = 0;
};

class BaseFactory
{
public:
	BaseFactory() {};
	virtual ~BaseFactory() {};
	void Set(Reactor* rec, uint16_t listenport) { this->reactor = rec; this->ServerPort = listenport; };

public:
	Reactor*		reactor = NULL;
	uint16_t		ServerPort = 0;
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
Iocp_API HSOCKET	IOCPConnectEx(BaseProtocol* proto, const char* ip, int port, CONN_TYPE iotype);
Iocp_API bool		IOCPPostSendEx(IOCP_SOCKET* IocpSock, const char* data, int len);
Iocp_API bool		IOCPCloseHsocket(IOCP_SOCKET* IocpSock);
Iocp_API int		IOCPSkipHsocketBuf(IOCP_SOCKET* IocpSock, int len);

#ifdef __cplusplus
}
#endif

#endif // !_IOCP_REACTOR_H_