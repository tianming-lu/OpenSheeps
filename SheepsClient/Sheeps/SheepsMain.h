#ifndef _SHEEPS_MAIN_H_
#define _SHEEPS_MAIN_H_

#ifdef STRESS_EXPORTS
#define Stress_API __declspec(dllexport)
#else
#define Stress_API __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

	Stress_API int StressClientRun(char* ip, short port, short listenPort);
	Stress_API int SheepsClientRun(char* stressIp, short stressPort, int projectid);
	Stress_API int StressClientStop();

#ifdef __cplusplus
}
#endif

#endif // !_SHEEPS_MAIN_H_