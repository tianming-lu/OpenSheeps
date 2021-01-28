/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _COMMON_H_
#define _COMMON_H_
#include <stddef.h>
#include <time.h>
#include <stdio.h>

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__
#define __STDCALL __stdcall
#define __CDECL__	__cdecl

#if defined COMMON_LIB || defined STRESS_EXPORTS
#define Common_API __declspec(dllexport)
#else
#define Common_API __declspec(dllimport)
#endif // COMMON_LIB
#else
#define __STDCALL
#define __CDECL__
#define Common_API
#endif // __WINDOWS__

#ifdef __WINDOWS__
#define TimeSleep(ms) Sleep(ms)
#else
#define TimeSleep(ms) usleep(ms*1000)
#endif // __WINDOWS__

#ifdef __cplusplus
extern "C"
{
#endif

Common_API int __STDCALL GetHostByName(char* name, char* buf, size_t size);
#ifdef __WINDOWS__
#include <Winsock2.h>
static inline int __STDCALL GetSockName(SOCKET fd, struct sockaddr* addr, int* nSize)
{
	return getsockname(fd, addr, nSize);
}
#else
#include <sys/socket.h>
static inline int __STDCALL GetSockName(int fd, struct sockaddr* addr, int* nSize)
{
	return getsockname(fd, addr, (socklen_t*)nSize);
}
#endif // __WINDOWS__

#ifdef __WINDOWS__
#include <windows.h>
static inline int __stdcall GetCpuCount()
{
	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	return sysInfor.dwNumberOfProcessors;
#else
#include <sys/sysinfo.h>
static inline int __STDCALL GetCpuCount()
{
	return get_nprocs_conf();
#endif // __WINDOWS__
}

#ifdef __WINDOWS__
#include <windows.h>
static inline long long int __stdcall GetSysTimeMicros()
{
	// 从1601年1月1日0:0:0:000到1970年1月1日0:0:0:000的时间(单位100ns)
#define EPOCHFILETIME   (116444736000000000UL)
	FILETIME ft;
	LARGE_INTEGER li;
	long long tt = 0;
	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	// 从1970年1月1日0:0:0:000到现在的微秒数(UTC时间)
	tt = (li.QuadPart - EPOCHFILETIME) / 10;
	return tt;
}
#else
#include <sys/time.h>
#include <unistd.h>
static inline long long int __STDCALL GetSysTimeMicros()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif // __WINDOWS__

static inline void GetTimeString(time_t ctime, char* buf, size_t size)
{
	struct tm tmm;
#ifdef __WINDOWS__
	localtime_s(&tmm, &ctime);
#else
	localtime_r(&ctime, &tmm);
#endif // __WINDOWS__
	snprintf(buf, size, "%04d%02d%02d%02d%02d%02d", tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec);
}

#ifdef __cplusplus
}
#endif

#endif