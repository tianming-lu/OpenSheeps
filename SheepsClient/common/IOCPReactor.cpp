#include "pch.h"
#include "framework.h"
#include "IOCPReactor.h"
#include <Ws2tcpip.h>
#include <map>

#pragma comment(lib, "Ws2_32.lib")	

SOCKET get_listen_sock(int port)
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

BOOL AcceptClient(BaseFactory* fc)
{
	fd_operation_data* pIO;
	pIO = (fd_operation_data*)GlobalAlloc(GPTR, sizeof(fd_operation_data));
	if (pIO == NULL)
	{
		printf("内存分配失败\n");
		return FALSE;
	}
	pIO->databuf.buf = pIO->recv_buffer;
	pIO->databuf.len = DATA_BUFSIZE;
	pIO->type = ACCEPT;

	pIO->sock = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (pIO->sock == INVALID_SOCKET)
	{
		printf("创建套接字失败\n");
		GlobalFree(pIO);
		return FALSE;
	}

	/*调用AcceptEx函数，地址长度需要在原有的上面加上16个字节
	向服务线程投递一个接收连接的的请求*/
	BOOL rc = fc->lpfnAcceptEx(fc->sListen, pIO->sock,
		pIO->recv_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		&pIO->databuf.len, &(pIO->overlapped));

	if (FALSE == rc)
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)
		{
			printf("发起新连接失败%d", WSAGetLastError());
			GlobalFree(pIO);
			return FALSE;
		}
		//LOG("发起新连接异常%d", WSAGetLastError());
	}
	//LOG("发起新连接成功%d", WSAGetLastError());
	return TRUE;
}

BOOL Close(t_completion_fd* pComKey, fd_operation_data* pIOoperData)
{
	if (pComKey != NULL)
	{
		//LOG("关闭连接 %d %d %lld %d", pIOoperData->type, WSAGetLastError(), pComKey, pComKey->sock);
		if (ACCEPT == pIOoperData->type)
		{
			GlobalFree(pIOoperData);
			AcceptClient(pComKey->factory);
			return TRUE;
		}
		else if (WRITEEX == pIOoperData->type)
		{
			if (pIOoperData->send_buffer != NULL)
				GlobalFree(pIOoperData->send_buffer);
			GlobalFree(pIOoperData);
			return TRUE;
		}
		else if (CONNECT == pIOoperData->type)
		{
			//LOG("关闭连接 %d %d", pComKey->sock, WSAGetLastError());
			if (pComKey->sock != INVALID_SOCKET  && *(pComKey->user) != NULL)
			{
				pComKey->userlock->lock();
				if (pComKey->sock != INVALID_SOCKET && *(pComKey->user) != NULL)
				{
					(*(pComKey->user))->sockCount -= 1;
					(*(pComKey->user))->ConnectionFailed(pComKey, pComKey->IP, pComKey->PORT);
				}
				pComKey->userlock->unlock();
			}
		}
		else if(READ == pIOoperData->type)
		{
			if (pComKey->sock != INVALID_SOCKET && *(pComKey->user) != NULL)
			{
				pComKey->userlock->lock();
				if (*(pComKey->user) != NULL && pComKey->sock != INVALID_SOCKET)
				{
					(*(pComKey->user))->sockCount -= 1;
					(*(pComKey->user))->ConnectionClosed(pComKey, pComKey->IP, pComKey->PORT);
				}
				pComKey->userlock->unlock();
			}
		}

		if (pComKey->sock != INVALID_SOCKET)
		{
			CancelIo((HANDLE)pComKey->sock);	//取消等待执行的异步操作
			closesocket(pComKey->sock);
			pComKey->sock = INVALID_SOCKET;
		}
		GlobalFree(pIOoperData);
		GlobalFree(pComKey);
	}
	MemoryBarrier();
	return TRUE;
}

