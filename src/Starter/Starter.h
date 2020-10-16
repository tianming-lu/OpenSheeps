#ifndef _STARTER_H_
#define _STARTER_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#include "windows.h"
#else
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#endif

typedef int (*StressClientRun)(char* ip, short port, short listen);
typedef int (*StressClientStop)();
typedef struct dllAPI
{
	void* dllHandle;
	StressClientRun   run;
	StressClientStop  stop;
}t_stress_dll;
#endif // !_STARTER_H_