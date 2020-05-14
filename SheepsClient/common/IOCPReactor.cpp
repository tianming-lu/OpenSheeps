#include "pch.h"
#include "framework.h"
#include "IOCPReactor.h"
#include <Ws2tcpip.h>
#include <map>

#pragma comment(lib, "Ws2_32.lib")	

Reactor::~Reactor()
{
	while (HbuffPool.size())
	{
		free(HbuffPool.top());
		HbuffPool.pop();
	}
	while (HsocketPool.size())
	{
		free(HsocketPool.top());
		HsocketPool.pop();
	}
}

static IOCP_SOCKET* NewIOCP_Socket(Reactor* reactor)
{
	IOCP_SOCKET* IocpSock = NULL;
	reactor->HsocketPoolLock.lock();
	if (!reactor->HsocketPool.empty())
	{
		IocpSock = reactor->HsocketPool.top();
		reactor->HsocketPool.pop();
	}
	else
	{
		IocpSock = (IOCP_SOCKET*)malloc(sizeof(IOCP_SOCKET));
	}
	reactor->HsocketPoolLock.unlock();
	if (IocpSock)
		memset(IocpSock, 0, sizeof(IOCP_SOCKET));
	return IocpSock;
}

static void ReleaseIOCP_Socket(Reactor* reactor, IOCP_SOCKET* IocpSock)
{
	reactor->HsocketPoolLock.lock();
	reactor->HsocketPool.push(IocpSock);
	reactor->HsocketPoolLock.unlock();
}

static IOCP_BUFF* NewIOCP_Buff(Reactor* reactor)
{
	IOCP_BUFF* IocpBuff = NULL;
	reactor->HbuffPoolLock.lock();
	if (!reactor->HbuffPool.empty())
	{
		IocpBuff = reactor->HbuffPool.top();
		reactor->HbuffPool.pop();
	}
	else
	{
		IocpBuff = (IOCP_BUFF*)malloc(sizeof(IOCP_BUFF));
	}
	reactor->HbuffPoolLock.unlock();
	if (IocpBuff)
		memset(IocpBuff, 0, sizeof(IOCP_BUFF));
	return IocpBuff;
}

static void ReleaseIOCP_Buff(Reactor* reactor, IOCP_BUFF* IocpBuff)
{
	reactor->HbuffPoolLock.lock();
	reactor->HbuffPool.push(IocpBuff);
	reactor->HbuffPoolLock.unlock();
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

static bool AcceptClient(BaseFactory* fc)
{
	IOCP_BUFF* IocpBuff;
	IocpBuff = NewIOCP_Buff(fc->reactor);
	if (IocpBuff == NULL)
	{
		return false;
	}
	IocpBuff->databuf.buf = IocpBuff->recv_buffer;
	IocpBuff->databuf.len = DATA_BUFSIZE;
	IocpBuff->type = ACCEPT;

	IocpBuff->sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (IocpBuff->sock == INVALID_SOCKET)
	{
		ReleaseIOCP_Buff(fc->reactor, IocpBuff);
		return false;
	}

	/*调用AcceptEx函数，地址长度需要在原有的上面加上16个字节向服务线程投递一个接收连接的的请求*/
	bool rc = fc->reactor->lpfnAcceptEx(fc->sListen, IocpBuff->sock,
		IocpBuff->recv_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&IocpBuff->databuf.len, &(IocpBuff->overlapped));

	if (false == rc)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			ReleaseIOCP_Buff(fc->reactor, IocpBuff);
			return false;
		}
	}
	return true;
}

