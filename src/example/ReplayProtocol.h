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
	void Init();
	void ConnectionMade(HSOCKET hsock);
	void ConnectionFailed(HSOCKET hsock);
	void ConnectionClosed(HSOCKET hsock);
	void Recved(HSOCKET hsock, const char* data, int len);
	void TimeOut();
	void Event(uint8_t event_type, const char* ip, int port, const char* content, int clen, bool udp);
	void ReInit();
	void Destroy();

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
