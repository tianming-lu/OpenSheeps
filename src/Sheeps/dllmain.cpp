/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

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
	Sheeps_Module = hModule;
	int i = 0;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		i = GetModuleFileNameA(NULL, EXE_Path, MAX_PATH);
		while (i > -1)
		{
			if (EXE_Path[i] == '\\' || EXE_Path[i] == '/')
				break;
			else
			{
				EXE_Path[i] = 0x0;
				i--;
			}
		}
		snprintf(ConfigFile, MAX_PATH, "%ssheeps.ini", EXE_Path);
		snprintf(ProjectPath, MAX_PATH, "%sproject\\", EXE_Path);
		snprintf(RecordPath, MAX_PATH, "%srecord\\", EXE_Path);
		snprintf(LogPath, MAX_PATH, "%slog\\", EXE_Path);
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

