/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#include "pch.h"
#include "ServerProxy.h"
#include <algorithm>
#include <string>
#ifdef __WINDOWS__
#include "ws2tcpip.h"
#endif // __WINDOWS__


bool Proxy_record = false;
sqlite3* mysql;
const char* record_database = "data.db";

std::map<HSOCKET, HPROXYINFO>* ProxyMap;
std::mutex* ProxyMapLock = NULL;

std::list<t_recode_info*>* recordList = NULL;
std::mutex* recordListLock = NULL;
std::list<t_recode_info*>* newrecordList = NULL;

int my_sqlite3_open(char* inDbName, sqlite3** ppdb)
{
#ifdef __WINDOWS__
	int codepage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;

	int nlen = MultiByteToWideChar(codepage, 0, inDbName, -1, NULL, 0);
	WCHAR wBuf[256];
	MultiByteToWideChar(CP_ACP, 0, inDbName, -1, wBuf, nlen);
	CHAR pBuf[256];
	nlen = WideCharToMultiByte(CP_UTF8, 0, wBuf, -1, 0, 0, 0, 0);
	WideCharToMultiByte(CP_UTF8, 0, wBuf, -1, pBuf, nlen, 0, 0);
	return sqlite3_open(pBuf, ppdb);
#else
	return sqlite3_open(inDbName, ppdb);
#endif // __WINDOWS__
}

bool ProxyServerInit()
{
	ProxyMap = new(std::nothrow) std::map<HSOCKET, HPROXYINFO>;
	ProxyMapLock = new(std::nothrow) std::mutex;
	recordList = new(std::nothrow) std::list<t_recode_info*>;
	recordListLock = new(std::nothrow) std::mutex;
	newrecordList = new(std::nothrow) std::list<t_recode_info*>;
	if (ProxyMap == NULL || ProxyMapLock == NULL || recordList == NULL || recordListLock == NULL ||
		newrecordList == NULL)
	{
		if (ProxyMap) delete ProxyMap;
		if (ProxyMapLock) delete ProxyMapLock;
		if (recordList) delete recordList;
		if (recordListLock) delete recordListLock;
		if (newrecordList)	delete newrecordList;
		return  false;
	}

	char dbfile[256] = { 0x0 };
	snprintf(dbfile, sizeof(dbfile), "%s%s", RecordPath, record_database);
	my_sqlite3_open(dbfile, &mysql);
	sqlite3_exec(mysql, "PRAGMA synchronous = OFF; ", 0, 0, 0);
	return true;
}

bool ChangeDatabaseName(const char* new_name)
{
	if (Proxy_record == true)
		return  false;

	char oldfile[256] = { 0x0 };
	char newfile[256] = { 0x0 };
	snprintf(oldfile, sizeof(oldfile), "%s%s", RecordPath, record_database);
	snprintf(newfile, sizeof(newfile), "%s%s", RecordPath, new_name);

	sqlite3_close(mysql);

	int ret = rename(oldfile, newfile);
	if (ret != 0)
	{
		my_sqlite3_open(oldfile, &mysql);
		sqlite3_exec(mysql, "PRAGMA synchronous = OFF; ", 0, 0, 0);
		return false;
	}

	my_sqlite3_open(oldfile, &mysql);
	sqlite3_exec(mysql, "PRAGMA synchronous = OFF; ", 0, 0, 0);
	return true;
}

static void insert_msg_to_record_list(const char* ip, int port, int type, const char* msg, int len)
{
	t_recode_info* info = (t_recode_info*)sheeps_malloc(sizeof(t_recode_info));
	if (info == NULL) return;
	info->time = GetSysTimeMicros();
	snprintf(info->ip, sizeof(info->ip), "%s", ip);
	info->port = port;
	info->type = type;
	if (msg)
	{
		info->msg = (char*)sheeps_malloc(len);
		if (info->msg == NULL)
		{
			sheeps_free(info);
			return;
		}
		memcpy(info->msg, msg, len);
		info->msglen = len;
	}
	recordListLock->lock();
	recordList->push_back(info);
	recordListLock->unlock();
}

