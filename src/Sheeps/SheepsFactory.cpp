#include "pch.h"
#include "SheepsFactory.h"
#include "ClientProtocol.h"
#include "ServerProtocol.h"
#include "ClientProtocolSub.h"
#include "ServerStress.h"
#include "ServerProxy.h"
#include "TaskManager.h"

Reactor* rec = NULL;
SheepsFactory* stressfc = NULL;

SheepsFactory::SheepsFactory()
{
}


SheepsFactory::~SheepsFactory()
{
}

bool SheepsFactory::FactoryInit()
{

	this->ClientInit();

	if (this->ServerPort > 0)
	{
		ServerInit(ConfigFile);
		StressServerInit();
		ProxyServerInit();
	}
	return true;
}

bool SheepsFactory::ClientInit()
{
	if (this->StressServerPort != 0 && this->ClientRun == true && this->ClientRuning == false)
	{
		ClientLogInit(ConfigFile);
		if (this->ClientProto == NULL)
			this->ClientProto = new ClientProtocol();
		this->ClientProto->SetFactory(this, CLIENT_PROTOCOL);
		memcpy(this->ClientProto->StressSerIP, this->StressServerIp, strlen(this->StressServerIp));
		this->ClientProto->StressSerPort = this->StressServerPort;
		this->ClientProto->ProjectID = this->projectid;
		this->ClientRuning = true;
	}
	return true;
}

bool SheepsFactory::ClientLoop()
{
	if (this->ClientRuning)
		this->ClientProto->Loop();
	return true;
}

bool SheepsFactory::ClientRunOrStop(const char* ip, int port)
{
	if (this->ClientRuning)
	{
		this->ClientRun = false;
		this->ClientRuning = false;
		this->ClientProto->Destroy();
	}
	else
	{
		memcpy(this->StressServerIp, ip, strlen(ip));
		this->StressServerPort = port;
		this->ClientRun = true;
	}
	return true;
}

void SheepsFactory::FactoryLoop()
{
	this->ClientInit();
	this->ClientLoop();
	LogLoop();
	insert_msg_recodr_db();
}

void SheepsFactory::FactoryClose()
{
	if (this->ClientProto != NULL)
	{
		this->ClientProto->Destroy();
	}
	if (this->ServerPort > 0)
	{
		ServerUnInit();
	}
}

BaseProtocol* SheepsFactory::CreateProtocol()
{
	BaseProtocol* proto = new(std::nothrow) ServerProtocol();
	return proto;
}

void SheepsFactory::DeleteProtocol(BaseProtocol* proto)
{
	ServerProtocol* user = (ServerProtocol*)proto;
	delete user;
}