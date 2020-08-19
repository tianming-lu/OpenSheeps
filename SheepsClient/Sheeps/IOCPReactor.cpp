#include "pch.h"
#include "IOCPReactor.h"
#include "SheepsMemory.h"
#include <Ws2tcpip.h>
#include <map>

#pragma comment(lib, "Ws2_32.lib")	

#define DATA_BUFSIZE 5120
#define READ	0
#define WRITE	1
#define ACCEPT	2
#define CONNECT 3

static IOCP_SOCKET* NewIOCP_Socket()
{
	return (IOCP_SOCKET*)sheeps_malloc(sizeof(IOCP_SOCKET));
}

static void ReleaseIOCP_Socket(IOCP_SOCKET* IocpSock)
{
	sheeps_free(IocpSock);
}

static IOCP_BUFF* NewIOCP_Buff()
{
	return (IOCP_BUFF*)sheeps_malloc(sizeof(IOCP_BUFF));
}

static void ReleaseIOCP_Buff(IOCP_BUFF* IocpBuff)
{
	sheeps_free(IocpBuff);
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

static bool PostAcceptClient(BaseFactory* fc)
{
	IOCP_BUFF* IocpBuff;
	IocpBuff = NewIOCP_Buff();
	if (IocpBuff == NULL)
	{
		return false;
	}
	IocpBuff->databuf.buf = (char*)sheeps_malloc(DATA_BUFSIZE);
	if (IocpBuff->databuf.buf == NULL)
	{
		ReleaseIOCP_Buff(IocpBuff);
		return false;
	}
	IocpBuff->databuf.len = DATA_BUFSIZE;
	IocpBuff->type = ACCEPT;

	IocpBuff->sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (IocpBuff->sock == INVALID_SOCKET)
	{
		ReleaseIOCP_Buff(IocpBuff);
		return false;
	}

	/*调用AcceptEx函数，地址长度需要在原有的上面加上16个字节向服务线程投递一个接收连接的的请求*/
	bool rc = fc->reactor->lpfnAcceptEx(fc->sListen, IocpBuff->sock,
		IocpBuff->databuf.buf, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&IocpBuff->databuf.len, &(IocpBuff->overlapped));

	if (false == rc)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			ReleaseIOCP_Buff(IocpBuff);
			return false;
		}
	}
	return true;
}

static void Close(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff )
{
	switch (IocpBuff->type)
	{
	case ACCEPT:
		if (IocpBuff->databuf.buf)
			sheeps_free(IocpBuff->databuf.buf);
		ReleaseIOCP_Buff(IocpBuff);
		PostAcceptClient(IocpSock->factory);
		return;
	case WRITE:
		if (IocpBuff->databuf.buf != NULL)
			sheeps_free(IocpBuff->databuf.buf);
		ReleaseIOCP_Buff(IocpBuff);
		return;
	default:
		break;
	}
	BaseProtocol* proto = *(IocpSock->_user);
	int left_count = 99;
	if (IocpSock->sock != INVALID_SOCKET)
	{
		proto->_protolock->lock();
		if (IocpSock->sock != INVALID_SOCKET)
		{
			left_count = proto->_sockCount -= 1;
			if (CONNECT == IocpBuff->type)
				proto->ConnectionFailed(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
			else
				proto->ConnectionClosed(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
		}
		proto->_protolock->unlock();
	}
	if (left_count == 0 && proto != NULL && proto->_protoType == SERVER_PROTOCOL)
		IocpSock->factory->DeleteProtocol(proto);

	if (IocpSock->sock != INVALID_SOCKET && IocpSock->sock != NULL)
	{
		SOCKET tempsock = IocpSock->sock;
		IocpSock->sock = INVALID_SOCKET;
		CancelIo((HANDLE)tempsock);	//取消等待执行的异步操作
		closesocket(tempsock);

	}
	if (IocpSock->recv_buf)
		sheeps_free(IocpSock->recv_buf);
	ReleaseIOCP_Buff(IocpBuff);
	ReleaseIOCP_Socket(IocpSock);
	MemoryBarrier();
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
			IocpSock->recv_buf = (char*)sheeps_malloc(DATA_BUFSIZE);
			if (IocpSock->recv_buf == NULL)
				return false;
			IocpBuff->size = DATA_BUFSIZE;
		}
	}
	IocpBuff->databuf.len = IocpBuff->size - IocpBuff->offset;
	if (IocpBuff->databuf.len == 0)
	{
		IocpBuff->size += DATA_BUFSIZE;
		IocpSock->recv_buf = (char*)sheeps_realloc(IocpSock->recv_buf, IocpBuff->size);
		if (IocpSock->recv_buf == NULL)
			return false;
		IocpBuff->databuf.len = IocpBuff->size - IocpBuff->offset;
	}
	IocpBuff->databuf.buf = IocpSock->recv_buf + IocpBuff->offset;
	return true;
}

