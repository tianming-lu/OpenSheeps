#pragma once
#include "StressStruct.h"
#include "./../common/cJSON.h"
#include "./../common/IOCPReactor.h"

typedef int (*client_cmd_cb) (HSOCKET sock, int cmdNO, cJSON* root);

extern BaseFactory* subfactory;
extern int clogId;

typedef struct
{
	string	fmd5;
	size_t	size;
	size_t	offset;
	int		type;
	FILE* file;
}t_file_info;

int ClientLogInit(const char* configfile);

void do_client_func_by_cmd(HSOCKET hsock, int cmdNO, cJSON* root);