static bool Close(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	if (IocpSock != NULL)
	{
		if (ACCEPT == IocpBuff->type)
		{
			ReleaseIOCP_Buff(IocpSock->factory->reactor, IocpBuff);
			AcceptClient(IocpSock->factory);
			return true;
		}
		else if (WRITEEX == IocpBuff->type)
		{
			if (IocpBuff->send_buffer != NULL)
				free(IocpBuff->send_buffer);
			ReleaseIOCP_Buff(IocpSock->factory->reactor, IocpBuff);
			return true;
		}
		else if (CONNECT == IocpBuff->type)
		{
			if (IocpSock->sock != INVALID_SOCKET  && *(IocpSock->user) != NULL)
			{
				IocpSock->userlock->lock();
				if (IocpSock->sock != INVALID_SOCKET && *(IocpSock->user) != NULL)
				{
					(*(IocpSock->user))->_sockCount -= 1;
					(*(IocpSock->user))->ConnectionFailed(IocpSock, IocpSock->IP, IocpSock->PORT);
				}
				IocpSock->userlock->unlock();
			}
		}
		else if(READ == IocpBuff->type)
		{
			if (IocpSock->sock != INVALID_SOCKET && *(IocpSock->user) != NULL)
			{
				IocpSock->userlock->lock();
				if (*(IocpSock->user) != NULL && IocpSock->sock != INVALID_SOCKET)
				{
					(*(IocpSock->user))->_sockCount -= 1;
					(*(IocpSock->user))->ConnectionClosed(IocpSock, IocpSock->IP, IocpSock->PORT);
				}
				IocpSock->userlock->unlock();
			}
		}

		if (IocpSock->sock != INVALID_SOCKET && IocpSock->sock != NULL)
		{
			CancelIo((HANDLE)IocpSock->sock);	//取消等待执行的异步操作
			closesocket(IocpSock->sock);
			IocpSock->sock = INVALID_SOCKET;
		}
		ReleaseIOCP_Buff(IocpSock->factory->reactor, IocpBuff);
		ReleaseIOCP_Socket(IocpSock->factory->reactor, IocpSock);
	}
	MemoryBarrier();
	return true;
}

static bool PostRecvUDP(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	DWORD flags = 0;
	memset(&IocpBuff->overlapped, 0, sizeof(OVERLAPPED));
	memset(&IocpBuff->recv_buffer, 0, DATA_BUFSIZE);

	IocpBuff->type = READ;
	IocpBuff->databuf.buf = IocpBuff->recv_buffer;
	IocpBuff->databuf.len = DATA_BUFSIZE;

	if (SOCKET_ERROR == WSARecvFrom(IocpSock->sock, &IocpBuff->databuf, 1, NULL, &flags, NULL, NULL, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			Close(IocpSock, IocpBuff);
			return false;
		}
	}
	return true;
}

static bool PostRecv(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	if (IocpSock->iotype == IOCP_UDP)
		return PostRecvUDP(IocpSock, IocpBuff);
	DWORD flags = 0;
	memset(&IocpBuff->overlapped, 0, sizeof(OVERLAPPED));
	memset(&IocpBuff->recv_buffer, 0, DATA_BUFSIZE);

	IocpBuff->type = READ;
	IocpBuff->databuf.buf = IocpBuff->recv_buffer;
	IocpBuff->databuf.len = DATA_BUFSIZE;

	if (SOCKET_ERROR == WSARecv(IocpSock->sock, &IocpBuff->databuf, 1, NULL, &flags, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			Close(IocpSock, IocpBuff);
			return false;
		}
	}
	return true;
}

static bool PostSendUDP(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	DWORD flags = 0;
	IocpBuff->databuf.len = IocpBuff->buffer_len;
	memset(&IocpBuff->overlapped, 0, sizeof(OVERLAPPED));

	IocpBuff->type = WRITE;

	if (SOCKET_ERROR == WSASendTo(IocpSock->sock, &IocpBuff->databuf, 1, NULL, flags, (sockaddr*)&IocpSock->SAddr, sizeof(IocpSock->SAddr), &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			Close(IocpSock, IocpBuff);
			return false;
		}
	}
	IocpSock->timeout = time(NULL);
	return true;
}

static bool PostSend(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	if (IocpSock->iotype == IOCP_UDP)
		return PostSendUDP(IocpSock, IocpBuff);
	DWORD flags = 0;
	IocpBuff->databuf.len = IocpBuff->buffer_len;
	memset(&IocpBuff->overlapped, 0, sizeof(OVERLAPPED));

	IocpBuff->type = WRITE;

	if (SOCKET_ERROR == WSASend(IocpSock->sock, &IocpBuff->databuf, 1, NULL, flags, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			Close(IocpSock, IocpBuff);
			return false;
		}
	}
	IocpSock->timeout = time(NULL);
	return true;
}

