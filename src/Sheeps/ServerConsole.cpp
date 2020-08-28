﻿#include "pch.h"
#include "ServerConsole.h"
#include "ServerProxy.h"
#include "ServerStress.h"
#include "SheepsFactory.h"
#include <algorithm>
#include "io.h"
#include "direct.h"

#define SQL_SIZE 1024

typedef void (*serverConsole_cmd_cb) (HSOCKET hsock, cJSON* root, char* uri);

std::map<uint8_t, uint8_t> taskIndexPool;
uint8_t taskIndex = 1;

std::map<uint8_t, HTASKCONFIG>	TaskCfg;
std::map<uint8_t, HTASKRUN>		TaskRun;
std::mutex* TaskCfgLock = new std::mutex;
std::mutex* TaskRunLock = new std::mutex;

static int task_add_once_user(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	int left_user = taskcfg->totalUser - runtask->userCount;
	int count = left_user > taskcfg->onceUser ? taskcfg->onceUser : left_user;
	runtask->userCount += count;
	return count;
}

static int64_t GetSysTimeMicros()
{
	// 从1601年1月1日0:0:0:000到1970年1月1日0:0:0:000的时间(单位100ns)
#define EPOCHFILETIME   (116444736000000000UL)
	FILETIME ft;
	LARGE_INTEGER li;
	int64_t tt = 0;
	GetSystemTimeAsFileTime(&ft);
	li.LowPart = ft.dwLowDateTime;
	li.HighPart = ft.dwHighDateTime;
	// 从1970年1月1日0:0:0:000到现在的微秒数(UTC时间)
	tt = (li.QuadPart - EPOCHFILETIME) / 10;
	return tt;
}

static int task_init_record_sql(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	if (taskcfg->replayAddr->empty())
	{
		runtask->dbsql = (char*)sheeps_malloc(1);
		return -1;
	}

	runtask->dbsql = (char*)sheeps_malloc(SQL_SIZE);

	char temp[256] = { 0x0 };
	char table[64] = { 0x0 };
	std::list<Readdr>::iterator iter = taskcfg->replayAddr->begin();
	for (; iter != taskcfg->replayAddr->end(); iter++)
	{
		memset(table, 0, sizeof(table));
		int offset = snprintf(table, sizeof(table), "record_%s", iter->srcAddr);
		std::replace(table, table + offset, '.', '_');
		std::replace(table, table + offset, ':', 'p');

		memset(temp, 0, sizeof(temp));
		snprintf(temp, sizeof(temp), "select recordtime, recordtype, ip, port, content from %s where recordtype < 3 or recordtype = 4", table);
		if (runtask->sqllen == 0)
			runtask->sqllen += snprintf(runtask->dbsql, SQL_SIZE, temp);
		else
			runtask->sqllen += snprintf(runtask->dbsql + runtask->sqllen, size_t(SQL_SIZE) - runtask->sqllen, " union  %s", temp);
	}
	//snprintf(runtask->dbsql + sqllen, SQL_SIZE - sqllen, " order by recordtime limit %d,100", runtask->startRow);
	return 0;
}

static int task_client_count(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	int count = 0;
	for (; iter != StressClientMap->end(); ++iter)
	{
		if (iter->second->projectid == taskcfg->projectID)
			count++;
	}
	StressClientMapLock->unlock();
	return count;
}

static void task_push_init(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	int client_count = task_client_count(runtask, taskcfg);
	if (client_count == 0)
	{
		runtask->stage = STAGE_CLEAN;
		return;
	}
	int once_user = task_add_once_user(runtask, taskcfg);
	int a = once_user / client_count;
	int b = once_user % client_count;

	char buf[512] = { 0x0 };
	char tem[128] = { 0x0 };
	if (taskcfg->loopMode == 0 && taskcfg->ignoreErr == true)
		snprintf(tem, sizeof(tem), "{\"TaskID\":%d,\"projectID\":%d,\"EnvID\":%d,\"IgnoreErr\":true,", taskcfg->taskID, taskcfg->projectID, taskcfg->envID);
	else
		snprintf(tem, sizeof(tem), "{\"TaskID\":%d,\"projectID\":%d,\"EnvID\":%d,\"IgnoreErr\":false,", taskcfg->taskID, taskcfg->projectID, taskcfg->envID);

	int machine_id = 0;
	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	for (; iter != StressClientMap->end(); ++iter)
	{
		if (iter->second->projectid != taskcfg->projectID)
			continue;

		int user_count = a;
		if (b > 0)
		{
			user_count++;
			b--;
		}
		memset(buf, 0, sizeof(buf));
		int n = snprintf(buf + 8 , sizeof(buf) - 8, "%s\"MachineID\":%d,\"UserCount\":%d}", tem, machine_id++, user_count);
		*(int*)buf = n + 8;
		*(int*)(buf + 4) = 2;
		IOCPPostSendEx(iter->first, buf, n + 8);
	}
	StressClientMapLock->unlock();

	runtask->lastChangUserTime = time(NULL);
	runtask->stage = STAGE_LOOP;
	runtask->tempMsg = new std::list<RecordMsg>;
	taskcfg->taskState = STATE_RUNING;
	char fullname[256] = { 0x0 };
	snprintf(fullname, sizeof(fullname), "%s\\%s", RecordPath, taskcfg->dbName);
	my_sqlite3_open(fullname, &runtask->dbConn);
	task_init_record_sql(runtask, taskcfg);
}

