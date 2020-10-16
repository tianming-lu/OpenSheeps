#ifndef _COMMON_H_
#define _COMMON_H_
#include <stddef.h>

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#if defined COMMON_LIB || defined STRESS_EXPORTS
#define Common_API __declspec(dllexport)
#else
#define Common_API __declspec(dllimport)
#endif // COMMON_LIB
#else
#define Common_API
#endif // __WINDOWS__


#ifdef __cplusplus
extern "C"
{
#endif

Common_API int GetHostByName(char* name, char* buf, size_t size);
#ifdef __WINDOWS__
#include <Winsock2.h>
Common_API int GetSockName(SOCKET fd, struct sockaddr* addr, int* nSize);
#else
#include <sys/socket.h>
Common_API int GetSockName(int fd, struct sockaddr* addr, int* nSize);
#endif // __WINDOWS__
Common_API int GetCpuCount();
Common_API long long int GetSysTimeMicros();
Common_API void TimeSleep(int time_ms);

#ifdef __cplusplus
}
#endif

#endif