static bool PostAceept(IOCP_SOCKET* IocpListenSock, IOCP_BUFF* IocpBuff)
{
	BaseFactory* fc = IocpListenSock->factory;
	Reactor* reactor = fc->reactor;
	setsockopt(IocpBuff->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) & (IocpListenSock->sock), sizeof(IocpListenSock->sock));

	IOCP_SOCKET* IocpSock = NewIOCP_Socket(fc->reactor);
	if (IocpSock == NULL)
	{
		return false;
	}
	memset(IocpSock, 0, sizeof(IOCP_SOCKET));
	IocpSock->sock = IocpBuff->sock;
	BaseProtocol* proto = fc->CreateProtocol();	//用户指针
	proto->_self = proto;
	proto->_factory = fc;
	IocpSock->factory = fc;
	IocpSock->user = &(proto->_self);	//用户指针的地址
	IocpSock->userlock = proto->_protolock;
	IocpSock->IocpBuff = IocpBuff;

	SOCKADDR_IN addr_conn;
	int nSize = sizeof(addr_conn);
	getpeername(IocpSock->sock, (SOCKADDR*)&addr_conn, &nSize);
	inet_ntop(AF_INET, &addr_conn.sin_addr, IocpSock->IP, sizeof(IocpSock->IP));
	IocpSock->PORT = ntohs(addr_conn.sin_port);

	SOCKADDR_IN* addrClient = NULL, * addrLocal = NULL;
	int nClientLen = sizeof(SOCKADDR_IN), nLocalLen = sizeof(SOCKADDR_IN);

	fc->reactor->lpfnGetAcceptExSockaddrs(IocpBuff->recv_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		(LPSOCKADDR*)& addrLocal, &nLocalLen,
		(LPSOCKADDR*)& addrClient, &nClientLen);

	CreateIoCompletionPort((HANDLE)IocpSock->sock, reactor->ComPort, (ULONG_PTR)IocpSock, 0);	//将监听到的套接字关联到完成端口
	proto->_sockCount += 1;
	(*(IocpSock->user))->ConnectionMade(IocpSock, IocpSock->IP, IocpSock->PORT);

	PostRecv(IocpSock, IocpBuff);

	AcceptClient(IocpListenSock->factory);
	return true;
}

