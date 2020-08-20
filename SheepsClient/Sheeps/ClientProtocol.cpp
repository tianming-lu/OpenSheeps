#include "pch.h"
#include "ClientProtocol.h"
#include "ClientProtocolSub.h"
#include "TaskManager.h"
#include "SheepsMemory.h"

ClientProtocol::ClientProtocol()
{
}


ClientProtocol::~ClientProtocol()
{
}


//固有函数，继承自基类
void ClientProtocol::ConnectionMade(HSOCKET hsock, const char* ip, int port)
{
	LOG(clogId, LOG_DEBUG, "stress server connection made：[%s:%d]\r\n", ip, port);
	StressHsocket = hsock;

	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	char data[64] = {0x0};
	snprintf(data, sizeof(data), "{\"CPU\":%d, \"ProjectID\":%d}", sysInfor.dwNumberOfProcessors, this->ProjectID);

	int len = int(strlen(data));
	t_stress_protocol_head head;
	head.msgLen = sizeof(t_stress_protocol_head) + len;
	head.cmdNo = 1;
	char buf[128] = { 0x0 };
	memcpy(buf, (char*)&head, sizeof(t_stress_protocol_head));
	memcpy(buf + sizeof(t_stress_protocol_head), data, len);
	IOCPPostSendEx(hsock, buf, len + 8);

	subfactory = this->_factory;
	TaskManagerRuning = true;
}

void ClientProtocol::ConnectionFailed(HSOCKET hsock, const char* ip, int port)
{
	//LOG(logId, LOG_DEBUG, "stress server connection failed:[%s:%d]\r\n", ip, port);
	this->StressHsocket = NULL;
}

void ClientProtocol::ConnectionClosed(HSOCKET hsock, const char* ip, int port)
{
	LOG(clogId, LOG_DEBUG, "stress server connection closed：[%s:%d] socket = %lld\r\n", ip, port, hsock->fd);
	this->StressHsocket = NULL;
	TaskManagerRuning = false;
}

void ClientProtocol::Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len)
{
	return this->CheckReq(hsock, data, len);
}

int ClientProtocol::Loop()
{
	if (this->StressHsocket == NULL )
	{
		char ip[20] = { 0x0 };
		GetHostByName((char*)this->StressSerIP, ip, sizeof(ip));
		this->StressHsocket = IOCPConnectEx(this, ip, this->StressSerPort, TCP_CONN);
	}

	this->ReportError();
	return 0;
}

int ClientProtocol::Destroy()
{
	if (this->StressHsocket != NULL)
	{
		IOCPCloseHsocket(this->StressHsocket);
	}
	return  0;
}


//自定义类成员函数
bool ClientProtocol::ReportError()
{
	if (this->StressHsocket == NULL)
		return false;
	t_task_error* err;
	while (err = get_task_error_front())
	{
		char data[1024] = { 0x0 };
		int len = snprintf(data, sizeof(data), "{\"TaskID\":%d,\"UserID\":%d,\"Timestamp\":%lld,\"detail\":\"%s\"}", err->taskID, err->userID, err->timeStamp, err->errMsg);
		t_stress_protocol_head head;
		head.msgLen = sizeof(t_stress_protocol_head) + len;
		head.cmdNo = 13;
		char buf[256] = { 0x0 };
		memcpy(buf, (char*)&head, sizeof(t_stress_protocol_head));
		memcpy(buf + sizeof(t_stress_protocol_head), data, len);
		IOCPPostSendEx(this->StressHsocket, buf, len + 8);
		//LOG("Report error %d %d", err->taskID, err->taskErrId);
		delete_task_error(err);
	}
	return true;
}

void ClientProtocol::CheckReq(HSOCKET hsock, const char* data, int len)
{
	int packlen = 0;
	while (len > 0)
	{
		packlen = this->CheckRequest(hsock, data, len);
		if (packlen > 0)
		{
			len = IOCPSkipHsocketBuf(hsock, packlen);
		}
		else if (packlen < 0)
			IOCPCloseHsocket(hsock);
		else
			break;
	}
}

int ClientProtocol::CheckRequest(HSOCKET hsock, const char* data, int len)
{
	if (len < sizeof(t_stress_protocol_head))
		return 0;
	t_stress_protocol_head head;
	memcpy(&head, data, sizeof(t_stress_protocol_head));
	if (len < head.msgLen)
		return 0;
	int clen = head.msgLen - sizeof(t_stress_protocol_head);
	char* body = (char*)sheeps_malloc((size_t)clen + 1);
	if (body == NULL)
		return -1;
	memcpy(body, data + sizeof(t_stress_protocol_head), clen);
	LOG(clogId, LOG_TRACE, "stress client recv [%d %d:%s]\r\n", head.msgLen, head.cmdNo, body);

	cJSON * root = cJSON_Parse(body);
	if (root == NULL)
	{
		LOG(clogId, LOG_ERROR, "json encoding error\r\n");
		sheeps_free(body);
		return -1;
	}
	do_client_func_by_cmd(hsock, head.cmdNo, root);
	sheeps_free(body);
	cJSON_Delete(root);
	return head.msgLen;
}
