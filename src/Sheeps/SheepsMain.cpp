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

int StressClientRun(char *stressIp, short stressPort, short listenPort)
{
	config_init(ConfigFile);
	cJSON_Hooks jsonhook = { 0x0 };
	jsonhook.malloc_fn = sheeps_malloc;
	jsonhook.free_fn = sheeps_free;
	cJSON_InitHooks(&jsonhook);

	if (rec == NULL)
	{
		rec = new Reactor();
		ReactorStart(rec);
	}
	if (NULL == stressfc)
	{
		stressfc = new SheepsFactory();
	}
	stressfc->Set(rec, listenPort);
	memcpy(stressfc->StressServerIp, stressIp, strlen(stressIp));
	stressfc->StressServerPort = stressPort;
	if (stressPort > 0)
		stressfc->ClientRun = true;
	FactoryRun(stressfc);
	return 0;
}

int SheepsClientRun(const char* stressIp, short stressPort, int projectid)
{
	cJSON_Hooks jsonhook = { 0x0 };
	jsonhook.malloc_fn = sheeps_malloc;
	jsonhook.free_fn = sheeps_free;
	cJSON_InitHooks(&jsonhook);

	if (rec == NULL)
	{
		rec = new Reactor();
		ReactorStart(rec);
	}
	if (NULL == stressfc)
	{
		stressfc = new SheepsFactory();
	}
	stressfc->Set(rec, 0);
	memcpy(stressfc->StressServerIp, stressIp, strlen(stressIp));
	stressfc->StressServerPort = stressPort;
	stressfc->projectid = projectid;
	if (stressPort > 0)
		stressfc->ClientRun = true;
	FactoryRun(stressfc);
	return 0;
}

int StressClientStop()
{
	FactoryStop(stressfc);
	//ReactorStop(stressfc);
	//delete stressfc;
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