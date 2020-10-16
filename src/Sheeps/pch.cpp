// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"

char DllPath[MAX_PATH] = { 0x0 };
char ConfigFile[MAX_PATH] = { 0x0 };
char ProjectPath[MAX_PATH] = { 0x0 };
char RecordPath[MAX_PATH] = { 0x0 };
char LogPath[MAX_PATH] = { 0x0 };
// 当使用预编译的头时，需要使用此源文件，编译才能成功。

#ifndef __WINDOWS__ //for linux
#include "dlfcn.h"

__attribute__((constructor)) void _init(void)  //告诉gcc把这个函数扔到init section  
{   
    Dl_info dl_info;  
    if (dladdr((void*)_init, &dl_info))        //第二个参数就是获取的结果
	{
        snprintf(DllPath, MAX_PATH, "%s", dl_info.dli_fname);
		char* p = strrchr(DllPath, '/');    //找到绝对路径中最后一个"/"
		*(p + 1) = 0x0;                            //舍弃掉最后“/”之后的文件名，只需要路径
	}
    snprintf(ConfigFile, MAX_PATH, "%ssheeps.ini", DllPath);
	snprintf(ProjectPath, MAX_PATH, "%sproject/", DllPath);
	snprintf(RecordPath, MAX_PATH, "%srecord/", DllPath);
	snprintf(LogPath, MAX_PATH, "%slog/", DllPath);
	mkdir(ProjectPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir(RecordPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir(LogPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

__attribute__((destructor)) void _fini(void)  //告诉gcc把这个函数扔到fini section 
{
}
#endif