static bool PostRecvUDP(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff, BaseProtocol* proto)
{
	int fromlen = sizeof(struct sockaddr);
	DWORD flags = 0;
	if (SOCKET_ERROR == WSARecvFrom(IocpSock->sock, &IocpBuff->databuf, 1, NULL, &flags, (sockaddr*)&IocpSock->peer_addr, &fromlen, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			Close(IocpSock, IocpBuff);
			return false;
		}
	}
	return true;
}

static bool PostRecvTCP(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff, BaseProtocol* proto)
{
	DWORD flags = 0;
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

static bool PostRecv(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff, BaseProtocol* proto)
{
	IocpBuff->type = READ;
	if (ResetIocp_Buff(IocpSock, IocpBuff) == false)
	{
		Close(IocpSock, IocpBuff);
		return false;
	}

	if (IocpSock->_iotype == UDP_CONN)
		return PostRecvUDP(IocpSock, IocpBuff, proto);
	return PostRecvTCP(IocpSock, IocpBuff, proto);
}

static bool AceeptClient(IOCP_SOCKET* IocpListenSock, IOCP_BUFF* IocpBuff)
{
	BaseFactory* fc = IocpListenSock->factory;
	Reactor* reactor = fc->reactor;
	setsockopt(IocpBuff->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) & (IocpListenSock->sock), sizeof(IocpListenSock->sock));

	IOCP_SOCKET* IocpSock = NewIOCP_Socket();
	if (IocpSock == NULL)
	{
		Close(IocpListenSock, IocpBuff);
		return false;
	}
	IocpSock->sock = IocpBuff->sock;
	BaseProtocol* proto = fc->CreateProtocol();	//用户指针
	if (proto == NULL)
	{
		ReleaseIOCP_Socket(IocpSock);
		Close(IocpListenSock, IocpBuff);
		return false;
	}
	proto->SetFactory(fc, SERVER_PROTOCOL);
	IocpSock->factory = fc;
	IocpSock->_user = &(proto->_self);	//用户指针的地址
	IocpSock->_IocpBuff = IocpBuff;

	int nSize = sizeof(IocpSock->peer_addr);
	getpeername(IocpSock->sock, (SOCKADDR*)&IocpSock->peer_addr, &nSize);
	inet_ntop(AF_INET, &IocpSock->peer_addr.sin_addr, IocpSock->peer_ip, sizeof(IocpSock->peer_ip));
	IocpSock->peer_port = ntohs(IocpSock->peer_addr.sin_port);
	proto->_sockCount += 1;
	CreateIoCompletionPort((HANDLE)IocpSock->sock, reactor->ComPort, (ULONG_PTR)IocpSock, 0);	//将监听到的套接字关联到完成端口
	proto->_protolock->lock();
	proto->ConnectionMade(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
	proto->_protolock->unlock();

	PostRecv(IocpSock, IocpBuff, proto);
	PostAcceptClient(IocpListenSock->factory);
	return true;
}

static bool ProcessIO(IOCP_SOCKET* &IocpSock, IOCP_BUFF* &IocpBuff)
{
	BaseProtocol* proto = NULL;
	switch (IocpBuff->type)
	{
	case READ:
		proto = *(IocpSock->_user);
		if (IocpSock->sock != INVALID_SOCKET)
		{
			if (IocpSock->_iotype == UDP_CONN)
			{
				inet_ntop(AF_INET, &IocpSock->peer_addr.sin_addr, IocpSock->peer_ip, sizeof(IocpSock->peer_ip));
				IocpSock->peer_port = ntohs(IocpSock->peer_addr.sin_port);
			}

			proto->_protolock->lock();
			if (IocpSock->sock != INVALID_SOCKET)
				proto->Recved(IocpSock, IocpSock->peer_ip, IocpSock->peer_port, IocpSock->recv_buf, IocpBuff->offset);
			proto->_protolock->unlock();

			PostRecv(IocpSock, IocpBuff, proto);
		}
		break;
	case WRITE:
		Close(IocpSock, IocpBuff);
		break;
	case ACCEPT:
		AceeptClient(IocpSock, IocpBuff);
		break;
	case CONNECT:
		proto = *(IocpSock->_user);
		if (IocpSock->sock != INVALID_SOCKET)
		{
			proto->_protolock->lock();
			if (IocpSock->sock != INVALID_SOCKET)
				proto->ConnectionMade(IocpSock, IocpSock->peer_ip, IocpSock->peer_port);
			proto->_protolock->unlock();
			PostRecv(IocpSock, IocpBuff, proto);
		}
		else
		{
			Close(IocpSock, IocpBuff);
		}
		break;
	default:
		break;
	}
	return true;
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
			if (WAIT_TIMEOUT == GetLastError())
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
	for (unsigned int i = 0; i < reactor->CPU_COUNT; i++)
	//for (unsigned int i = 0; i < 1; i++)
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
		std::map<uint16_t, BaseFactory*>::iterator iter;
		for (iter = reactor->FactoryAll.begin(); iter != reactor->FactoryAll.end(); ++iter)
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

	HANDLE ThreadHandle = CreateThread(NULL, 0, mainIOCPServer, reactor, 0, NULL);
	if (NULL == ThreadHandle) {
		return -4;
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

		IOCP_SOCKET* IcpSock = NewIOCP_Socket();
		if (IcpSock == NULL)
		{
			closesocket(fc->sListen);
			return -2;
		}
		IcpSock->factory = fc;
		IcpSock->sock = fc->sListen;

		CreateIoCompletionPort((HANDLE)fc->sListen, fc->reactor->ComPort, (ULONG_PTR)IcpSock, 0);
		for (DWORD i = 0; i < fc->reactor->CPU_COUNT; i++)
			PostAcceptClient(fc);
	}
	
	fc->reactor->FactoryAll.insert(std::pair<short, BaseFactory*>(fc->ServerPort, fc));
	return 0;
}

int IOCPFactoryStop(BaseFactory* fc)
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
	IocpSock->sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (IocpSock->sock == INVALID_SOCKET)
		return false;

	sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;
	bind(IocpSock->sock, (sockaddr*)(&local_addr), sizeof(sockaddr_in));

	DWORD flags = 0;
	if (ResetIocp_Buff(IocpSock, IocpBuff) == false)
	{
		closesocket(IocpSock->sock);
		return false;
	}

	CreateIoCompletionPort((HANDLE)IocpSock->sock, fc->reactor->ComPort, (ULONG_PTR)IocpSock, 0);

	int fromlen = sizeof(struct sockaddr);
	IocpBuff->type = READ;

	if (SOCKET_ERROR == WSARecvFrom(IocpSock->sock, &IocpBuff->databuf, 1, NULL, &flags, (sockaddr*)&IocpSock->peer_addr, &fromlen, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			closesocket(IocpSock->sock);
			return false;
		}
	}
	return true;
}