static void task_push_add_once_user(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	if (StressClientMap->empty())
		return;
	if (time(NULL) - runtask->lastChangUserTime < taskcfg->spaceTime)
		return;

	int client_count = task_client_count(runtask, taskcfg);
	int once_user = task_add_once_user(runtask, taskcfg);
	if (once_user == 0)
		return;
	int a = once_user / client_count;
	int b = once_user % client_count;

	int user_count = 0;
	char buf[128] = { 0x0 };
	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	for (; iter != StressClientMap->end(); ++iter)
	{
		if (iter->second->projectid != taskcfg->projectID)
			continue;
		user_count = a;
		if (b > 0)
		{
			user_count++;
			b--;
		}
		if (b < 0)
		{
			user_count--;
			b++;
		}
		memset(buf, 0, sizeof(buf));
		int n = snprintf(buf + 8, sizeof(buf) - 8, "{\"TaskID\":%d,\"Change\":%d}", taskcfg->taskID, user_count);
		*(int*)buf = n + 8;
		*(int*)(buf + 4) = 12;
		IOCPPostSendEx(iter->first, buf, n + 8);
	}
	StressClientMapLock->unlock();
	runtask->lastChangUserTime = time(NULL);
}

static void task_stop_push_msg(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	runtask->stopPushRecord = true;
	sqlite3_close(runtask->dbConn);
	runtask->dbConn = NULL;

	char buf[512] = { 0x0 };
	int n = 0;
	if (taskcfg->loopMode == 0)
		n = snprintf(buf + 8, sizeof(buf) - 8, "{\"TaskID\":%d,\"Loop\":true}", taskcfg->taskID);
	else
		n = snprintf(buf + 8, sizeof(buf) - 8, "{\"TaskID\":%d,\"Loop\":false}", taskcfg->taskID);

	*(int*)buf = n + 8;
	*(int*)(buf + 4) = 8;

	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	for (; iter != StressClientMap->end(); ++iter)
	{
		if (iter->second->projectid != taskcfg->projectID)
			continue;
		IOCPPostSendEx(iter->first, buf, n + 8);
	}
	StressClientMapLock->unlock();
}

static void task_get_record_msg(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	int rowcount = 0;
	if (runtask->sqllen != 0)
	{
		snprintf(runtask->dbsql + runtask->sqllen, size_t(SQL_SIZE) - runtask->sqllen, " order by recordtime limit %d,100", runtask->startRow);

		sqlite3_stmt* stmt = NULL;
		int result = sqlite3_prepare_v2(runtask->dbConn, runtask->dbsql, -1, &stmt, NULL);
		if (result == SQLITE_OK)
		{
			while (sqlite3_step(stmt) == SQLITE_ROW)
			{
				rowcount++;
				RecordMsg msg;
				msg.recordTime = sqlite3_column_int64(stmt, 0);
				msg.recordType = sqlite3_column_int(stmt, 1);
				const char* ip = (const char*)sqlite3_column_text(stmt, 2);
				snprintf(msg.ip, sizeof(msg.ip), ip);
				msg.port = sqlite3_column_int(stmt, 3);
				const char* content = (const char*)sqlite3_column_text(stmt, 4);
				msg.contentLen = strlen(content);
				msg.content = (char*)sheeps_malloc(msg.contentLen + 1);
				memcpy(msg.content, content, msg.contentLen);
				runtask->tempMsg->push_back(msg);
			}
		}
		sqlite3_finalize(stmt);
		runtask->startRow += rowcount;
	}
	
	if (rowcount == 0 && taskcfg->loopMode != 2)
	{
		task_stop_push_msg(runtask, taskcfg);
	}
}

static void task_chang_record_msg_addr(HTASKRUN runtask, HTASKCONFIG taskcfg, RecordMsg& msg)
{
	char chang[32] = { 0x0 };
	snprintf(chang, sizeof(chang), "%s:%d", msg.ip, msg.port);
	std::map<std::string, Readdr>::iterator iter = taskcfg->changeAddr->find(chang);
	if (iter != taskcfg->changeAddr->end())
	{
		Readdr dst = iter->second;
		char* p = strstr(dst.dstAddr, ":");
		if (p)
		{
			memcpy(msg.ip, dst.dstAddr, p - dst.dstAddr);
			msg.port = atoi(p + 1);
		}
	}
}

