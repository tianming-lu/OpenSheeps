#include "pch.h"
#include "StressFactory.h"
#include "ClientProtocol.h"
#include "ClientProtocolSub.h"
#include "./../common/log.h"
#include "./../common/TaskManager.h"

//const char* ConfigFile = "./stresstest/config.ini";

StressFactory::StressFactory()
{
}


StressFactory::~StressFactory()
{
}

bool StressFactory::FactoryInit()
{
	if (this->StressServerPort != 0 && this->StressProto == NULL)
	{
		ClientLogInit(ConfigFile);
		init_task_log(ConfigFile);
		this->StressProto = new StressProtocol();
		this->StressProto->factory = this;
		this->StressProto->self = (BaseProtocol*)(this->StressProto);
		this->StressProto->StressSerIP = this->StressServerIp.c_str();
		this->StressProto->StressSerPort = this->StressServerPort;
	}
	return true;
}

bool StressFactory::FactoryLoop()
{
	if (this->StressProto != NULL)
		this->StressProto->Loop();
	LogLoop();
	return true;
}

bool StressFactory::FactoryClose()
{
	if (this->StressProto != NULL)
	{
		this->StressProto->Destroy();
	}
	return true;
}

bool StressFactory::TcpConnect()
{
	return true;
}

BaseProtocol* StressFactory::CreateProtocol()
{
	return NULL;
}

bool StressFactory::DeleteProtocol(BaseProtocol* proto)
{
	return true;
}