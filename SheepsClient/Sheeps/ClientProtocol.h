#pragma once
#include "StressStruct.h"
#include "./../common/IOCPReactor.h"

#define INIT_BUFF_SIZE 10240

class StressProtocol :
	public BaseProtocol
{
public:
	std::string recvBuff;

public:
	StressProtocol();
	~StressProtocol();

public:
	const char* StressSerIP = NULL;
	short StressSerPort = 0;
	HSOCKET	StressHsocket = NULL;


public:
	bool ConnectionMade(HSOCKET sock, const char* ip, int port);
	bool ConnectionFailed(HSOCKET sock, const char* ip, int port);
	bool ConnectionClosed(HSOCKET sock, const char* ip, int port);
	int	 Recv(HSOCKET hsock, const char* ip, int port, const char* data, int len);
	int	 Loop();
	int  Destroy();

	bool ReportError();
	int  CheckReq(HSOCKET sock, const char* data, int len);
	int  CheckRequest(HSOCKET sock, const char* data, int len);
};
