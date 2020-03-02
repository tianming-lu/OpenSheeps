// pch.cpp: 与预编译标头对应的源文件

#include "pch.h"

// 当使用预编译的头时，需要使用此源文件，编译才能成功。

#ifdef _DEBUG
#ifndef _WIN64
#pragma comment(lib,".\\..\\Debug\\sheeps\\sheeps.lib")
#else
#pragma comment(lib, ".\\..\\X64\\Debug\\sheeps\\Sheeps.lib")
#endif // _WIN32

#else
#ifndef _WIN64
#pragma comment(lib, ".\\..\\Release\\sheeps\\sheeps.lib")
#else
#pragma comment(lib, ".\\..\\X64\\Release\\sheeps\\Sheeps.lib")
#endif // _WIN32
#endif