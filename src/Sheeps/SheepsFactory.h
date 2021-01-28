/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

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