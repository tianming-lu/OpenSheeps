// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "stdio.h"
#include "direct.h"

#pragma warning(disable:6031)

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
	int i = 0;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		i = GetModuleFileNameA(hModule, DllPath, MAX_PATH);
		while (i > -1)
		{
			if (DllPath[i] == '\\' || DllPath[i] == '/')
				break;
			else
			{
				DllPath[i] = 0x0;
				i--;
			}
		}
		snprintf(ConfigFile, MAX_PATH, "%ssheeps.ini", DllPath);
		snprintf(ProjectPath, MAX_PATH, "%sproject\\", DllPath);
		snprintf(RecordPath, MAX_PATH, "%srecord\\", DllPath);
		snprintf(LogPath, MAX_PATH, "%slog\\", DllPath);
		_mkdir(ProjectPath);
		_mkdir(RecordPath);
		_mkdir(LogPath);
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

