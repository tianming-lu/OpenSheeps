/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

// common.cpp : 定义静态库的函数。
//

#include "common.h"
#include <stdio.h>

#ifdef __WINDOWS__
#include <Winsock2.h>
#include <Ws2tcpip.h>
int __STDCALL GetHostByName(char* name, char* buf, size_t size)
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
	if (ret != 0)
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