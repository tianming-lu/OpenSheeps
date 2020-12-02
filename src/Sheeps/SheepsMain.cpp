// StressClient.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "pch.h"
#include "SheepsMain.h"
#include "SheepsFactory.h"
#include "SheepsMemory.h"
#include <iostream>

#ifdef __WINDOWS__
#ifdef _DEBUG
#ifndef _WIN64
#pragma comment(lib,".\\..\\Debug\\third.lib")
#else
#pragma comment(lib, ".\\..\\X64\\Debug\\third.lib")
#endif // _WIN32

#else
#ifndef _WIN64
#pragma comment(lib, ".\\..\\release\\third.lib")
#else
#pragma comment(lib, ".\\..\\X64\\release\\third.lib")
#endif // _WIN32
#endif
#endif // __WINDOWS__

char managerIp[16] = { 0x0 };
int managerPort = 0;

static int ReactorRun()
{
	if (rec == NULL)
	{
		rec = new(std::nothrow) Reactor();
		if (rec == NULL)
		{
			printf("%s:%d Reactor create failed!\n", __func__, __LINE__);
			return -1;
		}
		if (ReactorStart(rec))
		{
			printf("%s:%d Reactor start failed!\n", __func__, __LINE__);
			return -2;
		}
	}
	return 0;
}

static int SheepsFactoryRun(const char* sheepsIp, int sheepsPort, int projectId, int listenPort)
{
	if (NULL == sheepsfc)
	{
		sheepsfc = new(std::nothrow) SheepsFactory();
		if (sheepsfc == NULL)
		{
			printf("%s:%d Sheeps factory create failed!\n", __func__, __LINE__);
			return -1;
		}
	}
	if (sheepsIp == NULL || sheepsPort <= 0)
	{
		sheepsfc->Set(rec, listenPort);
		sheepsfc->StressServerPort = 0;
	}
	else
	{
		sheepsfc->Set(rec, listenPort);
		memcpy(sheepsfc->StressServerIp, sheepsIp, strlen(sheepsIp));
		sheepsfc->StressServerPort = sheepsPort;
		sheepsfc->projectid = projectId;
		sheepsfc->ClientRun = true;
	}
	
	if (FactoryRun(sheepsfc))
	{
		printf("%s:%d Sheeps factory start failed!\n", __func__, __LINE__);
		return -2;
	}
	return 0;
}

int SheepsServerRun(u_short listenPort)
{
	config_init(ConfigFile);
	if (listenPort <=  0)
		listenPort = config_get_int_value("agent", "srv_port", 1080);

	if (ReactorRun()) return -1;
	if (SheepsFactoryRun(NULL, 0, 0, listenPort)) return -2;

	return listenPort;
}

int SheepsClientRun(int projectid, bool server)
{
	config_init(ConfigFile);
	const char* sheepsIp = config_get_string_value("agent", "srv_ip", "127.0.0.1");
	snprintf(managerIp, sizeof(managerIp), "%s", sheepsIp);
	managerPort = config_get_int_value("agent", "srv_port", 1080);
	u_short listen_port = 0;
	if (server)
		listen_port = managerPort;

	if (ReactorRun()) return -1;
	if (SheepsFactoryRun(managerIp, managerPort, projectid, listen_port)) return -2;
	return 0;
}

int StressClientStop()
{
	FactoryStop(sheepsfc);
	//ReactorStop(sheepsfc);
	//delete sheepsfc;
	return 0;
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门提示: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件