BOOL PostRecv(t_completion_fd* pComKey, fd_operation_data* pIOoperData)
{
	DWORD flags = 0;
	ZeroMemory(&pIOoperData->overlapped, sizeof(OVERLAPPED));
	ZeroMemory(&pIOoperData->recv_buffer, DATA_BUFSIZE);

	pIOoperData->type = READ;
	pIOoperData->databuf.buf = pIOoperData->recv_buffer;
	pIOoperData->databuf.len = DATA_BUFSIZE;

	if (SOCKET_ERROR == WSARecv(pComKey->sock, &pIOoperData->databuf, 1, NULL, &flags, &pIOoperData->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			//LOG("发起重叠接收失败!   %d\n", GetLastError());
			Close(pComKey, pIOoperData);
			return FALSE;
		}
		//LOG("发起重叠接收异常!   %d\n", GetLastError());
	}
	return TRUE;
}


BOOL PostSend(t_completion_fd* pComKey, fd_operation_data* pIOoperData)
{
	DWORD flags = 0;
	pIOoperData->databuf.len = pIOoperData->buffer_len;
	ZeroMemory(&pIOoperData->overlapped, sizeof(OVERLAPPED));

	pIOoperData->type = WRITE;

	if (SOCKET_ERROR == WSASend(pComKey->sock, &pIOoperData->databuf, 1, NULL, flags, &pIOoperData->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			//LOG("发起重叠发送失败!\n");
			Close(pComKey, pIOoperData);
			return FALSE;
		}
		//LOG("发起发送重叠接收异常!\n");
	}
	return TRUE;
}

BOOL PostAceept(t_completion_fd* pComKey, fd_operation_data* pIOoperData)
{
	BaseFactory* fc = pComKey->factory;
	Reactor* reactor = fc->reactor;
	setsockopt(pIOoperData->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) & (pComKey->sock), sizeof(pComKey->sock));

	t_completion_fd* pClientComKey = (t_completion_fd*)GlobalAlloc(GPTR, sizeof(t_completion_fd));
	if (pClientComKey == NULL)
	{
		printf("内存分配失败\n");
		return FALSE;
	}
	memset(pClientComKey, 0, sizeof(t_completion_fd));
	pClientComKey->sock = pIOoperData->sock;
	BaseProtocol* proto = fc->CreateProtocol();	//用户指针
	proto->self = proto;
	proto->factory = fc;
	pClientComKey->user = &(proto->self);	//用户指针的地址
	pClientComKey->userlock = proto->protolock;
	pClientComKey->lpIOoperData = pIOoperData;

	SOCKADDR_IN addr_conn;
	int nSize = sizeof(addr_conn);
	getpeername(pClientComKey->sock, (SOCKADDR*)&addr_conn, &nSize);
	inet_ntop(AF_INET, &addr_conn.sin_addr, pClientComKey->IP, 20);
	pClientComKey->PORT = ntohs(addr_conn.sin_port);

	//(*(pClientComKey->user))->protolock->lock();
	//(*(pClientComKey->user))->ConnectionMade(pClientComKey, pClientComKey->IP, pClientComKey->PORT);
	//(*(pClientComKey->user))->protolock->unlock();

	SOCKADDR_IN* addrClient = NULL, * addrLocal = NULL;
	int nClientLen = sizeof(SOCKADDR_IN), nLocalLen = sizeof(SOCKADDR_IN);

	fc->lpfnGetAcceptExSockaddrs(pIOoperData->recv_buffer, 0,
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
		(LPSOCKADDR*)& addrLocal, &nLocalLen,
		(LPSOCKADDR*)& addrClient, &nClientLen);

	CreateIoCompletionPort((HANDLE)pClientComKey->sock, reactor->g_hComPort, (ULONG_PTR)pClientComKey, 0);	//将监听到的套接字关联到完成端口
	proto->sockCount += 1;
	(*(pClientComKey->user))->ConnectionMade(pClientComKey, pClientComKey->IP, pClientComKey->PORT);

	PostRecv(pClientComKey, pIOoperData);

	AcceptClient(pComKey->factory);
	return 0;
}