static bool ProcessIO(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	if (IocpBuff->type == READ)
	{
		if (IocpSock->sock != INVALID_SOCKET && *(IocpSock->user) != NULL)
		{
			int ret = CLOSE;
			IocpSock->userlock->lock();
			if (IocpSock->sock != INVALID_SOCKET && *(IocpSock->user) != NULL)
				ret = (*(IocpSock->user))->Recv(IocpSock, IocpSock->IP, IocpSock->PORT, IocpBuff->recv_buffer, IocpBuff->buffer_len);
			IocpSock->userlock->unlock();
			switch (ret)
			{
			case RECV:
				PostRecv(IocpSock, IocpBuff);
				break;
			case SEND:
				PostSend(IocpSock, IocpBuff);
				break;
			case SEND_CLOSE:
				IocpBuff->close = true;
				PostSend(IocpSock, IocpBuff);
				break;
			case CLOSE:
				Close(IocpSock, IocpBuff);
				break;
			default:
				break;
			}
		}
	}
	else if (IocpBuff->type == WRITE)
	{
		if (IocpBuff->close == true)
			Close(IocpSock, IocpBuff);
		else
			PostRecv(IocpSock, IocpBuff);
	}
	else if (IocpBuff->type == WRITEEX)
	{
		if (IocpBuff->send_buffer != NULL)
			free(IocpBuff->send_buffer);
		ReleaseIOCP_Buff(IocpSock->factory->reactor, IocpBuff);
	}
	else if (IocpBuff->type == ACCEPT)
	{
		PostAceept(IocpSock, IocpBuff);
	}
	else if (IocpBuff->type == CONNECT)
	{
		if (IocpSock->sock != INVALID_SOCKET && *(IocpSock->user) != NULL)
		{
			IocpSock->userlock->lock();
			if (IocpSock->sock != INVALID_SOCKET && *(IocpSock->user) != NULL)
				(*(IocpSock->user))->ConnectionMade(IocpSock, IocpSock->IP, IocpSock->PORT);
			IocpSock->userlock->unlock();
			PostRecv(IocpSock, IocpBuff);
		}
		else
		{
			Close(IocpSock, IocpBuff);
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////
//服务线程
DWORD WINAPI serverWorkerThread(LPVOID pParam)
{
	Reactor* reactor = (Reactor*)pParam;

	DWORD	dwIoSize;
	IOCP_SOCKET* IocpSock;
	IOCP_BUFF* IocpBuff;		//IO数据,用于发起接收重叠操作
	bool bRet;
	while (true)
	{
		bRet = false;
		dwIoSize = -1;	//IO操作长度
		IocpBuff = NULL;
		bRet = GetQueuedCompletionStatus(reactor->ComPort, &dwIoSize, (PULONG_PTR)&IocpSock, (LPOVERLAPPED*)&IocpBuff, INFINITE);
		if (bRet == false)
		{
			if (WAIT_TIMEOUT == GetLastError())
			{
				continue;
			}
			else if (NULL != IocpBuff)
			{
				Close(IocpSock, IocpBuff);
			}
		}
		else
		{
			if (0 == dwIoSize && (READ == IocpBuff->type || WRITE == IocpBuff->type || WRITEEX == IocpBuff->type))
			{
				Close(IocpSock, IocpBuff);
				continue;
			}
			else
			{
				IocpBuff->buffer_len = dwIoSize;
				ProcessIO(IocpSock, IocpBuff);
			}
		}
	}

	return 0;
}

DWORD WINAPI mainIOCPServer(LPVOID pParam)
{
	Reactor* reactor = (Reactor*)pParam;
	for (unsigned int i = 0; i < reactor->CPU_COUNT; i++)
	{
		HANDLE ThreadHandle;
		ThreadHandle = CreateThread(NULL, 0, serverWorkerThread, pParam, 0, NULL);
		if (NULL == ThreadHandle) {
			return -4;
		}
		CloseHandle(ThreadHandle);
	}
	while (reactor->Run)
	{
		std::map<short, BaseFactory*>::iterator iter;
		for (iter = reactor->FactoryAll.begin(); iter != reactor->FactoryAll.end(); iter++)
		{
			iter->second->FactoryLoop();
		}
		Sleep(1);
	}
	return 0;
}

int IOCPServerStart(Reactor* reactor)
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

	SOCKET listenSock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	//使用WSAIoctl获取AcceptEx函数指针
	DWORD dwbytes = 0;
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	if (0 != WSAIoctl(listenSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidAcceptEx, sizeof(guidAcceptEx),
		&reactor->lpfnAcceptEx, sizeof(reactor->lpfnAcceptEx),
		&dwbytes, NULL, NULL))
	{
		return -3;
	}
	// 获取GetAcceptExSockAddrs函数指针，也是同理
	GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
	if (0 != WSAIoctl(listenSock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&guidGetAcceptExSockaddrs,
		sizeof(guidGetAcceptExSockaddrs),
		&reactor->lpfnGetAcceptExSockaddrs,
		sizeof(reactor->lpfnGetAcceptExSockaddrs),
		&dwbytes, NULL, NULL))
	{
		return -4;
	}
	closesocket(listenSock);

	HANDLE ThreadHandle = CreateThread(NULL, 0, mainIOCPServer, reactor, 0, NULL);
	if (NULL == ThreadHandle) {
		return -3;
	}
	CloseHandle(ThreadHandle);
	return 0;
}

void IOCPServerStop(Reactor* reactor)
{
	reactor->Run = false;
}

int IOCPFactoryRun(BaseFactory* fc)
{	
	fc->FactoryInit();

	if (fc->ServerPort != 0)
	{
		fc->sListen = GetListenSock(fc->ServerPort);
		if (fc->sListen == SOCKET_ERROR)
			return -1;

		IOCP_SOCKET* IcpSock = NewIOCP_Socket(fc->reactor);
		if (IcpSock == NULL)
		{
			closesocket(fc->sListen);
			return -2;
		}
		IcpSock->factory = fc;
		IcpSock->sock = fc->sListen;

		CreateIoCompletionPort((HANDLE)fc->sListen, fc->reactor->ComPort, (ULONG_PTR)IcpSock, 0);
		for (DWORD i = 0; i < fc->reactor->CPU_COUNT; i++)
			AcceptClient(fc);
	}
	
	fc->reactor->FactoryAll.insert(std::pair<short, BaseFactory*>(fc->ServerPort, fc));
	return 0;
}

int IOCPFactoryStop(BaseFactory* fc)
{
	std::map<short, BaseFactory*>::iterator iter;
	iter = fc->reactor->FactoryAll.find(fc->ServerPort);
	if (iter != fc->reactor->FactoryAll.end())
	{
		fc->reactor->FactoryAll.erase(iter);
	}
	fc->FactoryClose();
	return 0;
}

static HSOCKET IOCPConnectUDP(const char* ip, int port, BaseProtocol* proto, uint8_t iotype)
{
	BaseFactory* fc = proto->_factory;
	IOCP_SOCKET* IocpSock = NewIOCP_Socket(fc->reactor);
	if (IocpSock == NULL)
	{
		return NULL;
	}
	IocpSock->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	IocpSock->iotype = iotype;

	IocpSock->SAddr.sin_family = AF_INET;
	IocpSock->SAddr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &IocpSock->SAddr.sin_addr);

	IocpSock->factory = fc;
	memcpy(IocpSock->IP, ip, strlen(ip));
	IocpSock->PORT = port;
	IocpSock->user = &(proto->_self);
	IocpSock->userlock = proto->_protolock;

	sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;

	bind(IocpSock->sock, (sockaddr*)(&local_addr), sizeof(sockaddr_in));
	IocpSock->IocpBuff = NewIOCP_Buff(fc->reactor);

	CreateIoCompletionPort((HANDLE)IocpSock->sock, fc->reactor->ComPort, (ULONG_PTR)IocpSock, 0);
	proto->_sockCount += 1;

	PostRecv(IocpSock, IocpSock->IocpBuff);
	return IocpSock;
}
HSOCKET IOCPConnectEx(const char* ip, int port, BaseProtocol* proto, uint8_t iotype)
{
	if (iotype == IOCP_UDP)
		return IOCPConnectUDP(ip, port, proto, iotype);
	BaseFactory* fc = proto->_factory;
	IOCP_BUFF* IocpBuff;
	IocpBuff = NewIOCP_Buff(fc->reactor);
	if (IocpBuff == NULL)
	{
		return NULL;
	}
	IocpBuff->databuf.buf = IocpBuff->recv_buffer;
	IocpBuff->databuf.len = DATA_BUFSIZE;
	IocpBuff->type = CONNECT;
	IocpBuff->overlapped.hEvent = NULL;
	//pIO->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	IocpBuff->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (IocpBuff->sock == INVALID_SOCKET)
	{
		closesocket(IocpBuff->sock);
		ReleaseIOCP_Buff(fc->reactor, IocpBuff);
		return NULL;
	}
	setsockopt(IocpBuff->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) & (fc->sListen), sizeof(fc->sListen));

	sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;

	int irt = bind(IocpBuff->sock, (sockaddr*)(&local_addr), sizeof(sockaddr_in));
	IOCP_SOCKET* IocpSock = NewIOCP_Socket(fc->reactor);
	if (IocpSock == NULL)
	{
		closesocket(IocpBuff->sock);
		ReleaseIOCP_Buff(fc->reactor, IocpBuff);
		return NULL;
	}
	IocpSock->factory = fc;
	IocpSock->sock = IocpBuff->sock;
	IocpSock->iotype = iotype;
	memcpy(IocpSock->IP, ip, strlen(ip));
	IocpSock->PORT = port;
	IocpSock->user = &(proto->_self);
	IocpSock->userlock = proto->_protolock;
	IocpSock->IocpBuff = IocpBuff;

	SOCKADDR_IN addrSrv;
	memset(&addrSrv, 0, sizeof(addrSrv));
	addrSrv.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &addrSrv.sin_addr);
	addrSrv.sin_port = htons(port);

	LPFN_CONNECTEX lpfnConnectEx = NULL;
	GUID GuidConnectEx = WSAID_CONNECTEX;
	DWORD dwBytes = 0;
	if (SOCKET_ERROR == WSAIoctl(IocpSock->sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidConnectEx, sizeof(GuidConnectEx),
		&lpfnConnectEx, sizeof(lpfnConnectEx), &dwBytes, 0, 0))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			closesocket(IocpSock->sock);
			ReleaseIOCP_Buff(fc->reactor, IocpBuff);
			ReleaseIOCP_Socket(fc->reactor, IocpSock);
			return NULL;
		}
	}
	CreateIoCompletionPort((HANDLE)IocpSock->sock, fc->reactor->ComPort, (ULONG_PTR)IocpSock, 0);
	proto->_sockCount += 1;

	PVOID lpSendBuffer = NULL;
	DWORD dwSendDataLength = 0;
	DWORD dwBytesSent = 0;
	BOOL bResult = lpfnConnectEx(IocpBuff->sock,
		(SOCKADDR*)&addrSrv,	// [in] 对方地址
		sizeof(addrSrv),		// [in] 对方地址长度
		lpSendBuffer,			// [in] 连接后要发送的内容，这里不用
		dwSendDataLength,		// [in] 发送内容的字节数 ，这里不用
		&dwBytesSent,			// [out] 发送了多少个字节，这里不用
		&(IocpBuff->overlapped));

	if (!bResult)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			closesocket(IocpSock->sock);
			ReleaseIOCP_Buff(fc->reactor, IocpBuff);
			ReleaseIOCP_Socket(fc->reactor, IocpSock);
			return NULL;
		}
	}
	return IocpSock;
}