static void task_push_record_msg_to_client(HTASKRUN runtask, HTASKCONFIG taskcfg, RecordMsg &msg)
{
	task_chang_record_msg_addr(runtask, taskcfg, msg);

	size_t alloc_len = msg.contentLen + 256;
	char* buf = (char*)sheeps_malloc(alloc_len);
	int n = 0;
	if (msg.recordType == 4)
	{
		msg.recordType = 1;
		n = snprintf(buf + 8, alloc_len - 8, "{\"TaskID\":%d,\"Host\":\"%s\",\"Port\":%d,\"Timestamp\":%lld,\"Microsecond\":%lld,\"Msg\":\"%s\",\"Iotype\":\"UDP\"}",
			taskcfg->taskID, msg.ip, msg.port, msg.recordTime / 1000000, msg.recordTime % 1000000, msg.content);
	}
	else
	{
		n = snprintf(buf + 8, alloc_len - 8, "{\"TaskID\":%d,\"Host\":\"%s\",\"Port\":%d,\"Timestamp\":%lld,\"Microsecond\":%lld,\"Msg\":\"%s\"}",
			taskcfg->taskID, msg.ip, msg.port, msg.recordTime / 1000000, msg.recordTime % 1000000, msg.content);
	}
	*(int*)buf = n + 8;
	*(int*)(buf + 4) = msg.recordType + 3;

	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	for (; iter != StressClientMap->end(); ++iter)
	{
		if (iter->second->projectid != taskcfg->projectID)
			continue;
		IOCPPostSendEx(iter->first, buf, n + 8);
	}
	StressClientMapLock->unlock();
	sheeps_free(buf);
}

static void task_push_record_msg(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	if (runtask->stopPushRecord)
		return;
	if (runtask->tempMsg->empty())
		task_get_record_msg(runtask, taskcfg);
	if (runtask->tempMsg->empty())
		return;
	RecordMsg msg = runtask->tempMsg->front();
	if (runtask->startReal == 0)
	{
		runtask->startReal = GetSysTimeMicros();
		runtask->startRecord = msg.recordTime;
	}
	time_t shouldtime = msg.recordTime - runtask->startRecord;
	time_t realtime = GetSysTimeMicros() - runtask->startReal;
	if (shouldtime > realtime)
		return;
	task_push_record_msg_to_client(runtask, taskcfg, msg);

	sheeps_free(msg.content);
	runtask->tempMsg->pop_front();
}

static void task_push_message(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	task_push_add_once_user(runtask, taskcfg);
	task_push_record_msg(runtask, taskcfg);
}

static void task_push_over(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	taskcfg->taskState = STATE_STOP;

	char buf[256] = { 0x0 };
	int n = snprintf(buf + 8, sizeof(buf) - 8, "{\"TaskID\":%d}", taskcfg->taskID);
	*(int*)buf = n + 8;
	*(int*)(buf + 4) = 6;

	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	for (; iter != StressClientMap->end(); ++iter)
	{
		if (iter->second->projectid != taskcfg->projectID)
			continue;
		IOCPPostSendEx(iter->first, buf, n + 8);
	}
	StressClientMapLock->unlock();

	while (runtask->tempMsg && runtask->tempMsg->size())
	{
		RecordMsg msg = runtask->tempMsg->front();
		sheeps_free(msg.content);
		runtask->tempMsg->pop_front();
	}
	if (runtask->tempMsg)
		delete runtask->tempMsg;

	if (runtask->dbsql)
		sheeps_free(runtask->dbsql);
	if (runtask->dbConn)
		sqlite3_close(runtask->dbConn);
	runtask->stage = STAGE_OVER;
}

static void task_runing(HTASKRUN runtask, HTASKCONFIG taskcfg)
{
	switch (runtask->stage)
	{
	case STAGE_INIT:
		task_push_init(runtask, taskcfg);
		break;
	case STAGE_LOOP:
		task_push_message(runtask, taskcfg);
		break;
	case STAGE_CLEAN:
		task_push_over(runtask, taskcfg);
	default:
		break;
	}
}

DWORD WINAPI task_runing_thread(LPVOID pParam)
{
	HTASKRUN runtask = (HTASKRUN)pParam;
	while (runtask->stage < STAGE_OVER)
	{
		Sleep(1);
		task_runing(runtask, runtask->taskCfg);
	}
	sheeps_free(runtask);
	TaskRunLock->lock();
	TaskRun.erase(runtask->taskCfg->taskID);
	TaskRunLock->unlock();
	return 0;
}

static void send_console_msg(HSOCKET hsock, cJSON* root)
{
	char* data = cJSON_PrintUnformatted(root);
	char buf[128] = { 0x0 };
	int clen = (int)strlen(data);
	int n = snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin:*\r\nContent-Type: application/json\r\nContent-Lenth: %zd\r\n\r\n", strlen(data));
	IOCPPostSendEx(hsock, buf, n);
	IOCPPostSendEx(hsock, data, clen);
	sheeps_free(data);
	cJSON_Delete(root);
}

