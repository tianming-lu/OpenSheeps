/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _SERVER_STRESS_H_
#define _SERVER_STRESS_H_
#include "SheepsStruct.h"
#include "Reactor.h"
#include "ServerProtocol.h"
#include <list>


extern std::map<HSOCKET, HCLIENTINFO>* StressClientMap;
extern std::mutex* StressClientMapLock;

typedef struct
{
	char	fmd5[64];
	size_t	size;
}t_file_update;

bool StressServerInit();
int ServerUnInit();

int sync_files(HSOCKET hsock, int projectid);

int CheckStressRequest(HSOCKET hsock, ServerProtocol* proto, const char* data, int len);
void StressConnectionClosed(HSOCKET hsock, ServerProtocol* proto);

#endif // !_SERVER_STRESS_H_