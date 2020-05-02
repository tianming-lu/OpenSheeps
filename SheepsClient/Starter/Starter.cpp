// Starter.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "Starter.h"
#include <iostream>
#include "windows.h"

const char* defaultIP = "127.0.0.1";
short defaultPort = 1080;
short defaultlisten = 0;
t_stress_dll strdll;

int main(int argc, char* argv[])
{
	char* ip = (char*)defaultIP;
	short port = defaultPort;
	short listen = defaultlisten;
	if (argc == 2)
	{
		listen = atoi(argv[1]);
	}
	else if (argc == 3)
	{
		ip = argv[1];
		port = atoi(argv[2]);
	}
	else if (argc == 4)
	{
		ip = argv[1];
		port = atoi(argv[2]);
		listen = atoi(argv[3]);
	}
	strdll.dllHandle = LoadLibraryA("Sheeps.dll");
	if (strdll.dllHandle == NULL)
	{
		printf("dll load failed.");
		return -1;
	}
	strdll.run = (StressClientRun)GetProcAddress((HMODULE)strdll.dllHandle, "StressClientRun");
	strdll.stop = (StressClientStop)GetProcAddress((HMODULE)strdll.dllHandle, "StressClientStop");

	strdll.run(ip, port, listen);
	int A = 0;
	while (true)
	{
		system("CLS");
		printf("\n压力测试负载端运行中......\n\n");
		printf("操作：\n【Q退出】\n");
		A = getchar();
		if (A == 'Q')
			break;
	}
	strdll.stop();
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