static void do_stress_client_list(HSOCKET hsock, cJSON* root, char* uri)
{
	LOG(slogid, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON* array = cJSON_CreateArray();
	if (array == NULL)
	{
		cJSON_Delete(res);
		return;
	}
	cJSON_AddItemToObject(res, "data", array);

	char addr[32] = { 0x0 };
	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	for (; iter != StressClientMap->end(); ++iter)
	{
		HCLIENTINFO info = iter->second;

		cJSON* item = cJSON_CreateObject();
		if (item == NULL)
			continue;

		memset(addr, 0, sizeof(addr));
		snprintf(addr, sizeof(addr), "%s:%d", info->ip, info->port);
		cJSON_AddStringToObject(item, "addr", addr);
		cJSON_AddNumberToObject(item, "cpu", info->cpu);
		cJSON_AddNumberToObject(item, "ready", info->ready);
		cJSON_AddItemToArray(array, item);
	}
	StressClientMapLock->unlock();
	send_console_msg(hsock, res);
}

static void do_stress_client_local(HSOCKET hsock, cJSON* root, char* uri)
{
	stressfc->ClientRunOrStop("127.0.0.1", 1080);

	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_stress_client_log(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* level = cJSON_GetObjectItem(root, "log_level");
	cJSON* taskid = cJSON_GetObjectItem(root, "task_id");
	if (level == NULL ||taskid == NULL || level->type != cJSON_Number ||taskid->type != cJSON_Number)
		return;

	char buf[128] = { 0x0 };
	int n = snprintf(buf + 8, sizeof(buf) - 8, "{\"LogLevel\":%d, \"TaskID\":%d}", level->valueint, taskid->valueint);
	*(int*)buf = n + 8;
	*(int*)(buf + 4) = 14;

	StressClientMapLock->lock();
	std::map<HSOCKET, HCLIENTINFO>::iterator iter = StressClientMap->begin();
	for (; iter != StressClientMap->end(); ++iter)
	{
		IOCPPostSendEx(iter->first, buf, n + 8);
	}
	StressClientMapLock->unlock();
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_console_task_create(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* projectid = cJSON_GetObjectItem(root, "project_id");
	cJSON* envid = cJSON_GetObjectItem(root, "env_id");
	cJSON* total = cJSON_GetObjectItem(root, "total_user");
	cJSON* once = cJSON_GetObjectItem(root, "once_user");
	cJSON* space = cJSON_GetObjectItem(root, "space_time");
	cJSON* loop = cJSON_GetObjectItem(root, "loop_mode");
	cJSON* ignor = cJSON_GetObjectItem(root, "ignor_error");
	cJSON* dbfile = cJSON_GetObjectItem(root, "db_file");
	cJSON* des = cJSON_GetObjectItem(root, "des");
	if (projectid == NULL || envid == NULL || total == NULL || once == NULL || space == NULL || loop == NULL || ignor == NULL || dbfile == NULL || des == NULL||
		projectid->type != cJSON_Number || envid->type != cJSON_Number || total->type != cJSON_Number || once->type != cJSON_Number ||
		space->type != cJSON_Number || loop->type != cJSON_Number || ignor->type != cJSON_Number || dbfile->type != cJSON_String ||des->type != cJSON_String)
	{
		return;
	}

	HTASKCONFIG task = (HTASKCONFIG)sheeps_malloc(sizeof(TaskConfig));
	if (task == NULL)
		return;
	TaskCfgLock->lock();
	if (taskIndex == 0 && taskIndexPool.empty())
	{
		sheeps_free(task);
		TaskCfgLock->unlock();
		return;
	}
	else if (taskIndex == 0)
	{
		std::map<uint8_t, uint8_t>::iterator iter = taskIndexPool.begin();
		task->taskID = iter->first;
		taskIndexPool.erase(iter);
	}
	else
		task->taskID = taskIndex++;
	task->projectID = projectid->valueint;
	task->envID = envid->valueint;
	task->totalUser = total->valueint;
	task->onceUser = once->valueint;
	task->spaceTime = space->valueint;
	task->loopMode = loop->valueint;
	task->ignoreErr = ignor->valueint == 0? false:true;
	snprintf(task->dbName, sizeof(task->dbName), dbfile->valuestring);
	snprintf(task->taskDes, sizeof(task->taskDes), des->valuestring);
	task->replayAddr = new std::list<Readdr>;
	task->changeAddr = new std::map<std::string, Readdr>;
	cJSON* array = cJSON_GetObjectItem(root, "replay");
	if (array != NULL && array->type == cJSON_Array)
	{
		for (int i = 0; i < cJSON_GetArraySize(array); i++)
		{
			cJSON* item = cJSON_GetArrayItem(array, i);
			if (item == NULL || item->type != cJSON_Object)
				continue;

			cJSON* src = cJSON_GetObjectItem(item, "src");
			if (src == NULL || src->type != cJSON_String)
				continue;
			Readdr addr = { 0x0 };
			snprintf(addr.srcAddr, sizeof(addr.srcAddr), src->valuestring);

			cJSON* dst = cJSON_GetObjectItem(item, "dst");
			if (dst != NULL && src->type == cJSON_String)
			{
				snprintf(addr.dstAddr, sizeof(addr.dstAddr), dst->valuestring);
				task->changeAddr->insert(std::pair<std::string, Readdr>(std::string(src->valuestring), addr));
			}
			task->replayAddr->push_back(addr);
		}
	}
	TaskCfg.insert(std::pair<uint8_t, HTASKCONFIG>(task->taskID, task));
	TaskCfgLock->unlock();
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_console_task_info(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* taskid = cJSON_GetObjectItem(root, "task_id");
	if (taskid == NULL || taskid->type != cJSON_Number)
		return;

	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	std::map<uint8_t, HTASKCONFIG>::iterator iter;
	iter = TaskCfg.find(taskid->valueint);
	if (iter == TaskCfg.end())
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}
	HTASKCONFIG taskcfg = iter->second;
	cJSON* data = cJSON_CreateObject();
	if (data == NULL)
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}
	cJSON_AddItemToObject(res, "data", data);
	cJSON_AddNumberToObject(data, "task_id", taskcfg->taskID);
	cJSON_AddStringToObject(data, "des", taskcfg->taskDes);
	cJSON_AddNumberToObject(data, "project_id", taskcfg->projectID);
	cJSON_AddNumberToObject(data, "total_user", taskcfg->totalUser);
	cJSON_AddNumberToObject(data, "once_user", taskcfg->onceUser);
	cJSON_AddNumberToObject(data, "space_time", taskcfg->spaceTime);
	cJSON_AddNumberToObject(data, "loop_mode", taskcfg->loopMode);
	cJSON_AddNumberToObject(data, "ignor_error", taskcfg->ignoreErr);
	cJSON_AddStringToObject(data, "db_file", taskcfg->dbName);
	cJSON* replay = cJSON_CreateArray();
	if (replay == NULL)
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}
	cJSON_AddItemToObject(data, "replay", replay);
	std::list<Readdr>::iterator it = taskcfg->replayAddr->begin();
	for (;it != taskcfg->replayAddr->end(); ++it)
	{
		cJSON* item = cJSON_CreateObject();
		if (item == NULL) continue;
		cJSON_AddStringToObject(item, "src", it->srcAddr);
		cJSON_AddStringToObject(item, "dst", it->dstAddr);
		cJSON_AddItemToArray(replay, item);
	}
	send_console_msg(hsock, res);
}

static void do_console_task_user(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* taskid = cJSON_GetObjectItem(root, "task_id");
	cJSON* count = cJSON_GetObjectItem(root, "user_count");
	if (taskid == NULL || count == NULL || taskid->type != cJSON_Number ||count->type != cJSON_Number)
		return;

	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	std::map<uint8_t, HTASKCONFIG>::iterator iter;
	iter = TaskCfg.find(taskid->valueint);
	if (iter != TaskCfg.end())
	{
		HTASKCONFIG taskcfg = iter->second;
		taskcfg->totalUser += count->valueint;
		if (taskcfg->totalUser < 0)
			taskcfg->totalUser = 0;
	}
	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_console_task_delete(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* taskid = cJSON_GetObjectItem(root, "task_id");
	if (taskid == NULL || taskid->type != cJSON_Number)
		return;

	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	std::map<uint8_t, HTASKRUN>::iterator it;
	it = TaskRun.find(taskid->valueint);
	if (it != TaskRun.end())
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}

	TaskCfgLock->lock();
	std::map<uint8_t, HTASKCONFIG>::iterator iter;
	iter = TaskCfg.find(taskid->valueint);
	if (iter != TaskCfg.end())
	{
		HTASKCONFIG taskcfg = iter->second;
		taskIndexPool.insert(std::pair<uint8_t, uint8_t>(taskcfg->taskID, taskcfg->taskID));
		taskcfg->replayAddr->clear();
		delete taskcfg->replayAddr;
		delete taskcfg->changeAddr;
		sheeps_free(taskcfg);
		TaskCfg.erase(iter);
	}
	TaskCfgLock->unlock();
	
	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_console_task_list(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON* array = cJSON_CreateArray();
	if (array == NULL)
	{
		cJSON_Delete(res);
		return;
	}
	cJSON_AddItemToObject(res, "data", array);

	std::map<uint8_t, HTASKCONFIG>::iterator iter = TaskCfg.begin();
	for (; iter != TaskCfg.end(); ++iter)
	{
		HTASKCONFIG info = iter->second;

		cJSON* item = cJSON_CreateObject();
		if (item == NULL)
			continue;
		cJSON_AddNumberToObject(item, "task_id", info->taskID);
		cJSON_AddNumberToObject(item,"state", info->taskState);
		cJSON_AddStringToObject(item, "des", info->taskDes);
		cJSON_AddItemToArray(array, item);
	}
	send_console_msg(hsock, res);
}

static void do_console_task_run(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* taskid = cJSON_GetObjectItem(root, "task_id");
	if (taskid == NULL || taskid->type != cJSON_Number)
		return;
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	std::map<uint8_t, HTASKRUN>::iterator it;
	it = TaskRun.find(taskid->valueint);
	if (it != TaskRun.end())
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}

	std::map<uint8_t, HTASKCONFIG>::iterator iter;
	iter = TaskCfg.find(taskid->valueint);
	if (iter == TaskCfg.end())
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}
	HTASKCONFIG taskcfg = iter->second;

	HTASKRUN taskrun = (HTASKRUN)sheeps_malloc(sizeof(TaskRuning));
	if (taskrun == NULL)
	{
		return;
	}
	taskrun->taskCfg = iter->second;
	TaskRunLock->lock();
	TaskRun.insert(std::pair<uint8_t, HTASKRUN>(taskcfg->taskID, taskrun));
	TaskRunLock->unlock();

	HANDLE ThreadHandle;
	ThreadHandle = CreateThread(NULL, 0, task_runing_thread, taskrun, 0, NULL);
	if (NULL == ThreadHandle) {
		LOG(slogid, LOG_ERROR, "%s-%d create thread failed!", __func__, __LINE__);
		return;
	}
	CloseHandle(ThreadHandle);

	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_console_task_stop(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* taskid = cJSON_GetObjectItem(root, "task_id");
	if (taskid == NULL || taskid->type != cJSON_Number)
		return;
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	std::map<uint8_t, HTASKRUN>::iterator iter;
	TaskRunLock->lock();
	iter = TaskRun.find(taskid->valueint);
	if (iter == TaskRun.end())
	{
		TaskRunLock->unlock();
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}
	HTASKRUN taskrun = iter->second;
	taskrun->stage = STAGE_CLEAN;
	TaskRunLock->unlock();

	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_proxy_addr_list(HSOCKET hsock, cJSON* root, char* uri)
{
	LOG(slogid, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON* array = cJSON_CreateArray();
	if (array == NULL)
	{
		cJSON_Delete(res);
		return;
	}
	cJSON_AddItemToObject(res, "data", array);

	char addr[32] = { 0x0 };
	ProxyMapLock->lock();
	std::map<HSOCKET, HPROXYINFO>::iterator iter = ProxyMap->begin();
	for (; iter != ProxyMap->end(); ++iter)
	{
		HPROXYINFO info = iter->second;

		cJSON* item = cJSON_CreateObject();
		if (item == NULL)
			continue;
		
		memset(addr, 0, sizeof(addr));
		snprintf(addr, sizeof(addr), "%s:%d", info->serverip, info->serverport);
		cJSON_AddStringToObject(item, "addr", addr);
		cJSON_AddItemToArray(array, item);
	}
	ProxyMapLock->unlock();
	send_console_msg(hsock, res);
}

static void do_proxy_record_list(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* dbfile = cJSON_GetObjectItem(root, "db_file");
	if (dbfile == NULL || dbfile->type != cJSON_String)
		return;

	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON* array = cJSON_CreateArray();
	if (array == NULL)
	{
		cJSON_Delete(res);
		return;
	}
	cJSON_AddItemToObject(res, "data", array);

	sqlite3* dbConn;
	char fullpath[256] = { 0x0 };
	snprintf(fullpath, sizeof(fullpath), "%s\\%s", RecordPath, dbfile->valuestring);
	my_sqlite3_open(fullpath, &dbConn);

	const char* sql = "select name from sqlite_sequence";
	sqlite3_stmt* stmt = NULL;
	int result = sqlite3_prepare_v2(dbConn, sql, -1, &stmt, NULL);
	if (result == SQLITE_OK)
	{
		char* addr = NULL;
		while (sqlite3_step(stmt) == SQLITE_ROW)
		{
			const char* table = (const char*)sqlite3_column_text(stmt, 0);
			
			char buf[64] = { 0x0 };
			snprintf(buf, sizeof(buf), "%s", table);
			char* p = strstr(buf, "_");
			if (p == NULL)
				continue;
			addr = p + 1;
			while (*p)
			{
				if (*p == '_')
					*p = '.';
				if (*p == 'p')
				{
					*p = ':';
					break;
				}
				p++;
			}

			cJSON* item = cJSON_CreateObject();
			if (item == NULL)
				continue;
			cJSON_AddStringToObject(item, "addr", addr);
			cJSON_AddItemToArray(array, item);
		}
	}
    sqlite3_finalize(stmt);
	sqlite3_close(dbConn);
	send_console_msg(hsock, res);
}

static void do_proxy_record_chang(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	cJSON* record = cJSON_GetObjectItem(root, "record");
	if (record != NULL && (record->type == cJSON_True || record->type == cJSON_False))
	{
		Proxy_record = record->valueint == 0 ? false : true;
	}
	
	cJSON_AddStringToObject(res, "ret", "OK");
	cJSON_AddBoolToObject(res, "record", Proxy_record);
	cJSON_AddNumberToObject(res, "left", (int)recordList->size());
	send_console_msg(hsock, res);
}

static void do_proxy_record_delete(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* addr = cJSON_GetObjectItem(root, "addr");
	cJSON* dbfile = cJSON_GetObjectItem(root, "db_file");
	if (addr == NULL || dbfile == NULL || addr->type != cJSON_Array || dbfile->type != cJSON_String)
		return;
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	char file[256] = { 0x0 };
	snprintf(file, sizeof(file), "%s\\%s", RecordPath, dbfile->valuestring);
	sqlite3* dbConn;
	my_sqlite3_open(file, &dbConn);

	char table[64] = { 0x0 };
	char sql[128] = { 0x0 };
	int size = cJSON_GetArraySize(addr);
	for (int i = 0; i < size; i++)
	{
		cJSON* item = cJSON_GetArrayItem(addr, i);
		memset(table, 0, sizeof(table));
		int n = snprintf(table, sizeof(table), "record_%s", item->valuestring);
		std::replace(table, table + n, '.', '_');
		std::replace(table, table + n, ':', 'p');

		memset(sql, 0, sizeof(sql));
		snprintf(sql, sizeof(sql), "DROP TABLE %s", table);

		int ret = sqlite3_exec(dbConn, sql, NULL, NULL, NULL);
	}
	sqlite3_close(dbConn);
	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_proxy_database_file(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;
	cJSON* array = cJSON_CreateArray();
	if (array == NULL)
	{
		cJSON_Delete(res);
		return;
	}
	cJSON_AddItemToObject(res, "data", array);

	intptr_t hFile = 0;
	struct _finddata_t fileinfo;
	char file[256] = { 0x0 };
	snprintf(file, sizeof(file), "%s\\*", RecordPath);
	if ((hFile = _findfirst(file, &fileinfo)) != -1)
	{
		do
		{
			if ((fileinfo.attrib & _A_SUBDIR))
			{
				continue;
			}
			else
			{
				cJSON_AddItemToArray(array, cJSON_CreateString(fileinfo.name));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}

	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_proxy_database_delete(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* dbfile = cJSON_GetObjectItem(root, "db_file");
	if (dbfile == NULL || dbfile->type != cJSON_String)
		return;
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	if (strcmp(dbfile->valuestring, record_database) == 0)
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}

	char fullpath[256] = { 0x0 };
	snprintf(fullpath, sizeof(fullpath), "%s\\%s", RecordPath, dbfile->valuestring);
	int ret = remove(fullpath);
	if (ret != 0)
	{
		cJSON_AddStringToObject(res, "ret", "Failed");
		send_console_msg(hsock, res);
		return;
	}

	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

static void do_proxy_database_name(HSOCKET hsock, cJSON* root, char* uri)
{
	cJSON* dbfile = cJSON_GetObjectItem(root, "db_file");
	cJSON* newname = cJSON_GetObjectItem(root, "new_name");
	if (dbfile == NULL|| newname == NULL || dbfile->type != cJSON_String || newname->type != cJSON_String)
		return;
	cJSON* res = cJSON_CreateObject();
	if (res == NULL)
		return;

	if (strcmp(dbfile->valuestring, record_database) == 0)
	{
		if (ChangeDatabaseName(newname->valuestring) == false)
		{
			cJSON_AddStringToObject(res, "ret", "Failed");
			send_console_msg(hsock, res);
			return;
		}
	}
	else
	{
		char oldfile[256] = { 0x0 };
		char newfile[256] = { 0x0 };
		snprintf(oldfile, sizeof(oldfile), "%s\\%s", RecordPath, dbfile->valuestring);
		snprintf(newfile, sizeof(newfile), "%s\\%s", RecordPath, newname->valuestring);
		int ret = rename(oldfile, newfile);
		if (ret != 0)
		{
			cJSON_AddStringToObject(res, "ret", "Failed");
			send_console_msg(hsock, res);
			return;
		}
	}

	cJSON_AddStringToObject(res, "ret", "OK");
	send_console_msg(hsock, res);
}

std::map<std::string, serverConsole_cmd_cb> ConsoleFunc{
	{std::string("/api/client_list"),	do_stress_client_list},		//受控端列表
	{std::string("/api/client_open"),	do_stress_client_local},	//开启或者关闭本地受控端
	{std::string("/api/task_log"),		do_stress_client_log},		//修改受控端日志等级
	{std::string("/api/task_create"),	do_console_task_create},	//创建任务
	{std::string("/api/task_info"),		do_console_task_info},
	{std::string("/api/task_user"),		do_console_task_user},		//增减任务人数
	{std::string("/api/task_delete"),	do_console_task_delete},	//删除任务
	{std::string("/api/task_list"),		do_console_task_list},		//任务列表
	{std::string("/api/task_run"),		do_console_task_run},		//任务开始
	{std::string("/api/task_stop"),		do_console_task_stop},		//任务停止
	{std::string("/api/proxy_list"),	do_proxy_addr_list},		//代理列表
	{std::string("/api/record_list"),	do_proxy_record_list},		//db内录制表
	{std::string("/api/record_change"),	do_proxy_record_chang},		//是否录制
	{std::string("/api/record_delete"), do_proxy_record_delete},	//删除db录制表
	{std::string("/api/db_list"),		do_proxy_database_file},	//db文件列表
	{std::string("/api/db_delete"),		do_proxy_database_delete},	//删除db
	{std::string("/api/db_rename"),		do_proxy_database_name}		//db改名
};

static void do_request_by_uri(HSOCKET hsock, cJSON* root, char* uri)
{
	std::map<std::string, serverConsole_cmd_cb>::iterator iter = ConsoleFunc.find(uri);
	if (iter != ConsoleFunc.end())
		iter->second(hsock, root, uri);
}

static int get_content_length(const char* http_stream)
{
	const char* k = "Content-Length: ";
	char v[32] = { 0x0 };
	char* t = strstr((char*)http_stream, k);
	if (t == NULL)
		return 0;

	char* e = strstr(t, "\r\n");
	if (e == NULL)
		return 0;
	*e = 0x0;

	snprintf(v, sizeof(v), "%s", t + strlen(k));
	*e = '\r';
	return atoi(v);
}

static int get_uri_string(const char* http_stream, char* buff, size_t bufflen)
{
	const char* p = strstr(http_stream, " ");
	if (p == NULL)
		return -1;
	p++;
	const char* e = strstr(p, " ");
	if (e == NULL)
		return -1;
	const char* s = strstr(p, "?");
	if (s == NULL || s > e)
		memcpy(buff, p, e - p);
	else
		memcpy(buff, p, s - p);
	return 0;
}

static int get_home_page(HSOCKET hsock)
{	
#define PAGESIZE 30*1024
#define SPACELEN 100
	char* buf = (char*)sheeps_malloc(PAGESIZE);

	char file[256] = { 0x0 };
	snprintf(file, sizeof(file), "%s\\console.html", DllPath);

	FILE* hfile = NULL;
	int ret = fopen_s(&hfile, file, "rb");
	if (ret != 0)
	{
		LOG(slogid, LOG_ERROR, "%s:%d open file error\r\n", __func__, __LINE__);
		sheeps_free(buf);
		IOCPCloseHsocket(hsock);
		return 0;
	}
	int flen = (int)fread(buf + SPACELEN, sizeof(char), PAGESIZE - SPACELEN, hfile);
	int offset = snprintf(buf, SPACELEN, "HTTP/1.1 200 OK\r\nContent-Length:%d\r\n\r\n", flen);
	char* s = buf + SPACELEN - offset;
	memmove(s, buf, offset);
	IOCPPostSendEx(hsock, s, offset + flen);

	fclose(hfile);
	sheeps_free(buf);
	IOCPCloseHsocket(hsock);
	return 0;
}

int CheckConsoleRequest(HSOCKET hsock, ServerProtocol* proto, const char* data, int len)
{
	if (len < 4)
		return 0;
	const char* p = strstr(data, "\r\n\r\n");
	if (p == NULL)
		return 0;
	int hlen = int(p + 4 - data);
	int clen = get_content_length(data);
	if (len < hlen + clen)
		return 0;

	char uri[256] = { 0x0 };
	get_uri_string(data, uri, sizeof(uri));
	if (strcmp(uri, "/") == 0)
		return get_home_page(hsock);

	char* content = (char*)sheeps_malloc((size_t)len + 1);
	if (content == NULL)
		return 0;
	memcpy(content, p + 4, len);
	LOG(slogid, LOG_DEBUG, "%s:%d uri[%s] data[%s]\r\n", __func__, __LINE__, uri, content);
	cJSON* root = cJSON_Parse(content);
	if (root)
	{
		do_request_by_uri(hsock, root, uri);
	}

	sheeps_free(content);
	cJSON_Delete(root);
	IOCPCloseHsocket(hsock);
	return 0;
}