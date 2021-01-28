/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#ifndef _SERVER_PROXY_H_
#define _SERVER_PROXY_H_
#include "ServerProtocol.h"

extern bool Proxy_record;
extern const char* record_database;

extern std::map<HSOCKET, HPROXYINFO>* ProxyMap;
extern std::mutex* ProxyMapLock;

typedef struct {
	long long	time;  //微秒
	char	ip[16];
	int		port;
	int		type;
	char*	msg;
	int		msglen;
}t_recode_info;

extern std::list<t_recode_info*>* recordList;

void insert_msg_recodr_db();
int	 CheckPoxyRequest(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port, const char* data, int len);
void ProxyConnectionMade(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port);
void ProxyConnectionFailed(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port);
void ProxyConnectionClosed(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port);
bool ProxyServerInit();
bool ChangeDatabaseName(const char* new_name);
int	 my_sqlite3_open(char* inDbName, sqlite3** ppdb);

#endif // !_SERVER_PROXY_H_