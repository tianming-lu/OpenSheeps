#ifndef _SHEEPS_MAIN_H_
#define _SHEEPS_MAIN_H_

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#ifdef STRESS_EXPORTS
#define Stress_API __declspec(dllexport)
#else
#define Stress_API __declspec(dllimport)
#endif
#else
#define Stress_API
#endif // __WINDOWS__



#ifdef __cplusplus
extern "C" {
#endif

	Stress_API int StressClientRun(char* ip, short port, short listenPort);
	Stress_API int SheepsClientRun(const char* stressIp, short stressPort, int projectid);
	Stress_API int StressClientStop();

#ifdef __cplusplus
}
#endif

#endif // !_SHEEPS_MAIN_H_