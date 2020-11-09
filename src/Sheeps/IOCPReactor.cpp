#include "pch.h"
#include "Reactor.h"
#include <Ws2tcpip.h>
#include <map>

#pragma comment(lib, "Ws2_32.lib")

#define DATA_BUFSIZE 5120
#define READ	0
#define WRITE	1
#define ACCEPT	2
#define CONNECT 3

static inline void* pst_malloc(size_t size)
{
	return GlobalAlloc(GPTR, size);
}

static inline void* pst_realloc(void* ptr, size_t size)
{
	return GlobalReAlloc(ptr, size, GMEM_MOVEABLE);
}

static inline void pst_free(void* ptr)
{
	GlobalFree(ptr);
}

static inline IOCP_SOCKET* NewIOCP_Socket()
{
	return (IOCP_SOCKET*)pst_malloc(sizeof(IOCP_SOCKET));
}

static inline void ReleaseIOCP_Socket(IOCP_SOCKET* IocpSock)
{
	pst_free(IocpSock);
}

static inline IOCP_BUFF* NewIOCP_Buff()
{
	return (IOCP_BUFF*)pst_malloc(sizeof(IOCP_BUFF));
}

static inline void ReleaseIOCP_Buff(IOCP_BUFF* IocpBuff)
{
	pst_free(IocpBuff);
}

static SOCKET GetListenSock(int port)
{
	SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN serAdd;
	serAdd.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serAdd.sin_family = AF_INET;
	serAdd.sin_port = htons(port);

	int ret = bind(listenSock, (SOCKADDR*)& serAdd, sizeof(SOCKADDR));
	if (ret != 0)
	{
		return SOCKET_ERROR;
	}
	listen(listenSock, 5);
	if (listenSock == SOCKET_ERROR)
	{
		return SOCKET_ERROR;
	}
	return listenSock;
}

static void PostAcceptClient(BaseFactory* fc)
{
	IOCP_BUFF* IocpBuff;
	IocpBuff = NewIOCP_Buff();
	if (IocpBuff == NULL)
	{
		return;
	}
	IocpBuff->databuf.buf = (char*)pst_malloc(DATA_BUFSIZE);
	if (IocpBuff->databuf.buf == NULL)
	{
		ReleaseIOCP_Buff(IocpBuff);
		return;
	}
	IocpBuff->databuf.len = DATA_BUFSIZE;
	IocpBuff->type = ACCEPT;

	IocpBuff->fd = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (IocpBuff->fd == INVALID_SOCKET)
	{
		ReleaseIOCP_Buff(IocpBuff);
		return;
	}

	/*调用AcceptEx函数，地址长度需要在原有的上面加上16个字节向服务线程投递一个接收连接的的请求*/
	bool rc = fc->reactor->lpfnAcceptEx(fc->Listenfd, IocpBuff->fd,
		IocpBuff->databuf.buf, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&IocpBuff->databuf.len, &(IocpBuff->overlapped));

	if (false == rc)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			ReleaseIOCP_Buff(IocpBuff);
			return;
		}
	}
	return;
}

static inline void CloseSocket(IOCP_SOCKET* IocpSock)
{
	SOCKET fd = InterlockedExchange(&IocpSock->fd, INVALID_SOCKET);
	if (fd != INVALID_SOCKET && fd != NULL)
	{
		CancelIo((HANDLE)fd);	//取消等待执行的异步操作
		closesocket(fd);
	}
}