BOOL ProcessIO(fd_operation_data* pIOoperData, t_completion_fd* pComKey)
{

	if (pIOoperData->type == READ)	//消息接受后动作
	{
		if (pComKey->sock != INVALID_SOCKET && *(pComKey->user) != NULL)
		{
			int ret = CLOSE;
			pComKey->userlock->lock();
			if (pComKey->sock != INVALID_SOCKET && *(pComKey->user) != NULL)
				ret = (*(pComKey->user))->Recv(pComKey, pComKey->IP, pComKey->PORT, pIOoperData->recv_buffer, pIOoperData->buffer_len);
			pComKey->userlock->unlock();
			switch (ret)
			{
			case RECV:
				PostRecv(pComKey, pIOoperData);
				break;
			case SEND:
				PostSend(pComKey, pIOoperData);
				break;
			case SEND_CLOSE:
				pIOoperData->close = true;
				PostSend(pComKey, pIOoperData);
				break;
			case CLOSE:
				Close(pComKey, pIOoperData);
				break;
			default:
				break;
			}
		}
	}
	else if (pIOoperData->type == WRITE)	//发送结束后动作
	{
		if (pIOoperData->close == true)
			Close(pComKey, pIOoperData);
		else
			PostRecv(pComKey, pIOoperData);
	}
	else if (pIOoperData->type == WRITEEX)	//异步主动发送结束后动作
	{
		if (pIOoperData->send_buffer != NULL)
			GlobalFree(pIOoperData->send_buffer);
		GlobalFree(pIOoperData);
	}
	else if (pIOoperData->type == ACCEPT)	//新连接接入后动作
	{
		PostAceept(pComKey, pIOoperData);
	}
	else if (pIOoperData->type == CONNECT)
	{
		if (pComKey->sock != INVALID_SOCKET && *(pComKey->user) != NULL)
		{
			pComKey->userlock->lock();
			if (pComKey->sock != INVALID_SOCKET && *(pComKey->user) != NULL)
				(*(pComKey->user))->ConnectionMade(pComKey, pComKey->IP, pComKey->PORT);
			pComKey->userlock->unlock();
			PostRecv(pComKey, pIOoperData);
		}
		else
		{
			Close(pComKey, pIOoperData);
		}
	}
	return TRUE;
}

/////////////////////////////////////////////////////////////////////////
//服务线程
DWORD WINAPI serverWorkerThread(LPVOID pParam)
{
	Reactor* reactor = (Reactor*)pParam;

	DWORD	dwIoSize;
	t_completion_fd* pComKey;
	fd_operation_data* lpIOoperData;		//IO数据,用于发起接收重叠操作
	BOOL bRet;
	while (TRUE)
	{
		bRet = FALSE;
		dwIoSize = -1;	//IO操作长度
		lpIOoperData = NULL;
		bRet = GetQueuedCompletionStatus(reactor->g_hComPort, &dwIoSize, (PULONG_PTR)&pComKey, (LPOVERLAPPED*)&lpIOoperData, INFINITE);
		if (bRet == FALSE)
		{
			DWORD dwIOError = GetLastError();
			if (WAIT_TIMEOUT == dwIOError)
			{
				continue;
			}
			else if (NULL != lpIOoperData)
			{
				Close(pComKey, lpIOoperData);
			}
		}
		else
		{
			if (0 == dwIoSize && (READ == lpIOoperData->type || WRITE == lpIOoperData->type || WRITEEX == lpIOoperData->type))
			{
				Close(pComKey, lpIOoperData);
				continue;
			}
			else
			{
				lpIOoperData->buffer_len = dwIoSize;
				ProcessIO(lpIOoperData, pComKey);
			}
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
			printf("创建工作线程失败！");
			return -4;
		}
		CloseHandle(ThreadHandle);
	}
	while (reactor->g_Run)
	{
		map<short, BaseFactory*>::iterator iter;
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
		printf("加载套接字库失败!   %d\n", WSAGetLastError());
		return SOCKET_ERROR;
	}

	reactor->g_hComPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (reactor->g_hComPort == NULL)
	{
		printf("Create completionport error!   %d\n", WSAGetLastError());
		return -2;
	}

	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	reactor->CPU_COUNT = sysInfor.dwNumberOfProcessors;

	HANDLE ThreadHandle = CreateThread(NULL, 0, mainIOCPServer, reactor, 0, NULL);
	if (NULL == ThreadHandle) {
		return -3;
	}
	CloseHandle(ThreadHandle);
	return 0;
}

