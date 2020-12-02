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
	void ConnectionMade(HSOCKET hsock, const char* ip, int port);
	void ConnectionFailed(HSOCKET hsock, const char* ip, int port);
	void ConnectionClosed(HSOCKET hsock, const char* ip, int port);
	void Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len);
	int	 Loop();
	int  Destroy();

	void CheckReq(HSOCKET hsock, const char* data, int len);
	int  CheckRequest(HSOCKET hsock, const char* data, int len);
};

#endif // !_CLIENT_PROTOCOL_H_