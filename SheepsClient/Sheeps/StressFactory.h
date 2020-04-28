#pragma once
#include "ClientProtocol.h"
#include <string>

using namespace std;

class StressFactory :
	public BaseFactory
{
public:
	StressFactory();
	~StressFactory();

public:
	string StressServerIp;
	short StressServerPort;
	StressProtocol* StressProto = NULL;

public:
	bool	FactoryInit();
	bool	FactoryLoop();
	bool	FactoryClose();
	BaseProtocol* CreateProtocol();
	bool	DeleteProtocol(BaseProtocol* proto);
};