void IOCPServerStop(Reactor* reactor)
{
	reactor->g_Run = FALSE;
}

int IOCPFactoryRun(BaseFactory* fc)
{	
	if (fc->ServerPort != 0)
	{
		fc->sListen = get_listen_sock(fc->ServerPort);
		if (fc->sListen == SOCKET_ERROR)
			return -1;

		//使用WSAIoctl获取AcceptEx函数指针
		DWORD dwbytes = 0;
		//Accept function GUID
		GUID guidAcceptEx = WSAID_ACCEPTEX;
		if (0 != WSAIoctl(fc->sListen, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidAcceptEx, sizeof(guidAcceptEx),
			&fc->lpfnAcceptEx, sizeof(fc->lpfnAcceptEx),
			&dwbytes, NULL, NULL))
		{
			return -2;
		}

		// 获取GetAcceptExSockAddrs函数指针，也是同理
		GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
		if (0 != WSAIoctl(fc->sListen, SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guidGetAcceptExSockaddrs,
			sizeof(guidGetAcceptExSockaddrs),
			&fc->lpfnGetAcceptExSockaddrs,
			sizeof(fc->lpfnGetAcceptExSockaddrs),
			&dwbytes, NULL, NULL))
		{
			return -3;
		}

		t_completion_fd* pListenComKey = (t_completion_fd*)GlobalAlloc(GPTR, sizeof(t_completion_fd));
		if (pListenComKey == NULL)
		{
			closesocket(fc->sListen);
			return -4;
		}
		pListenComKey->factory = fc;
		pListenComKey->sock = fc->sListen;

		CreateIoCompletionPort((HANDLE)fc->sListen, fc->reactor->g_hComPort, (ULONG_PTR)pListenComKey, 0);  //注意：此处可能存在多余操作
		for (DWORD i = 0; i < fc->reactor->CPU_COUNT; i++)
			AcceptClient(fc);
	}
	fc->FactoryInit();
	fc->reactor->FactoryAll.insert(pair<short, BaseFactory*>(fc->ServerPort, fc));
	return 0;
}

int IOCPFactoryStop(BaseFactory* fc)
{
	map<short, BaseFactory*>::iterator iter;
	iter = fc->reactor->FactoryAll.find(fc->ServerPort);
	if (iter != fc->reactor->FactoryAll.end())
	{
		fc->reactor->FactoryAll.erase(iter);
	}
	fc->FactoryClose();
	return 0;
}

