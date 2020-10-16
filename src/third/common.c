// common.cpp : 定义静态库的函数。
//

#include "common.h"
#include <stdio.h>

#ifdef __WINDOWS__
#include <Winsock2.h>
#include <Ws2tcpip.h>
int GetHostByName(char* name, char* buf, size_t size)
{
	struct addrinfo hints;
	struct addrinfo* res, * cur;
	struct sockaddr_in* addr;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;	
	hints.ai_flags = AI_PASSIVE; 
	hints.ai_protocol = 0;
	hints.ai_socktype = SOCK_STREAM; 

	int ret = getaddrinfo(name, NULL, &hints, &res);
	if (ret == -1)
	{
		return -1;
	}

	for (cur = res; cur != NULL; cur = cur->ai_next)
	{
		addr = (struct sockaddr_in*) cur->ai_addr;
		snprintf(buf, size, "%d.%d.%d.%d", addr->sin_addr.S_un.S_un_b.s_b1,
			addr->sin_addr.S_un.S_un_b.s_b2,
			addr->sin_addr.S_un.S_un_b.s_b3,
			addr->sin_addr.S_un.S_un_b.s_b4);
	}
	return 0;
}
#else
#include <netdb.h>
#include <arpa/inet.h>
int GetHostByName(char* name, char* buf, size_t size)
{
	struct hostent* h;
	if ((h = gethostbyname(name)) == NULL)
	{
		return -1;
	}
	snprintf(buf, size, "%s", inet_ntoa(*((struct in_addr*)h->h_addr_list[0])));
	return 0;
}
#endif // __WINDOWS__

#ifdef __WINDOWS__
int GetSockName(SOCKET fd, struct sockaddr* addr, int* nSize)
{
	return getsockname(fd, addr, nSize);
}
#else
int GetSockName(int fd, struct sockaddr* addr, int* nSize)
{
	return getsockname(fd, addr, (socklen_t*)nSize);
}
#endif // __WINDOWS__

#ifdef __WINDOWS__
#include <windows.h>
int GetCpuCount()
{
	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	return sysInfor.dwNumberOfProcessors;
#else
#include <sys/sysinfo.h>
int GetCpuCount()
{
	return get_nprocs_conf();
#endif // __WINDOWS__
}

#ifdef __WINDOWS__
#include <windows.h>
long long int GetSysTimeMicros()
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
long long int GetSysTimeMicros()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}
#endif // __WINDOWS__

void TimeSleep(int time_ms)
{
#ifdef __WINDOWS__
	Sleep(time_ms);
#else
	usleep(time_ms*1000);
#endif // __WINDOWS__
}