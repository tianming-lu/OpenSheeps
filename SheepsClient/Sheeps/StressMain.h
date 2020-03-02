#pragma once

#ifdef STRESS_EXPORTS
#define Stress_API __declspec(dllexport)
#else
#define Stress_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	Stress_API int StressClientRun(char* ip, short port, short listenPort);
	Stress_API int StressClientStop();

#ifdef __cplusplus
}
#endif
