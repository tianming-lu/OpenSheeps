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
	string sendbuf;
	string recvbuf;
}t_connection_info;

class ReplayProtocol :
	public UserProtocol
{
public:
	ReplayProtocol();
	~ReplayProtocol();

public:
	bool pause = false;
	HTASKCFG task = NULL;
	HMESSAGE message = NULL;
	//t_msg_pointer MsgPointer = {0x0};

	map<int, t_connection_info> Connection;

public:
	void ProtoInit(void* parm, int index);
	bool ConnectionMade(HSOCKET hsock, char* ip, int port);
	bool ConnectionFailed(HSOCKET hsock, char* ip, int port);
	bool ConnectionClosed(HSOCKET hsock, char* ip, int port);
	int  Send(HSOCKET hsock, char* ip, int port, char* data, int len);
	int  Recv(HSOCKET hsock, char* ip, int port, char* data, int len);
	int	 Loop();
	int	 ReInit();
	int  Destroy();

	HSOCKET* GetScokFromConnection(const char* ip, int port);
	bool CloseAllConnection();
	int CheckReq(SOCKET sock, char* data, int len);
	int CheckRequest(SOCKET sock, char* data, int len);
};

#ifdef __cplusplus
extern "C" {
#endif

	RePlay_API ReplayProtocol* CreateUser(void);
	RePlay_API void DestoryUser(ReplayProtocol* hdl);
	RePlay_API int Init(void* parm);
	RePlay_API int UnInit(void* parm);

#ifdef __cplusplus
}
#endif