static bool get_base64_string(char** content, size_t* len, char* src, int srclen)
{
	if (src)
	{
		Base64_Context str = {0x0};
		str.data = (u_char*)src;
		str.len = srclen;

		Base64_Context temp = {0x0};
		temp.len = str.len * 8 / 6 + 5;
		temp.data = (u_char*)sheeps_malloc(temp.len);
		if (temp.data == NULL)
			return false;

		encode_base64(&temp, &str);
		*content = (char*)temp.data;
		*len = temp.len;
	}
	else
	{
		*content = (char*)sheeps_malloc(4);
		if (*content == NULL)
			return false;
	}
	return true;
}

static bool cratet_table(const char* tablename)
{
	char* errmsg = NULL;
	char sql[512] = { 0x0 };
	snprintf(sql, sizeof(sql), "CREATE TABLE '%s' (\
                'id'  INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\
                'recordtime'  INT8,\
                'recordtype'  INTEGER,\
                'ip'  TEXT,\
                'port'  INTEGER,\
                'content'  TEXT);", tablename);
	int ret = sqlite3_exec(mysql, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		char* p = strstr(errmsg, "already exists");
		if (p == NULL)
			LOG(slogid, LOG_ERROR, "%s:%d sqlite3 error[%s]\r\n", __func__, __LINE__, errmsg);
		return false;
	}
	return true;
}

static int insert_table(const char* tablename, t_recode_info* info, int flag)
{
	char* errmsg = NULL;
	char* B64 = NULL;
	size_t B64len = 0;
	bool res = get_base64_string(&B64, &B64len, info->msg, info->msglen);
	if (res == false)
		return -1;

	size_t alloc_len = B64len + 256;
	char* sql = (char*)sheeps_malloc(alloc_len);
	if (sql == NULL)
	{
		sheeps_free(B64);
		return -2;
	}
	snprintf(sql, alloc_len, "insert into '%s' (recordtime, recordtype, ip, port, content) values(%lld, %d, '%s', %d, '%s')", tablename, info->time, info->type, info->ip, info->port, B64);
	sheeps_free(B64);

	int ret = sqlite3_exec(mysql, sql, NULL, NULL, &errmsg);
	if (ret != SQLITE_OK)
	{
		if (flag)
		{
			LOG(slogid, LOG_ERROR, "%s:%d sqlite3 error[%s]\r\n", __func__, __LINE__, errmsg);
			LOG(slogid, LOG_ERROR, "%s:%d [%s]\r\n", __func__, __LINE__, sql);
		}
		sheeps_free(sql);
		return -3;
	}
	sheeps_free(sql);
	return 0;
}

void insert_msg_recodr_db()
{
	static time_t lasttime = time(NULL);
	if (recordList == NULL)
		return;
	if ((recordList->size() > 10) || (recordList->size() > 0 && time(NULL) - lasttime > 5))
	{
		recordListLock->lock();
		std::list<t_recode_info*>* oldlist = recordList;
		recordList = newrecordList;
		newrecordList = oldlist;
		recordListLock->unlock();

		sqlite3_exec(mysql, "begin;", 0, 0, 0);
		std::list<t_recode_info*>::iterator iter = oldlist->begin();
		for (; iter != oldlist->end(); ++iter)
		{
			t_recode_info* info = *iter;
			char tablename[64] = { 0x0 };
			int offset = snprintf(tablename, sizeof(tablename), "record_%sp%d", info->ip, info->port);
			std::replace(tablename, tablename + offset, '.', '_');

			if (info->type == 0)
				cratet_table(tablename);
			int ret = insert_table(tablename, info, 0);
			if (ret != 0)
			{
				if (cratet_table(tablename) == true)
				{
					ret = insert_table(tablename, info, 1);
					if (ret != 0)
						LOG(slogid, LOG_ERROR, "%s:%d insert error\n", __func__, __LINE__);
				}
				else
				{
					LOG(slogid, LOG_ERROR, "%s:%d create error\n", __func__, __LINE__);
				}
			}

			if (info->msg)
				sheeps_free(info->msg);
			sheeps_free(info);
		}
		sqlite3_exec(mysql, "commit;", 0, 0, 0);
		oldlist->clear();
		lasttime = time(NULL);
	}
}

static int ProxyRemoteRequest(HSOCKET hsock, ServerProtocol* proto, const char* data, int len)
{
	HsocketSend(proto->proxyInfo->proxySock, data, len);
	if (Proxy_record == true)
	{
		insert_msg_to_record_list(proto->proxyInfo->serverip, proto->proxyInfo->serverport, 1, data, len);
	}
	return len;
}

