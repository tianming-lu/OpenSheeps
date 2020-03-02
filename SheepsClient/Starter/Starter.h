#pragma once

typedef int (*StressClientRun)(char* ip, short port, short listen);
typedef int (*StressClientStop)();
typedef struct dllAPI
{
	void* dllHandle;
	StressClientRun   run;
	StressClientStop  stop;
}t_stress_dll;