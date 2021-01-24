// example.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include "./../Sheeps/TaskManager.h"
#include "ReplayProtocol.h"

#ifdef __WINDOWS__
#ifdef _DEBUG
#ifndef _WIN64
#pragma comment(lib,".\\..\\Win32\\sheeps\\Debug\\sheeps.lib")
#else
#pragma comment(lib, ".\\..\\X64\\sheeps\\Debug\\Sheeps.lib")
#endif // _WIN32

#else
#ifndef _WIN64
#pragma comment(lib, ".\\..\\Win32\\sheeps\\Release\\sheeps.lib")
#else
#pragma comment(lib, ".\\..\\X64\\sheeps\\Release\\Sheeps.lib")
#endif // _WIN32
#endif
#endif

int main()
{
	int projectid = 0;
    TaskManagerRun(projectid, CreateUser, DestoryUser, TaskStart, TaskStop, true);
}