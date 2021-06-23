/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _SERVER_PROTOCOL_H_
#define _SERVER_PROTOCOL_H_
#include "SheepsStruct.h"
#include "Reactor.h"
#include "SheepsMemory.h"
#include <list>
#include <mutex>

enum
{
	PEER_UNNOWN = 0,
	PEER_UNDEFINE,
	PEER_PROXY,
	PEER_STRESS,
	PEER_CONSOLE,
};

enum
{
	PROXY_INIT = 0,
	PROXY_AUTHED,
	PROXY_CONNECTING,
	PROXY_CONNECTED
};

enum {
	AGENT_DEFAULT = 0,
	AGENT_READY,
	AGENT_FAIL
};

typedef struct {
	const char* ip;
	int			port;
	short		cpu;
	int			ready;
	uint8_t		projectid;
}t_client_info, * HCLIENTINFO;

typedef struct {
	char	serverip[16];
	int		serverport;
	uint8_t proxytype;

	char	clientip[16];
	int		clientport;
	char	tempmsg[16];

	HSOCKET udpClientSock;
	HSOCKET proxySock;
	int		proxyStat;
	int		retry;
}t_proxy_info, * HPROXYINFO;

extern int slogid;

class ServerProtocol :
	public BaseProtocol
{
public:
	HSOCKET initSock = NULL;
	int peerType = PEER_UNNOWN;
	
	HPROXYINFO proxyInfo = NULL;	//proxy
	HCLIENTINFO stressInfo = NULL;	//stress

public:
	ServerProtocol();
	~ServerProtocol();

public:
	void ConnectionMade(HSOCKET hsock);
	void ConnectionFailed(HSOCKET hsock, int err);
	void ConnectionClosed(HSOCKET hsock, int err);
	void ConnectionRecved(HSOCKET hsock, const char* data, int len);

	void CheckReq(HSOCKET hsock, const char* ip, int port, const char* data, int len);
	int  CheckRequest(HSOCKET hsock, const char* ip, int port, const char* data, int len);
};

bool ServerInit(char* configfile);

#endif // !_SERVER_PROTOCOL_H_