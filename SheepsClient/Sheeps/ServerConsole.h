#ifndef _SERVER_CONSOLE_H_
#define _SERVER_CONSOLE_H_
#include "ServerProtocol.h"
#include <map>

enum {
	STATE_UNSTART = 0,
	STATE_RUNING,
	STATE_STOP
};

enum {
	STAGE_INIT = 0,
	STAGE_LOOP,
	STAGE_CLEAN,
	STAGE_OVER
};

typedef struct {
	char srcAddr[32];
	char dstAddr[32];
}Readdr, *HREADDR;

typedef struct {
	uint8_t		taskID;
	uint8_t		projectID;
	char		projectName[20];
	uint8_t		envID;
	int32_t		totalUser;
	int32_t		onceUser;
	uint16_t	spaceTime;
	uint8_t		loopMode; //0循环 1不循环 2实时回放
	bool		ignoreErr;
	uint8_t		taskState;
	char		taskDes[64];
	char		dbName[64];
	std::list<Readdr>* replayAddr;
	std::map<std::string, Readdr> *changeAddr;
}TaskConfig, * HTASKCONFIG;

typedef struct {
	time_t		recordTime;
	uint8_t		recordType;
	char		ip[16];
	int			port;
	char*		content;
	size_t		contentLen;
}RecordMsg, * HRECORDMSG;

typedef struct {
	HTASKCONFIG taskCfg;
	int16_t userCount;
	uint8_t stage;

	time_t lastChangUserTime;

	std::list<RecordMsg> *tempMsg;
	time_t startRecord;
	time_t startReal;
	uint32_t startRow;
	bool stopPushRecord;
	sqlite3* dbConn;
	char* dbsql;
	int sqllen;
}TaskRuning, * HTASKRUN;

int CheckConsoleRequest(HSOCKET hsock, ServerProtocol* proto, const char* data, int len);

#endif // !_SERVER_CONSOLE_H_