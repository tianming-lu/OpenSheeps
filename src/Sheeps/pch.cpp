/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#include "pch.h"

#ifdef __WINDOWS__
HMODULE Sheeps_Module;
#endif // __WINDOWS__


char EXE_Path[MAX_PATH] = { 0x0 };
char ConfigFile[MAX_PATH] = { 0x0 };
char ProjectPath[MAX_PATH] = { 0x0 };
char RecordPath[MAX_PATH] = { 0x0 };
char LogPath[MAX_PATH] = { 0x0 };

#ifndef __WINDOWS__ //for linux
#include "dlfcn.h"

__attribute__((constructor)) void _init(void)  //告诉gcc把这个函数扔到init section  
{   
    Dl_info dl_info;  
    if (dladdr((void*)_init, &dl_info))        //第二个参数就是获取的结果
	{
        snprintf(EXE_Path, MAX_PATH, "%s", dl_info.dli_fname);
		char* p = strrchr(EXE_Path, '/');    //找到绝对路径中最后一个"/"
		*(p + 1) = 0x0;                            //舍弃掉最后“/”之后的文件名，只需要路径
	}
    snprintf(ConfigFile, MAX_PATH, "%ssheeps.ini", EXE_Path);
	snprintf(ProjectPath, MAX_PATH, "%sproject/", EXE_Path);
	snprintf(RecordPath, MAX_PATH, "%srecord/", EXE_Path);
	snprintf(LogPath, MAX_PATH, "%slog/", EXE_Path);
	mkdir(ProjectPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir(RecordPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir(LogPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

__attribute__((destructor)) void _fini(void)  //告诉gcc把这个函数扔到fini section 
{
}
#endif