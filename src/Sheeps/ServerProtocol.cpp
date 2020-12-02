#include "pch.h"
#include "ServerProtocol.h"
#include "ServerProxy.h"
#include "ServerConsole.h"
#include "ServerStress.h"

int slogid = -1;

bool ServerInit(char* configfile)
{
	const char* file = config_get_string_value("LOG", "server_file", "server.log");
	char fullpath[256] = { 0x0 };
	snprintf(fullpath, sizeof(fullpath), "%s%s", LogPath, file);
	int loglevel = config_get_int_value("LOG", "server_level", 0);
	int maxsize = config_get_int_value("LOG", "serveer_size", 20);
	int timesplit = config_get_int_value("LOG", "server_time", 3600);
	slogid = RegisterLog(fullpath, loglevel, maxsize, timesplit, 5);
	return true;
}

ServerProtocol::ServerProtocol()
{
}


ServerProtocol::~ServerProtocol()
{
	if (this->proxyInfo)sheeps_free(this->proxyInfo);
	if (this->stressInfo)sheeps_free(this->stressInfo);
}


//固有函数，继承自基类
void ServerProtocol::ConnectionMade(HSOCKET hsock, const char* ip, int port)
{
	LOG(slogid, LOG_DEBUG, "%s:%d %p\r\n", __func__, __LINE__, hsock);
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
	HsocketClose(hsock);
}

void ServerProtocol::ConnectionFailed(HSOCKET hsock, const char* ip, int port)
{
	LOG(slogid, LOG_DEBUG, "%s:%d %p\r\n", __func__, __LINE__, hsock);
	switch (this->peerType)
	{
	case PEER_PROXY:
		ProxyConnectionFailed(hsock, this, ip, port);
		return;
	default:
		break;
	}
	HsocketClose(this->initSock);
}

void ServerProtocol::ConnectionClosed(HSOCKET hsock, const char* ip, int port)
{
	LOG(slogid, LOG_DEBUG, "%s:%d %p\r\n", __func__, __LINE__, hsock);
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

//自定义类成员函数

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
				len = HsocketSkipBuf(hsock, clen);
			}
			else if (clen < 0)
			{
				HsocketClose(hsock);
				break;
			}
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
		HsocketSkipBuf(hsock, len);
	}
	return;
}

#ifdef __WINDOWS__
static inline int strncasecmp(const char* input_buffer, const char* s2, int n)
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
		else if (strncasecmp(data, "GET", 3) == 0 || strncasecmp(data, "POST", 4) == 0 || strncasecmp(data, "HEAD", 4) == 0 ||
			strncasecmp(data, "PUT", 3) == 0 || strncasecmp(data, "DELETE", 6) == 0 || strncasecmp(data, "CONNECT", 7) == 0 ||
			strncasecmp(data, "OPTIONS", 7) == 0 || strncasecmp(data, "TRACE", 5) == 0 || strncasecmp(data, "PATCH", 5) == 0)
		{
			this->peerType = PEER_CONSOLE;
		}
		else if (strncasecmp(data, "sheeps", 6) == 0)
		{
			this->peerType = PEER_STRESS;
			this->stressInfo = (HCLIENTINFO)sheeps_malloc(sizeof(t_client_info));
			if (this->stressInfo == NULL)
				return -1;
			len = HsocketSkipBuf(hsock, 6);
		}
		else
		{
			this->peerType = PEER_UNDEFINE;
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
	case PEER_UNDEFINE:
		break;
	default:
		break;
	}
	//LOG(slogid, LOG_DEBUG, "%p peer_type[%d]\r\n", hsock, this->peerType);
	return clen;
}