static void Close(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff )
{
	switch (IocpBuff->type)
	{
	case ACCEPT:
		if (IocpBuff->databuf.buf)
			pst_free(IocpBuff->databuf.buf);
		ReleaseIOCP_Buff(IocpBuff);
		PostAcceptClient(IocpSock->factory);
		return;
	case WRITE:
		if (IocpBuff->databuf.buf != NULL)
			pst_free(IocpBuff->databuf.buf);
		ReleaseIOCP_Buff(IocpBuff);
		return;
	default:
		break;
	}
	BaseProtocol* proto = IocpSock->_user;
	int left_count = 99;
	if (IocpSock->fd != INVALID_SOCKET)
	{
		if (proto->protolock)
		{
			proto->protolock->lock();
			if (IocpSock->fd != INVALID_SOCKET)
			{
				left_count = InterlockedDecrement(&proto->sockCount);
				if (CONNECT == IocpBuff->type)
					proto->ConnectionFailed(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
				else
					proto->ConnectionClosed(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
			}
			CloseSocket(IocpSock);
			proto->protolock->unlock();
		}
		else
		{
			left_count = InterlockedDecrement(&proto->sockCount);
			if (CONNECT == IocpBuff->type)
				proto->ConnectionFailed(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
			else
				proto->ConnectionClosed(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
			CloseSocket(IocpSock);
		}
	}
	if (left_count == 0 && proto != NULL && proto->protoType == SERVER_PROTOCOL)
		IocpSock->factory->DeleteProtocol(proto);

	if (IocpSock->recv_buf)
		pst_free(IocpSock->recv_buf);
	ReleaseIOCP_Buff(IocpBuff);
	ReleaseIOCP_Socket(IocpSock);
	//MemoryBarrier();
}

static bool ResetIocp_Buff(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	memset(&IocpBuff->overlapped, 0, sizeof(OVERLAPPED));

	if (IocpSock->recv_buf == NULL)
	{
		if (IocpBuff->databuf.buf != NULL)
		{
			IocpSock->recv_buf = IocpBuff->databuf.buf;
		}
		else
		{
			IocpSock->recv_buf = (char*)pst_malloc(DATA_BUFSIZE);
			if (IocpSock->recv_buf == NULL)
				return false;
			IocpBuff->size = DATA_BUFSIZE;
		}
	}
	IocpBuff->databuf.len = IocpBuff->size - IocpBuff->offset;
	if (IocpBuff->databuf.len == 0)
	{
		IocpBuff->size += DATA_BUFSIZE;
		char* new_ptr = (char*)pst_realloc(IocpSock->recv_buf, IocpBuff->size);
		if (new_ptr == NULL)
			return false;
		IocpSock->recv_buf = new_ptr;
		IocpBuff->databuf.len = IocpBuff->size - IocpBuff->offset;
	}
	IocpBuff->databuf.buf = IocpSock->recv_buf + IocpBuff->offset;
	return true;
}

static inline void PostRecvUDP(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff, BaseProtocol* proto)
{
	int fromlen = sizeof(struct sockaddr);
	DWORD flags = 0;
	if (SOCKET_ERROR == WSARecvFrom(IocpSock->fd, &IocpBuff->databuf, 1, NULL, &flags, (sockaddr*)&IocpSock->peer_addr, &fromlen, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			return Close(IocpSock, IocpBuff);
		}
	}
}

static inline void PostRecvTCP(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff, BaseProtocol* proto)
{
	DWORD flags = 0;
	if (SOCKET_ERROR == WSARecv(IocpSock->fd, &IocpBuff->databuf, 1, NULL, &flags, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			return Close(IocpSock, IocpBuff);
		}
	}
}

static void PostRecv(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff, BaseProtocol* proto)
{
	IocpBuff->type = READ;
	if (ResetIocp_Buff(IocpSock, IocpBuff) == false)
	{
		return Close(IocpSock, IocpBuff);
	}

	if (IocpSock->_iotype == UDP_CONN)
		return PostRecvUDP(IocpSock, IocpBuff, proto);
	return PostRecvTCP(IocpSock, IocpBuff, proto);
}

static void AceeptClient(IOCP_SOCKET* IocpListenSock, IOCP_BUFF* IocpBuff)
{
	BaseFactory* fc = IocpListenSock->factory;
	Reactor* reactor = fc->reactor;
	setsockopt(IocpBuff->fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) & (IocpListenSock->fd), sizeof(IocpListenSock->fd));

	IOCP_SOCKET* IocpSock = NewIOCP_Socket();
	if (IocpSock == NULL)
	{
		return Close(IocpListenSock, IocpBuff);
	}
	IocpSock->fd = IocpBuff->fd;
	BaseProtocol* proto = fc->CreateProtocol();	//用户指针
	if (proto == NULL)
	{
		ReleaseIOCP_Socket(IocpSock);
		return Close(IocpListenSock, IocpBuff);
	}
	if (proto->factory == NULL)
		proto->SetFactory(fc, SERVER_PROTOCOL);
	IocpSock->factory = fc;
	IocpSock->_user = proto;	//用户指针
	IocpSock->_IocpBuff = IocpBuff;

	int nSize = sizeof(IocpSock->peer_addr);
	getpeername(IocpSock->fd, (SOCKADDR*)&IocpSock->peer_addr, &nSize);
	inet_ntop(AF_INET, &IocpSock->peer_addr.sin_addr, IocpSock->peer_ip, sizeof(IocpSock->peer_ip));
	IocpSock->peer_port = ntohs(IocpSock->peer_addr.sin_port);

	InterlockedIncrement(&proto->sockCount);
	CreateIoCompletionPort((HANDLE)IocpSock->fd, reactor->ComPort, (ULONG_PTR)IocpSock, 0);	//将监听到的套接字关联到完成端口
	if (proto->protolock)
	{
		proto->protolock->lock();
		proto->ConnectionMade(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
		proto->protolock->unlock();
	}
	else
	{
		proto->ConnectionMade(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
	}

	PostRecv(IocpSock, IocpBuff, proto);
	PostAcceptClient(IocpListenSock->factory);
}

static void ProcessIO(IOCP_SOCKET* &IocpSock, IOCP_BUFF* &IocpBuff)
{
	BaseProtocol* proto = NULL;
	switch (IocpBuff->type)
	{
	case READ:
		proto = IocpSock->_user;
		if (IocpSock->fd != INVALID_SOCKET)
		{
			if (IocpSock->_iotype == UDP_CONN && IocpSock->peer_port == 0)
			{
				inet_ntop(AF_INET, &IocpSock->peer_addr.sin_addr, IocpSock->peer_ip, sizeof(IocpSock->peer_ip));
				IocpSock->peer_port = ntohs(IocpSock->peer_addr.sin_port);
			}
			if (proto->protolock)
			{
				proto->protolock->lock();
				if (IocpSock->fd != INVALID_SOCKET)
				{
					proto->Recved(IocpSock, IocpSock->peer_ip, IocpSock->peer_port, IocpSock->recv_buf, IocpBuff->offset);
					proto->protolock->unlock();
				}
				else
				{
					proto->protolock->unlock();
					return Close(IocpSock, IocpBuff);
				}
			}
			else
			{
				proto->Recved(IocpSock, IocpSock->peer_ip, IocpSock->peer_port, IocpSock->recv_buf, IocpBuff->offset);
			}
			PostRecv(IocpSock, IocpBuff, proto);
		}
		else
		{
			return Close(IocpSock, IocpBuff);
		}
		break;
	case WRITE:
		return Close(IocpSock, IocpBuff);
		break;
	case ACCEPT:
		AceeptClient(IocpSock, IocpBuff);
		break;
	case CONNECT:
		proto = IocpSock->_user;
		if (IocpSock->fd != INVALID_SOCKET)
		{
			if (proto->protolock)
			{
				proto->protolock->lock();
				if (IocpSock->fd != INVALID_SOCKET)
				{
					proto->ConnectionMade(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
					proto->protolock->unlock();
				}
				else
				{
					proto->protolock->unlock();
					return Close(IocpSock, IocpBuff);
				}
			}
			else
			{
				proto->ConnectionMade(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
			}
			PostRecv(IocpSock, IocpBuff, proto);
		}
		else
		{
			return Close(IocpSock, IocpBuff);
		}
		break;
	default:
		break;
	}
}

/////////////////////////////////////////////////////////////////////////
//服务线程
DWORD WINAPI serverWorkerThread(LPVOID pParam)
{
	Reactor* reactor = (Reactor*)pParam;

	DWORD	dwIoSize = 0;
	IOCP_SOCKET* IocpSock = NULL;
	IOCP_BUFF* IocpBuff = NULL;		//IO数据,用于发起接收重叠操作
	bool bRet = false;
	while (true)
	{
		bRet = false;
		dwIoSize = 0;	//IO操作长度
		IocpSock = NULL;
		IocpBuff = NULL;
		bRet = GetQueuedCompletionStatus(reactor->ComPort, &dwIoSize, (PULONG_PTR)&IocpSock, (LPOVERLAPPED*)&IocpBuff, INFINITE);
		if (bRet == false)
		{
			if (IocpBuff == NULL || WAIT_TIMEOUT == GetLastError())
				continue;
			Close(IocpSock, IocpBuff);
			continue;
		}
		else if (0 == dwIoSize && (READ == IocpBuff->type || WRITE == IocpBuff->type))
		{
			Close(IocpSock, IocpBuff);
			continue;
		}
		else
		{
			IocpBuff->offset += dwIoSize;
			ProcessIO(IocpSock, IocpBuff);
		}
	}
	return 0;
}

DWORD WINAPI mainIOCPServer(LPVOID pParam)
{
	Reactor* reactor = (Reactor*)pParam;
	for (int i = 0; i < reactor->CPU_COUNT*2; i++)
	//for (unsigned int i = 0; i < 1; i++)
	{
		HANDLE ThreadHandle;
		ThreadHandle = CreateThread(NULL, 1024*1024*16, serverWorkerThread, pParam, 0, NULL);
		if (NULL == ThreadHandle) {
			return -4;
		}
		CloseHandle(ThreadHandle);
	}
	std::map<uint16_t, BaseFactory*>::iterator iter;
	while (reactor->Run)
	{
		for (iter = reactor->FactoryAll.begin(); iter != reactor->FactoryAll.end(); ++iter)
		{
			iter->second->FactoryLoop();
		}
		Sleep(1);
	}
	return 0;
}

int ReactorStart(Reactor* reactor)
{
	WSADATA wsData;
	if (0 != WSAStartup(0x0202, &wsData))
	{
		return SOCKET_ERROR;
	}

	reactor->ComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (reactor->ComPort == NULL)
	{
		return -2;
	}

	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	reactor->CPU_COUNT = sysInfor.dwNumberOfProcessors;

	SOCKET tempSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	//使用WSAIoctl获取AcceptEx函数指针
	DWORD dwbytes = 0;
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	if (0 != WSAIoctl(tempSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx, sizeof(guidAcceptEx),
		&reactor->lpfnAcceptEx, sizeof(reactor->lpfnAcceptEx),
		&dwbytes, NULL, NULL))
	{
		return -3;
	}
	closesocket(tempSock);

	HANDLE ThreadHandle;
	ThreadHandle = CreateThread(NULL, 1024*1024*16, mainIOCPServer, reactor, 0, NULL);
	if (NULL == ThreadHandle) {
		return -4;
	}
	CloseHandle(ThreadHandle);
	return 0;
}

void ReactorStop(Reactor* reactor)
{
	reactor->Run = false;
}

int FactoryRun(BaseFactory* fc)
{	
	if (!fc->FactoryInit())
		return -1;

	if (fc->ServerPort != 0)
	{
		fc->Listenfd = GetListenSock(fc->ServerPort);
		if (fc->Listenfd == SOCKET_ERROR)
			return -2;

		IOCP_SOCKET* IcpSock = NewIOCP_Socket();
		if (IcpSock == NULL)
		{
			closesocket(fc->Listenfd);
			return -3;
		}
		IcpSock->factory = fc;
		IcpSock->fd = fc->Listenfd;

		CreateIoCompletionPort((HANDLE)fc->Listenfd, fc->reactor->ComPort, (ULONG_PTR)IcpSock, 0);
		for (int i = 0; i < fc->reactor->CPU_COUNT; i++)
			PostAcceptClient(fc);
	}
	
	fc->reactor->FactoryAll.insert(std::pair<uint16_t, BaseFactory*>(fc->ServerPort, fc));
	return 0;
}

int FactoryStop(BaseFactory* fc)
{
	std::map<uint16_t, BaseFactory*>::iterator iter;
	iter = fc->reactor->FactoryAll.find(fc->ServerPort);
	if (iter != fc->reactor->FactoryAll.end())
	{
		fc->reactor->FactoryAll.erase(iter);
	}
	fc->FactoryClose();
	return 0;
}

static bool IOCPConnectUDP(BaseFactory* fc, IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	IocpSock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (IocpSock->fd == INVALID_SOCKET)
		return false;

	sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;
	bind(IocpSock->fd, (sockaddr*)(&local_addr), sizeof(sockaddr_in));

	DWORD flags = 0;
	if (ResetIocp_Buff(IocpSock, IocpBuff) == false)
	{
		closesocket(IocpSock->fd);
		return false;
	}

	CreateIoCompletionPort((HANDLE)IocpSock->fd, fc->reactor->ComPort, (ULONG_PTR)IocpSock, 0);

	int fromlen = sizeof(struct sockaddr);
	IocpBuff->type = READ;

	if (SOCKET_ERROR == WSARecvFrom(IocpSock->fd, &IocpBuff->databuf, 1, NULL, &flags, (sockaddr*)&IocpSock->peer_addr, &fromlen, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			closesocket(IocpSock->fd);
			return false;
		}
	}
	return true;
}

static bool IOCPConnectTCP(BaseFactory* fc, IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	IocpSock->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (IocpSock->fd == INVALID_SOCKET)
		return false;

	setsockopt(IocpSock->fd, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&(fc->Listenfd), sizeof(fc->Listenfd));

	sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;
	bind(IocpSock->fd, (sockaddr*)(&local_addr), sizeof(sockaddr_in));

	LPFN_CONNECTEX lpfnConnectEx = NULL;
	GUID GuidConnectEx = WSAID_CONNECTEX;
	DWORD dwBytes = 0;
	if (SOCKET_ERROR == WSAIoctl(IocpSock->fd, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidConnectEx, sizeof(GuidConnectEx),
		&lpfnConnectEx, sizeof(lpfnConnectEx), &dwBytes, 0, 0))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			closesocket(IocpSock->fd);
			return false;
		}
	}
	CreateIoCompletionPort((HANDLE)IocpSock->fd, fc->reactor->ComPort, (ULONG_PTR)IocpSock, 0);

	PVOID lpSendBuffer = NULL;
	DWORD dwSendDataLength = 0;
	DWORD dwBytesSent = 0;
	BOOL bResult = lpfnConnectEx(IocpSock->fd,
		(SOCKADDR*)&IocpSock->peer_addr,	// [in] 对方地址
		sizeof(IocpSock->peer_addr),		// [in] 对方地址长度
		lpSendBuffer,			// [in] 连接后要发送的内容，这里不用
		dwSendDataLength,		// [in] 发送内容的字节数 ，这里不用
		&dwBytesSent,			// [out] 发送了多少个字节，这里不用
		&(IocpBuff->overlapped));

	if (!bResult)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			closesocket(IocpSock->fd);
			return false;
		}
	}
	return true;
}

HSOCKET HsocketConnect(BaseProtocol* proto, const char* ip, int port, CONN_TYPE iotype)
{
	if (proto == NULL || (proto->sockCount == 0 && proto->protoType == SERVER_PROTOCOL && proto->protolock != NULL))
		return NULL;
	BaseFactory* fc = proto->factory;
	IOCP_SOCKET* IocpSock = NewIOCP_Socket();
	if (IocpSock == NULL)
	{
		return NULL;
	}
	IOCP_BUFF* IocpBuff = NewIOCP_Buff();
	if (IocpBuff == NULL)
	{
		ReleaseIOCP_Socket(IocpSock);
		return NULL;
	}
	IocpBuff->type = CONNECT;

	IocpSock->factory = fc;
	IocpSock->_iotype = iotype > UDP_CONN ? 0:iotype;
	memcpy(IocpSock->peer_ip, ip, strlen(ip));
	IocpSock->peer_port = port;
	IocpSock->_user = proto;
	IocpSock->_IocpBuff = IocpBuff;
	IocpSock->peer_addr.sin_family = AF_INET;
	IocpSock->peer_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &IocpSock->peer_addr.sin_addr);

	bool ret = false;
	if (iotype == UDP_CONN)
		ret = IOCPConnectUDP(fc, IocpSock, IocpBuff);   //UDP连接
	else
		ret = IOCPConnectTCP(fc, IocpSock, IocpBuff);   //TCP连接

	if (ret == false)
	{
		ReleaseIOCP_Buff(IocpBuff);
		ReleaseIOCP_Socket(IocpSock);
		return NULL;
	}
	InterlockedIncrement(&proto->sockCount);
	return IocpSock;
}

static bool IOCPPostSendUDPEx(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	DWORD flags = 0;
	if (SOCKET_ERROR == WSASendTo(IocpSock->fd, &IocpBuff->databuf, 1, NULL, flags, (sockaddr*)&IocpSock->peer_addr, sizeof(IocpSock->peer_addr), &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
			return false;
	}
	return true;
}

static bool IOCPPostSendTCPEx(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	DWORD flags = 0;
	if (SOCKET_ERROR == WSASend(IocpSock->fd, &IocpBuff->databuf, 1, NULL, flags, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
			return false;
	}
	return true;
}

bool HsocketSend(IOCP_SOCKET* IocpSock, const char* data, int len)    //注意此方法存在内存泄漏风险，如果此投递未返回时socket被关闭
{
	if (IocpSock == NULL)
		return false;
	
	IOCP_BUFF* IocpBuff = NewIOCP_Buff();
	if (IocpBuff == NULL)
		return false;

	IocpBuff->databuf.buf = (char*)pst_malloc(len);
	if (IocpBuff->databuf.buf == NULL)
	{
		ReleaseIOCP_Buff(IocpBuff);
		return false;
	}
	memcpy(IocpBuff->databuf.buf, data, len);
	IocpBuff->databuf.len = len;
	memset(&IocpBuff->overlapped, 0, sizeof(OVERLAPPED));
	IocpBuff->type = WRITE;

	bool ret = false;
	if (IocpSock->_iotype == UDP_CONN)
		ret = IOCPPostSendUDPEx(IocpSock, IocpBuff);
	else
		ret = IOCPPostSendTCPEx(IocpSock, IocpBuff);
	
	if (ret == false)
	{
		pst_free(IocpBuff->databuf.buf);
		ReleaseIOCP_Buff(IocpBuff);
		return false;
	}
	IocpSock->heartbeat = time(NULL);
	return true;
}

bool HsocketClose(IOCP_SOCKET*  &IocpSock)
{
	if (IocpSock == NULL ||IocpSock->fd == INVALID_SOCKET)
		return false;
	shutdown(IocpSock->fd, SD_RECEIVE);
	IocpSock = NULL;
	//closesocket(IocpSock->fd);
	return true;
}

int HsocketSkipBuf(IOCP_SOCKET* IocpSock, int len)
{
	IocpSock->_IocpBuff->offset -= len;
	memmove(IocpSock->recv_buf, IocpSock->recv_buf + len, IocpSock->_IocpBuff->offset);
	return IocpSock->_IocpBuff->offset;
}