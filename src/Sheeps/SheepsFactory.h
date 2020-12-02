#ifndef _SHEEPS_FACTORY_H_
#define _SHEEPS_FACTORY_H_
#include "ClientProtocol.h"

class SheepsFactory :
	public BaseFactory
{
public:
	SheepsFactory();
	~SheepsFactory();

public:
	bool ClientRun = false;
	bool ClientRuning = false;
	char StressServerIp[16] = {0x0};
	short StressServerPort = 0;
	ClientProtocol* ClientProto = NULL;
	uint8_t	projectid = 0;

public:
	bool	FactoryInit();

	bool	ClientInit();
	bool	ClientLoop();
	bool	ClientRunOrStop(const char* ip, int port);

	void	FactoryLoop();
	void	FactoryClose();
	BaseProtocol* CreateProtocol();
	void	DeleteProtocol(BaseProtocol* proto);
};

extern Reactor* rec;
extern SheepsFactory* sheepsfc;

#endif // !_SHEEPS_FACTORY_H_