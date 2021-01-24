#ifndef _CLIENT_PROTOCOL_H_
#define _CLIENT_PROTOCOL_H_
#include "SheepsStruct.h"
#include "Reactor.h"

class ClientProtocol :
	public BaseProtocol
{
public:
	ClientProtocol();
	~ClientProtocol();

public:
	char		StressSerIP[16] = { 0x0 };
	uint16_t	StressSerPort = 0;
	uint8_t		ProjectID = 0;
	HSOCKET		StressHsocket = NULL;
	time_t		heartbeat = 0;


public:
	void ConnectionMade(HSOCKET hsock);
	void ConnectionFailed(HSOCKET hsock);
	void ConnectionClosed(HSOCKET hsock);
	void ConnectionRecved(HSOCKET hsock, const char* data, int len);
	int	 Loop();
	int  Destroy();

	void CheckReq(HSOCKET hsock, const char* data, int len);
	int  CheckRequest(HSOCKET hsock, const char* data, int len);
};

#endif // !_CLIENT_PROTOCOL_H_