/*SOCKET IOCPConnect(const char* ip, int port, BaseProtocol** proto)
{
	BaseFactory* fc = (*proto)->factory;
	SOCKET sockClt = socket(AF_INET, SOCK_STREAM, 0);
	if (sockClt == INVALID_SOCKET)
	{
		printf("socket() fail:%d\n", WSAGetLastError());
		return NULL;
	}

	SOCKADDR_IN addrSrv;
	memset(&addrSrv, 0, sizeof(addrSrv));
	addrSrv.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &addrSrv.sin_addr);
	addrSrv.sin_port = htons(port);

	int err = connect(sockClt, (SOCKADDR*)& addrSrv, sizeof(SOCKADDR));
	if (err == -1)
	{
		(*proto)->ConnectionFailed((char*)ip, port);
		return NULL;
	}

	fd_operation_data* pIOoperData;
	t_completion_fd* pClientComKey;
	pIOoperData = (fd_operation_data*)GlobalAlloc(GPTR, sizeof(fd_operation_data));
	pClientComKey = (t_completion_fd*)GlobalAlloc(GPTR, sizeof(t_completion_fd));
	if (pClientComKey == NULL || pIOoperData == NULL)
	{
		printf("内存分配失败\n");
		closesocket(sockClt);
		return NULL;
	}
	memset(pClientComKey, 0, sizeof(t_completion_fd));
	memset(pIOoperData, 0, sizeof(fd_operation_data));
	pIOoperData->sock = sockClt;
	pClientComKey->sock = sockClt;
	memcpy(pClientComKey->IP, ip, strlen(ip));
	pClientComKey->PORT = port;
	pClientComKey->lpIOoperData = pIOoperData;

	//setsockopt(pIOoperData->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) & (pClientComKey->sock), sizeof(pClientComKey->sock));
	pClientComKey->user = proto;
	//pClientComKey->user->sock = pClientComKey->sock;
	(*(pClientComKey->user))->ConnectionMade(pClientComKey, pClientComKey->IP, pClientComKey->PORT);

	CreateIoCompletionPort((HANDLE)pClientComKey->sock, fc->reactor->g_hComPort, (ULONG_PTR)pClientComKey, 0);	//将监听到的套接字关联到完成端口
	PostRecv(pClientComKey, pIOoperData);

	return sockClt;
}*/

HSOCKET IOCPConnectEx(const char* ip, int port, BaseProtocol* proto)
{
	BaseFactory* fc = proto->factory;
	fd_operation_data* pIO;
	pIO = (fd_operation_data*)GlobalAlloc(GPTR, sizeof(fd_operation_data));
	if (pIO == NULL)
	{
		printf("内存分配失败\n");
		return NULL;
	}
	pIO->databuf.buf = pIO->recv_buffer;
	pIO->databuf.len = DATA_BUFSIZE;
	pIO->type = CONNECT;
	pIO->overlapped.hEvent = NULL;
	//pIO->sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, NULL, WSA_FLAG_OVERLAPPED);
	pIO->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (pIO->sock == INVALID_SOCKET)
	{
		printf("socket create fail:%d\n", WSAGetLastError());
		closesocket(pIO->sock);
		GlobalFree(pIO);
		return NULL;
	}
	setsockopt(pIO->sock, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*) & (fc->sListen), sizeof(fc->sListen));

	sockaddr_in local_addr;
	ZeroMemory(&local_addr, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;

	int irt = bind(pIO->sock, (sockaddr*)(&local_addr), sizeof(sockaddr_in));
	t_completion_fd* pClientComKey = (t_completion_fd*)GlobalAlloc(GPTR, sizeof(t_completion_fd));
	if (pClientComKey == NULL)
	{
		printf("内存分配失败\n");
		closesocket(pIO->sock);
		GlobalFree(pIO);
		return NULL;
	}
	memset(pClientComKey, 0, sizeof(t_completion_fd));
	pClientComKey->sock = pIO->sock;
	memcpy(pClientComKey->IP, ip, strlen(ip));
	pClientComKey->PORT = port;
	pClientComKey->user = &(proto->self);
	pClientComKey->userlock = proto->protolock;
	pClientComKey->lpIOoperData = pIO;
	//LOG("sockex is made  %d. Error code = %d\n", pIO->sock, WSAGetLastError());

	SOCKADDR_IN addrSrv;
	memset(&addrSrv, 0, sizeof(addrSrv));
	addrSrv.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &addrSrv.sin_addr);
	addrSrv.sin_port = htons(port);

	LPFN_CONNECTEX lpfnConnectEx = NULL;
	GUID GuidConnectEx = WSAID_CONNECTEX;
	DWORD dwBytes = 0;
	if (SOCKET_ERROR == WSAIoctl(pClientComKey->sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
		&GuidConnectEx, sizeof(GuidConnectEx),
		&lpfnConnectEx, sizeof(lpfnConnectEx), &dwBytes, 0, 0))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			printf("WSAIoctl is failed  %lld. Error code = %d\n", pIO->sock, WSAGetLastError());
			closesocket(pClientComKey->sock);
			GlobalFree(pIO);
			GlobalFree(pClientComKey);
			return NULL;
		}
		//LOG("WSAIoctl fate err. Error code = %d\n", WSAGetLastError());
	}
	CreateIoCompletionPort((HANDLE)pClientComKey->sock, fc->reactor->g_hComPort, (ULONG_PTR)pClientComKey, 0);	//将监听到的套接字关联到完成端口
	proto->sockCount += 1;

	PVOID lpSendBuffer = NULL;
	DWORD dwSendDataLength = 0;
	DWORD dwBytesSent = 0;
	BOOL bResult = lpfnConnectEx(pIO->sock,
		(SOCKADDR*)&addrSrv,	// [in] 对方地址
		sizeof(addrSrv),		// [in] 对方地址长度
		lpSendBuffer,			// [in] 连接后要发送的内容，这里不用
		dwSendDataLength,		// [in] 发送内容的字节数 ，这里不用
		&dwBytesSent,			// [out] 发送了多少个字节，这里不用
		&(pIO->overlapped));	// [in] 这东西复杂，下一篇有详解

	if (!bResult)      // 返回值处理
	{
		if (WSAGetLastError() != ERROR_IO_PENDING)   // 调用失败
		{
			//printf("ConnextEx error:%lld  %d  %lld\n", pIO->sock, WSAGetLastError(), pClientComKey);
			closesocket(pClientComKey->sock);
			GlobalFree(pIO);
			GlobalFree(pClientComKey);    //已经注册到IOCP后，由IOCP释放资源，主动释放会造成程序崩溃
			return NULL;
		}
		//LOG("ConnextEx fate error: %d\n", WSAGetLastError());
	}
	return pClientComKey;
}

