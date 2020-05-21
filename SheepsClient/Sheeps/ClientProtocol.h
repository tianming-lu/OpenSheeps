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
	void ConnectionMade(HSOCKET sock, const char* ip, int port);
	void ConnectionFailed(HSOCKET sock, const char* ip, int port);
	void ConnectionClosed(HSOCKET sock, const char* ip, int port);
	void Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len);
	int	 Loop();
	int  Destroy();

	bool ReportError();
	void CheckReq(HSOCKET sock, const char* data, int len);
	int  CheckRequest(HSOCKET sock, const char* data, int len);
};
