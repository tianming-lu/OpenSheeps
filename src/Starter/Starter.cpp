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