bool IOCPPostSendEx(t_completion_fd* pComKey, char* data, int len)    //注意此方法存在内存泄漏风险，如果此投递未返回时socket被关闭
{
	if (pComKey == NULL)
	{
		return FALSE;
	}

	DWORD flags = 0;
	fd_operation_data* pIOoperData = (fd_operation_data*)GlobalAlloc(GPTR, sizeof(fd_operation_data));
	if (pIOoperData == NULL)
	{
		printf("内存分配失败\n");
		return false;
	}

	pIOoperData->send_buffer = (char*)GlobalAlloc(GPTR, len);
	if (pIOoperData->send_buffer == NULL)
	{
		GlobalFree(pIOoperData);
		return false;
	}
	memcpy(pIOoperData->send_buffer, data, len);

	pIOoperData->databuf.len = len;
	pIOoperData->databuf.buf = pIOoperData->send_buffer;
	ZeroMemory(&pIOoperData->overlapped, sizeof(OVERLAPPED));

	//pIOoperData->type = WRITE;
	pIOoperData->type = WRITEEX;

	if (SOCKET_ERROR == WSASend(pComKey->sock, &pIOoperData->databuf, 1, NULL, flags, &pIOoperData->overlapped, NULL))
	{
		if (ERROR_IO_PENDING != WSAGetLastError())
		{
			//LOG("发起重叠发送失败!\n");
			GlobalFree(pIOoperData->send_buffer);
			GlobalFree(pIOoperData);
			return false;
		}
		//LOG("发起发送重叠接收异常!\n");
	}
	return true;
}

bool IOCPCloseHsocket(t_completion_fd*  pComKey)
{
	if (pComKey == NULL ||pComKey->sock == INVALID_SOCKET)
		return false;
	closesocket(pComKey->sock);
	return true;
}

bool IOCPDestroyProto(BaseProtocol* proto)
{
	if (proto->sockCount > 0)
		return false;
	BaseFactory* fc = proto->factory;
	fc->DeleteProtocol(proto->self);
	return true;
}