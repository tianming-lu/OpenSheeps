// common.cpp : 定义静态库的函数。
//

#include "pch.h"
#include "common.h"
#include "framework.h"
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>

int GetHostByName(char* name, char* buf, size_t size)
{
	struct addrinfo hints;
	struct addrinfo* res, * cur;
	struct sockaddr_in* addr;

	memset(&hints, 0, sizeof(addrinfo));
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