static bool IOCPPostSendUDP(IOCP_SOCKET* IocpSock, const char* data, int len)    //注意此方法存在内存泄漏风险，如果此投递未返回时socket被关闭
{
	if (IocpSock != NULL && IocpSock->sock != INVALID_SOCKET)
	{
		sendto(IocpSock->sock, data, len, 0, (sockaddr*)&IocpSock->SAddr, sizeof(IocpSock->SAddr));
	}
	
	return true;
}

static bool IOCPPostSend(IOCP_SOCKET* IocpSock, const char* data, int len)
{
	if (IocpSock->iotype == IOCP_UDP)
		return IOCPPostSendUDP(IocpSock, data, len);
	if (IocpSock != NULL && IocpSock->sock != INVALID_SOCKET)
	{
		send(IocpSock->sock, data, len, 0);
	}
	return true;
}

bool IOCPPostSendEx(IOCP_SOCKET* IocpSock, const char* data, int len)    //注意此方法存在内存泄漏风险，如果此投递未返回时socket被关闭
{
	return IOCPPostSend(IocpSock, data, len);
	if (IocpSock == NULL)
	{
		return false;
	}

	DWORD flags = 0;
	IOCP_BUFF* IocpBuff = NewIOCP_Buff(IocpSock->factory->reactor);
	if (IocpBuff == NULL)
	{
		return false;
	}

	IocpBuff->send_buffer = (char*)malloc(len);
	if (IocpBuff->send_buffer == NULL)
	{
		ReleaseIOCP_Buff(IocpSock->factory->reactor, IocpBuff);
		return false;
	}
	memcpy(IocpBuff->send_buffer, data, len);

	IocpBuff->databuf.len = len;
	IocpBuff->databuf.buf = IocpBuff->send_buffer;
	memset(&IocpBuff->overlapped, 0, sizeof(OVERLAPPED));

	IocpBuff->type = WRITEEX;

	if (SOCKET_ERROR == WSASend(IocpSock->sock, &IocpBuff->databuf, 1, NULL, flags, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			free(IocpBuff->send_buffer);
			ReleaseIOCP_Buff(IocpSock->factory->reactor, IocpBuff);
			return false;
		}
	}
	IocpSock->timeout = time(NULL);
	return true;
}

bool IOCPCloseHsocket(IOCP_SOCKET*  IocpSock)
{
	if (IocpSock == NULL ||IocpSock->sock == INVALID_SOCKET)
		return false;
	closesocket(IocpSock->sock);
	return true;
}

bool IOCPDestroyProto(BaseProtocol* proto)
{
	if (proto->_sockCount > 0)
		return false;
	BaseFactory* fc = proto->_factory;
	fc->DeleteProtocol(proto->_self);
	return true;
}