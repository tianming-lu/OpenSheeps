#pragma once
#include "StressStruct.h"
#include "./../common/IOCPReactor.h"

#define INIT_BUFF_SIZE 10240

class StressProtocol :
	public BaseProtocol
{
public:
	t_socket_buff recvBuff = {0};

public:
	StressProtocol();
	~StressProtocol();

public:
	const char* StressSerIP = NULL;
	short StressSerPort = 0;
	HSOCKET	StressHsocket = NULL;


public:
	void ProtoInit(void* parm, int index);
	bool ConnectionMade(HSOCKET sock, char* ip, int port);
	bool ConnectionFailed(HSOCKET sock, char* ip, int port);
	bool ConnectionClosed(HSOCKET sock, char* ip, int port);
	int Send(HSOCKET hsock, char* ip, int port, char* data, int len);
	int	 Recv(HSOCKET hsock, char* ip, int port, char* data, int len);
	int	 Loop();
	int	 ReInit();
	int  Destroy();

	bool ReportError();
	bool AddBuff(HSOCKET sock, char* data, int len);
	bool DestroyBuff(HSOCKET sock, int len);
	int  CheckReq(HSOCKET sock, char* data, int len);
	int  CheckRequest(HSOCKET sock, char* data, int len);
};