static bool IOCPConnectTCP(BaseFactory* fc, IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	IocpSock->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (IocpSock->sock == INVALID_SOCKET)
		return false;

	setsockopt(IocpSock->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&(fc->sListen), sizeof(fc->sListen));

	sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;
	bind(IocpSock->sock, (sockaddr*)(&local_addr), sizeof(sockaddr_in));

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
			return false;
		}
	}
	CreateIoCompletionPort((HANDLE)IocpSock->sock, fc->reactor->ComPort, (ULONG_PTR)IocpSock, 0);

	PVOID lpSendBuffer = NULL;
	DWORD dwSendDataLength = 0;
	DWORD dwBytesSent = 0;
	BOOL bResult = lpfnConnectEx(IocpSock->sock,
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
			closesocket(IocpSock->sock);
			return false;
		}
	}
	return true;
}

HSOCKET IOCPConnectEx(BaseProtocol* proto, const char* ip, int port, CONN_TYPE iotype)
{
	if (proto == NULL || (proto->_sockCount == 0 && proto->_protoType == SERVER_PROTOCOL))
		return NULL;
	BaseFactory* fc = proto->_factory;
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
	IocpSock->_user = &(proto->_self);
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
	proto->_sockCount += 1;
	return IocpSock;
}

static bool IOCPPostSendUDPEx(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	DWORD flags = 0;
	if (SOCKET_ERROR == WSASendTo(IocpSock->sock, &IocpBuff->databuf, 1, NULL, flags, (sockaddr*)&IocpSock->peer_addr, sizeof(IocpSock->peer_addr), &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
			return false;
	}
	return true;
}

static bool IOCPPostSendTCPEx(IOCP_SOCKET* IocpSock, IOCP_BUFF* IocpBuff)
{
	DWORD flags = 0;
	if (SOCKET_ERROR == WSASend(IocpSock->sock, &IocpBuff->databuf, 1, NULL, flags, &IocpBuff->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
			return false;
	}
	return true;
}

bool IOCPPostSendEx(IOCP_SOCKET* IocpSock, const char* data, int len)    //注意此方法存在内存泄漏风险，如果此投递未返回时socket被关闭
{
	if (IocpSock == NULL)
		return false;
	
	IOCP_BUFF* IocpBuff = NewIOCP_Buff();
	if (IocpBuff == NULL)
		return false;

	IocpBuff->databuf.buf = (char*)sheeps_malloc(len);
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
		sheeps_free(IocpBuff->databuf.buf);
		ReleaseIOCP_Buff(IocpBuff);
		return false;
	}
	IocpSock->heartbeat = time(NULL);
	return true;
}

bool IOCPCloseHsocket(IOCP_SOCKET*  IocpSock)
{
	if (IocpSock == NULL ||IocpSock->sock == INVALID_SOCKET)
		return false;
	closesocket(IocpSock->sock);
	return true;
}

int IOCPSkipHsocketBuf(IOCP_SOCKET* IocpSock, int len)
{
	IocpSock->_IocpBuff->offset -= len;
	memmove(IocpSock->recv_buf, IocpSock->recv_buf + len, IocpSock->_IocpBuff->offset);
	return IocpSock->_IocpBuff->offset;
}