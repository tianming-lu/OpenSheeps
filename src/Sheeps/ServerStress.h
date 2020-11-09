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

int CheckStressRequest(HSOCKET hsock, ServerProtocol* proto, const char* data, int len);
void StressConnectionClosed(HSOCKET hsock, ServerProtocol* proto);

#endif // !_SERVER_STRESS_H_