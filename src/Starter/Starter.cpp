// Starter.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "Starter.h"
#include <iostream>

short		defaultlisten = 0;
t_sheeps_dll sheeps_api;

int main(int argc, char* argv[])
{
	short listen = defaultlisten;
	if (argc == 2)
	{
		listen = atoi(argv[1]);
	}
#ifdef __WINDOWS__
	sheeps_api.dllHandle = LoadLibraryA("Sheeps.dll");
	if (sheeps_api.dllHandle == NULL)
	{
		printf("dll load failed.");
		return -1;
	}
	sheeps_api.run = (SheepsServerRun)GetProcAddress((HMODULE)sheeps_api.dllHandle, "SheepsServerRun");
	sheeps_api.stop = (SheepsServerStop)GetProcAddress((HMODULE)sheeps_api.dllHandle, "SheepsServerStop");
#else
	sheeps_api.dllHandle = dlopen("./libsheeps.so", RTLD_NOW);
	if (sheeps_api.dllHandle == NULL)
	{
		printf("dll load failed: %s\n", dlerror());
		return -1;
	}
	sheeps_api.run = (SheepsServerRun)dlsym(sheeps_api.dllHandle, "SheepsServerRun");
	sheeps_api.stop = (SheepsServerStop)dlsym(sheeps_api.dllHandle, "SheepsServerStop");
#endif

	listen = sheeps_api.run(listen);
	if (listen < 0)
	{
		system("pause");
		return -2;
	}
	char in[4] = { 0x0 };
#ifdef __WINDOWS__
	system("chcp 65001");
#define clear_screen "CLS"
#else
#define clear_screen "clear"
#endif
	while (true)
	{
		system(clear_screen);
		printf("\nSheeps控制端运行中......\n\n");
		printf("控  制  端：[0.0.0.0:%d]\nsocks5代理：[0.0.0.0:%d]\n后      台：[http://127.0.0.1:%d]\n", listen, listen, listen);
		printf("\n选择:【Q退出】\n操作：");
		fgets(in, 3, stdin);
		if (in[0] == 'Q')
			break;
	}
	sheeps_api.stop();
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
