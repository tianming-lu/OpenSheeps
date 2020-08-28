#include "pch.h"
#include "ws2tcpip.h"
#include "ServerProtocol.h"
#include "ServerProxy.h"
#include "ServerConsole.h"
#include "ServerStress.h"

int slogid = -1;

void ServerInit(char* configfile)
{
	char file[128] = { 0x0 };
	GetPrivateProfileStringA("LOG", "server_file", "server.log", file, sizeof(file), configfile);
	char fullpath[256] = { 0x0 };
	snprintf(fullpath, sizeof(fullpath), "%s\\%s", LogPath, file);
	int loglevel = GetPrivateProfileIntA("LOG", "server_level", 0, configfile);
	int maxsize = GetPrivateProfileIntA("LOG", "serveer_size", 20, configfile);
	int timesplit = GetPrivateProfileIntA("LOG", "server_time", 3600, configfile);
	slogid = RegisterLog(fullpath, loglevel, maxsize, timesplit, 5);
}

ServerProtocol::ServerProtocol()
{
}


ServerProtocol::~ServerProtocol()
{
	sheeps_free(this->proxyInfo);
	sheeps_free(this->stressInfo);
}


//���к������̳��Ի���
void ServerProtocol::ConnectionMade(HSOCKET hsock, const char* ip, int port)
{
	//LOG(slogid, LOG_DEBUG, "%s:%d %s:%d\r\n", __func__, __LINE__, ip, port);
	switch (this->peerType)
	{
	case PEER_UNNOWN:
		this->initSock = hsock;
		return;
	case PEER_PROXY:
		ProxyConnectionMade(hsock, this, ip, port);
		return;
	default:
		break;
	}
	//LOG(slogid, LOG_DEBUG, "new unnkown connetion made: %s:%d\r\n", ip, port);
	IOCPCloseHsocket(hsock);
}

void ServerProtocol::ConnectionFailed(HSOCKET hsock, const char* ip, int port)
{
	//LOG(slogid, LOG_DEBUG, "connetion failed: %s:%d  [%d]\r\n", ip, port, this->sockCount);
	switch (this->peerType)
	{
	case PEER_PROXY:
		ProxyConnectionFailed(hsock, this, ip, port);
		return;
	default:
		break;
	}
	IOCPCloseHsocket(this->initSock);
}

void ServerProtocol::ConnectionClosed(HSOCKET hsock, const char* ip, int port)
{
	//LOG(slogid, LOG_DEBUG, "%s:%d %s:%d\r\n", __func__, __LINE__, ip, port);
	if (this->peerType == PEER_PROXY)
	{
		ProxyConnectionClosed(hsock, this, ip, port);
	}
	else if (this->peerType == PEER_STRESS)
	{
		StressConnectionClosed(hsock, this);
	}
	else if (this->peerType == PEER_CONSOLE || this->peerType == PEER_UNNOWN)
	{
		this->initSock = NULL;
	}
}

void ServerProtocol::Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len)
{
	return this->CheckReq(hsock, ip, port, data, len);
}

//�Զ������Ա����

void ServerProtocol::CheckReq(HSOCKET hsock, const char* ip, int port, const char* data, int len)
{
	if (hsock == this->initSock)
	{
		int clen = 0;
		while (len)
		{
			clen = this->CheckRequest(hsock, ip, port, data, len);
			if (clen > 0)
			{
				len = IOCPSkipHsocketBuf(hsock, clen);
			}
			else if (clen < 0)
				IOCPCloseHsocket(hsock);
			else
				break;
		}
	}
	else
	{
		switch (this->peerType)
		{
		case PEER_PROXY:
			CheckPoxyRequest(hsock, this, ip, port, data, len);
		default:
			break;
		}
		IOCPSkipHsocketBuf(hsock, len);
	}
	return;
}

#ifdef _MSC_VER
static int strncasecmp(const char* input_buffer, const char* s2, int n)
{
	return _strnicmp(input_buffer, s2, n);
}
#endif

int ServerProtocol::CheckRequest(HSOCKET hsock, const char* ip, int port, const char* data, int len)
{
	int clen = -1;
	do_switch:
	switch (this->peerType)
	{
	case PEER_UNNOWN:
		if (*data == 0x5)
		{
			this->peerType = PEER_PROXY;
			this->proxyInfo = (HPROXYINFO)sheeps_malloc(sizeof(t_proxy_info));
			if (this->proxyInfo == NULL)
				return -1;
			this->proxyInfo->retry = 3;
		}
		else if (strncasecmp(data, "GET", 3) == 0 || strncasecmp(data, "POST", 4) == 0)
			this->peerType = PEER_CONSOLE;
		else
		{
			this->peerType = PEER_STRESS;
			this->stressInfo = (HCLIENTINFO)sheeps_malloc(sizeof(t_client_info));
			if (this->stressInfo == NULL)
				return -1;
		}
		goto do_switch;
	case PEER_PROXY:
		clen = CheckPoxyRequest(hsock, this, ip, port, data, len);
		break;
	case PEER_STRESS:
		clen = CheckStressRequest(hsock, this, data, len);
		break;
	case PEER_CONSOLE:
		clen = CheckConsoleRequest(hsock, this, data, len);
	default:
		break;
	}
	return clen;
}