static int ProxyConnectRequest(HSOCKET hsock, ServerProtocol* proto, const char* data, int len)
{
	if (len < 10)
		return 0;
	//uint8_t ver = *data;
	uint8_t cmd = *(data + 1);
	uint8_t type = *(data + 3);

	proto->proxyInfo->proxytype = TCP_CONN;
	if (cmd == 0x3)
		proto->proxyInfo->proxytype = UDP_CONN;

	char name[64] = { 0x0 };
	char ip[64] = { 0x0 };
	uint16_t port = ntohs(*((uint16_t*)(data + len - 2)));
	in_addr addr = { 0x0 };
	switch (type)
	{
	case 0x1:
		memcpy(&addr, data + 4, sizeof(addr));
		inet_ntop(AF_INET, &addr, ip, sizeof(ip));
		break;
	case 0x3:
		memcpy(name, data + 5, size_t(len) - 6);
		GetHostByName(name, ip, sizeof(ip));
		break;
	case 0x6:
		break;
	default:
		break;
	}
	proto->proxyInfo->proxyStat = PROXY_CONNECTING;
	if (proto->proxyInfo->proxytype == TCP_CONN)
	{
		proto->proxyInfo->proxySock = HsocketConnect(proto, ip, port, TCP_CONN);
		if (proto->proxyInfo->proxySock == NULL)
		{
			HsocketClose(proto->initSock);
		}
	}
	else if (proto->proxyInfo->proxytype == UDP_CONN)
	{
		proto->proxyInfo->udpClientSock = HsocketConnect(proto, "0.0.0.0", 0, UDP_CONN);
		ProxyConnectionMade(proto->proxyInfo->udpClientSock, proto, ip, port);
	}
	return len;
}

static int ProxyAuthRequest(HSOCKET hsock, ServerProtocol* proto, const char* data, int len)
{
	int clen = *(data + 1);
	if (len < clen + 2)
		return 0;
	char buf[4] = { 0x0 };
	buf[0] = 0x5;
	buf[1] = 0x0;
	proto->proxyInfo->proxyStat = PROXY_AUTHED;
	HsocketSend(hsock, buf, 2);
	return clen + 2;
}

int CheckPoxyRequest(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port, const char* data, int len)
{
	int clen = 0;
	if (hsock == proto->initSock)
	{
		switch (proto->proxyInfo->proxyStat)
		{
		case PROXY_INIT:
			clen = ProxyAuthRequest(hsock, proto, data, len);
			break;
		case PROXY_AUTHED:
			clen = ProxyConnectRequest(hsock, proto, data, len);
			break;
		case PROXY_CONNECTED:
			clen = ProxyRemoteRequest(hsock, proto, data, len);
			break;
		default:
			break;
		}
	}
	else if (hsock == proto->proxyInfo->proxySock)
	{
		if (proto->proxyInfo->proxytype == TCP_CONN)
		{
			HsocketSend(proto->initSock, data, len);
			if (Proxy_record)
				insert_msg_to_record_list(proto->proxyInfo->serverip, proto->proxyInfo->serverport, 3, data, len);
		}
		else if (proto->proxyInfo->proxytype == UDP_CONN)
		{
			char* buf = (char*)sheeps_malloc((size_t)len + 10);
			if (buf)
			{
				memcpy(buf, proto->proxyInfo->tempmsg, 10);
				memcpy(buf + 10, data, len);
				HsocketSend(proto->proxyInfo->udpClientSock, buf, len + 10);
				sheeps_free(buf);
				if (Proxy_record)
					insert_msg_to_record_list(proto->proxyInfo->serverip, proto->proxyInfo->serverport, 5, data, len);
			}
			else
			{
				LOG(slogid, LOG_ERROR, "%s:%d malloc error\r\n", __func__, __LINE__);
				HsocketClose(proto->initSock);
			}
		}
	}
	else if (hsock == proto->proxyInfo->udpClientSock)
	{
		memcpy(proto->proxyInfo->clientip, ip, strlen(ip));
		proto->proxyInfo->clientport = port;
		memcpy(proto->proxyInfo->tempmsg, data, 10);

		inet_ntop(AF_INET, (data+4), proto->proxyInfo->serverip, sizeof(proto->proxyInfo->serverip));
		proto->proxyInfo->serverport = ntohs(*(u_short*)(data + 8));
	
		if (proto->proxyInfo->proxySock == NULL)
			proto->proxyInfo->proxySock = HsocketConnect(proto, proto->proxyInfo->serverip, proto->proxyInfo->serverport, UDP_CONN);
		HsocketSend(proto->proxyInfo->proxySock, data + 10, len - 10);
		if (Proxy_record)
			insert_msg_to_record_list(proto->proxyInfo->serverip, proto->proxyInfo->serverport, 4, data + 10, len - 10);
	}
	return clen;
}

