#pragma once
#include "./../common/TaskManager.h"
#include <map>

#ifdef REPLAY_EXPORTS
#define RePlay_API __declspec(dllexport)
#else
#define RePlay_API __declspec(dllimport)
#endif

typedef struct {
	HSOCKET hsock;
	std::string sendbuf;
	std::string recvbuf;
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
	void ProtoInit();
	void ConnectionMade(HSOCKET hsock, const char* ip, int port);
	void ConnectionFailed(HSOCKET hsock, const char* ip, int port);
	void ConnectionClosed(HSOCKET hsock, const char* ip, int port);
	void Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len);
	void TimeOut();
	void Event(uint8_t event_type, const char* ip, int port, const char* content, int clen);
	void ReInit();
	void Destroy();

	HSOCKET* GetScokFromConnection(const char* ip, int port);
	bool CloseAllConnection();
};

#ifdef __cplusplus
extern "C" {
#endif

	RePlay_API UserProtocol* CreateUser(void);
	RePlay_API void DestoryUser(UserProtocol* hdl);
	RePlay_API int Init(HTASKCFG task);
	RePlay_API int UnInit(HTASKCFG task);
	RePlay_API int ThreadInit(HTASKCFG task);
	RePlay_API int ThreadUnInit(HTASKCFG task);

#ifdef __cplusplus
}
#endif
