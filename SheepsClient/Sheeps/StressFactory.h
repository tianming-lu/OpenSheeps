#pragma once
#include "ClientProtocol.h"
#include <string>

class StressFactory :
	public BaseFactory
{
public:
	StressFactory();
	~StressFactory();

public:
	std::string StressServerIp;
	short StressServerPort;
	StressProtocol* StressProto = NULL;

public:
	bool	FactoryInit();
	bool	FactoryLoop();
	bool	FactoryClose();
	BaseProtocol* CreateProtocol();
	bool	DeleteProtocol(BaseProtocol* proto);
};

