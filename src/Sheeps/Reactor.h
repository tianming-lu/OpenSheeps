#ifndef _REACTOR_H_
#define _REACTOR_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#define API_EXPORTS

#ifdef __WINDOWS__
#define __STDCALL __stdcall
#define __CDECL__	__cdecl
#if defined API_EXPORTS
#define Reactor_API __declspec(dllexport)
#else
#define Reactor_API __declspec(dllimport)
#endif
#else
#define __STDCALL
#define __CDECL__
#define Reactor_API
#endif // __WINDOWS__

#include <map>
#ifdef __WINDOWS__
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include <Winsock2.h>
#include <mswsock.h>    //微软扩展的类库
#pragma warning(disable:26812)
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <mutex>
#endif // __WINDOWS__

enum CONN_TYPE {
	TCP_CONN = 0,
	UDP_CONN = 1,
	ITMER = 2
};

enum PROTOCOL_TPYE {
	CLIENT_PROTOCOL = 0,
	SERVER_PROTOCOL = 1
};

class BaseFactory;
class BaseProtocol;

typedef void (*timer_callback) (BaseProtocol*); 

#ifdef __WINDOWS__
struct _IOCP_SOCKET;
#else
struct _EPOLL_SOCKET;
#endif

#ifdef __WINDOWS__
typedef struct _IOCP_BUFF
{
	OVERLAPPED	overlapped;
	WSABUF		databuf;
	int32_t		offset;
	int32_t		size;
	BYTE		type;
	SOCKET		fd;
	DWORD		flags;
	struct _IOCP_SOCKET* hsock;
}IOCP_BUFF, *HNETBUFF;
#else
typedef struct _EPOLL_BUFF
{
	char* buff;
	size_t offset;
	size_t size;
	uint8_t lock_flag;
}EPOLL_BUFF, * HNETBUFF;
#endif

#ifdef __WINDOWS__
typedef struct _IOCP_SOCKET
{
	SOCKET			fd;
#else
typedef struct _EPOLL_SOCKET
{
	int				fd;
#endif
	char*			recv_buf;
	char			peer_ip[16];
	uint16_t		peer_port;
	struct sockaddr_in		peer_addr;
	time_t			heartbeat;
	BaseFactory*	factory;

#ifdef __WINDOWS__
	uint8_t			_iotype;
	BaseProtocol*	_user;
	IOCP_BUFF*		_IocpBuff;
}IOCP_SOCKET, * HSOCKET;
#else
	uint8_t			_is_close;
	CONN_TYPE		_conn_type;
	uint8_t			_epoll_type;
	BaseProtocol*	_user;
	EPOLL_BUFF		_recv_buf;
	char*			_sendbuff;
	EPOLL_BUFF		_send_buf;
	//timer
	timer_callback  _callback;
}EPOLL_SOCKET, * HSOCKET;
#endif // __WINDOWS__

class Reactor
{
public:
	Reactor() {};
	~Reactor() {};

public:
	bool	Run = true;
	int		CPU_COUNT = 1;
#ifdef __WINDOWS__
	HANDLE	ComPort = NULL;
	LPFN_ACCEPTEX				lpfnAcceptEx = NULL;					 //AcceptEx函数指针
#else
	int		epfd = 0;
	int 	maxevent = 1024;
#endif
	std::map<uint16_t, BaseFactory*>	FactoryAll;
};

class BaseProtocol
{
public:
	BaseProtocol() { 
		this->protoType = SERVER_PROTOCOL;
#ifdef __WINDOWS__
		this->mutex = CreateMutexA(NULL, false, NULL);
#else
		this->mutex = new(std::nothrow) std::mutex();
#endif
	};
	virtual ~BaseProtocol() {
		if (this->mutex)
#ifdef __WINDOWS__
			CloseHandle(this->mutex);
#else
			delete this->mutex;
#endif
	};
	void SetFactory(BaseFactory* pfc, PROTOCOL_TPYE prototype) { this->factory = pfc; this->protoType = prototype; };
	void SetNoLock() {
#ifdef __WINDOWS__
		CloseHandle(this->mutex); this->mutex = NULL;
#else
		if (this->mutex) delete this->mutex; this->mutex = NULL;
#endif
	}
	void Lock() { 
		if(this->mutex)
#ifdef __WINDOWS__
			WaitForSingleObject(this->mutex, INFINITE);
#else
			this->mutex->lock();
#endif
	};
	void UnLock() {
		if (this->mutex)
#ifdef __WINDOWS__
			ReleaseMutex(this->mutex);
#else
			this->mutex->unlock();
#endif
	};

public:
	BaseFactory*	factory = NULL;
#ifdef __WINDOWS__
	HANDLE			mutex = NULL;
#else
	std::mutex*		mutex = NULL;
#endif // __WINDOWS__

	//std::mutex*		protolock = NULL;
	PROTOCOL_TPYE	protoType = SERVER_PROTOCOL;
	long			sockCount = 0;

public:
	virtual void ConnectionMade(HSOCKET hsock) = 0;
	virtual void ConnectionFailed(HSOCKET hsock) = 0;
	virtual void ConnectionClosed(HSOCKET hsock) = 0;
	virtual void ConnectionRecved(HSOCKET hsock, const char* data, int len) = 0;
};

class BaseFactory
{
public:
	BaseFactory() {};
	virtual ~BaseFactory() {};
	void Set(Reactor* rec, uint16_t listenport) { this->reactor = rec; this->ServerPort = listenport; };

public:
	Reactor* reactor = NULL;
	uint16_t	ServerPort = 0;
#ifdef __WINDOWS__
	SOCKET		Listenfd = NULL;
#else
	int			Listenfd = 0;
#endif // __WINDOWS__
	virtual bool	FactoryInit() = 0;
	virtual void	FactoryLoop() = 0;
	virtual void	FactoryClose() = 0;
	virtual BaseProtocol* CreateProtocol() = 0;
	virtual void	DeleteProtocol(BaseProtocol* proto) = 0;
};

#ifdef __cplusplus
extern "C"
{
#endif

	Reactor_API int		__STDCALL	ReactorStart(Reactor* reactor);
	Reactor_API void	__STDCALL	ReactorStop(Reactor* reactor);
	Reactor_API int		__STDCALL	FactoryRun(BaseFactory* fc);
	Reactor_API int		__STDCALL	FactoryStop(BaseFactory* fc);
	Reactor_API HSOCKET	__STDCALL	HsocketConnect(BaseProtocol* proto, const char* ip, int port, CONN_TYPE iotype);
	Reactor_API bool	__STDCALL	HsocketSend(HSOCKET hsock, const char* data, int len);
	Reactor_API bool	__STDCALL	HsocketClose(HSOCKET hsock);
	Reactor_API int		__STDCALL	HsocketSkipBuf(HSOCKET hsock, int len);

	Reactor_API	HNETBUFF	__STDCALL	HsocketGetBuff();
	Reactor_API	bool	__STDCALL	HsocketSetBuff(HNETBUFF netbuff, const char* data, int len);
	Reactor_API	bool	__STDCALL	HsocketSendBuff(HSOCKET IocpSock, HNETBUFF netbuff);
#ifndef __WINDOWS__
	Reactor_API	HSOCKET	__STDCALL	TimerCreate(BaseProtocol* proto, int duetime, int looptime, timer_callback callback);
	Reactor_API void 	__STDCALL	TimerDelete(HSOCKET hsock);
#endif

#ifdef __cplusplus
}
#endif

#endif // !_REACTOR_H_
