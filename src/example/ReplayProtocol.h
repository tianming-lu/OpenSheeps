#pragma once
#include "./../Sheeps/TaskManager.h"
#include <map>

typedef struct {
	HSOCKET hsock;
}t_connection_info;

class UserProtocol :
	public ReplayProtocol
{
public:
	UserProtocol();
	~UserProtocol();

public:
	std::map<int, t_connection_info> Connection;

public:
	void EventInit();
	void ConnectionMade(HSOCKET hsock);
	void ConnectionFailed(HSOCKET hsock);
	void ConnectionClosed(HSOCKET hsock);
	void ConnectionRecved(HSOCKET hsock, const char* data, int len);
	void EventConnectOpen(const char* ip, int port, bool udp);
	void EventConnectClose(const char* ip, int port, bool udp);
	void EventSend(const char* ip, int port, const char* content, int clen, bool udp);
	void EventTimeOut();
	void EventReInit();
	void EventDestroy();

	HSOCKET* GetScokFromConnection(const char* ip, int port);
	bool CloseAllConnection();
};

#ifdef __cplusplus
extern "C" {
#endif

ReplayProtocol* CreateUser(void);
void	DestoryUser(ReplayProtocol* hdl);
int		TaskStart(HTASKCFG task);
int		TaskStop(HTASKCFG task);

#ifdef __cplusplus
}
#endif