void ProxyConnectionMade(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port)
{
	if (proto->proxyInfo->proxyStat == PROXY_CONNECTING)
	{
		char buf[64] = { 0x0 };
		buf[0] = 0x5;
		buf[3] = 0x1;

		struct sockaddr_in addr_tcp = {0x0};
		int nSize = sizeof(addr_tcp);
		GetSockName(proto->initSock->fd, (struct sockaddr*)&addr_tcp, &nSize);

		memcpy(buf + 4, &addr_tcp.sin_addr, sizeof(addr_tcp.sin_addr));
			
		if (proto->proxyInfo->proxytype == UDP_CONN)
		{
			struct sockaddr_in addr_udp = {0x0};
			GetSockName(proto->proxyInfo->udpClientSock->fd, (struct sockaddr*)&addr_udp, &nSize);
			memcpy(buf + 4 + sizeof(in_addr), (char*)&addr_udp.sin_port, 2);
		}
		else
			memcpy(buf + 4 + sizeof(in_addr), (char*)&addr_tcp.sin_port, 2);
		proto->proxyInfo->proxyStat = PROXY_CONNECTED;
		HsocketSend(proto->initSock, buf, 4 + sizeof(in_addr) + 2);

		snprintf(proto->proxyInfo->serverip, sizeof(proto->proxyInfo->serverip), "%s", ip);
		proto->proxyInfo->serverport = port;
		ProxyMapLock->lock();
		ProxyMap->insert(std::pair<HSOCKET, HPROXYINFO>(proto->initSock, proto->proxyInfo));
		ProxyMapLock->unlock();

		if (Proxy_record == true && proto->proxyInfo->proxytype == TCP_CONN)
		{
			insert_msg_to_record_list(ip, port, 0, NULL, 0);
		}

		//LOG(slogid, LOG_DEBUG, "new proxy connetion made: %s:%d\r\n", ip, port);
	}
}

void ProxyConnectionFailed(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port)
{
	LOG(slogid, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	if (proto->proxyInfo->proxyStat == PROXY_CONNECTING && proto->proxyInfo->retry > 0 && proto->initSock != NULL)
	{
		proto->proxyInfo->retry -= 1;
		proto->proxyInfo->proxySock = HsocketConnect(proto, ip, port, TCP_CONN);
		if (proto->proxyInfo->proxySock == NULL)
		{
			HsocketClose(proto->initSock);
		}
	}
	else
	{
		proto->proxyInfo->proxySock = NULL;
		HsocketClose(proto->initSock);
	}
}

void ProxyConnectionClosed(HSOCKET hsock, ServerProtocol* proto, const char* ip, int port)
{
	if (hsock == proto->proxyInfo->proxySock)
	{
		proto->proxyInfo->proxySock = NULL;
		HsocketClose(proto->initSock);
		HsocketClose(proto->proxyInfo->udpClientSock);
	}
	else if (hsock == proto->initSock)
	{
		ProxyMapLock->lock();
		ProxyMap->erase(proto->initSock);
		ProxyMapLock->unlock();
		proto->initSock = NULL;
		if (proto->proxyInfo->proxyStat >= PROXY_CONNECTING)
			HsocketClose(proto->proxyInfo->proxySock);
		HsocketClose(proto->proxyInfo->udpClientSock);
	}
	else if (hsock == proto->proxyInfo->udpClientSock)
	{
		proto->proxyInfo->udpClientSock = NULL;
		HsocketClose(proto->initSock);
		HsocketClose(proto->proxyInfo->proxySock);
	}
	if (proto->proxyInfo->proxySock == NULL && proto->initSock == NULL && proto->proxyInfo->udpClientSock == NULL)
	{
		if (Proxy_record == true && proto->proxyInfo->proxyStat == PROXY_CONNECTED && proto->proxyInfo->proxytype == TCP_CONN)
		{
			insert_msg_to_record_list(proto->proxyInfo->serverip, proto->proxyInfo->serverport, 2, NULL, 0);
		}
	}
}