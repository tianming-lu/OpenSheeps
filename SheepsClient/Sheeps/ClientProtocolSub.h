#ifndef _CLIENT_PROTOCOL_SUB_H_
#define _CLIENT_PROTOCOL_SUB_H_
#include "SheepsStruct.h"
#include "IOCPReactor.h"

typedef int (*client_cmd_cb) (HSOCKET sock, int cmdNO, cJSON* root);

extern BaseFactory* subfactory;
extern int clogId;

typedef struct
{
	char	fmd5[64];
	size_t	size;
	size_t	offset;
	int		type;
	FILE*	file;
}t_file_info;

int ClientLogInit(const char* configfile);
void do_client_func_by_cmd(HSOCKET hsock, int cmdNO, cJSON* root);

#endif // !_CLIENT_PROTOCOL_SUB_H_
