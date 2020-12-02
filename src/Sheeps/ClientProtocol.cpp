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

	HsocketSend(hsock, "sheeps", 6);

	char data[64] = {0x0};
	snprintf(data, sizeof(data), "{\"CPU\":%d, \"ProjectID\":%d}", GetCpuCount(), this->ProjectID);

	int len = int(strlen(data));
	t_stress_protocol_head head = {0x0};
	head.msgLen = sizeof(t_stress_protocol_head) + len;
	head.cmdNo = 1;
	char buf[128] = { 0x0 };
	memcpy(buf, (char*)&head, sizeof(t_stress_protocol_head));
	memcpy(buf + sizeof(t_stress_protocol_head), data, len);
	HsocketSend(hsock, buf, len + 8);

	this->heartbeat = time(NULL);
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
		this->StressHsocket = HsocketConnect(this, ip, this->StressSerPort, TCP_CONN);
	}
	else
	{
		if (TaskManagerRuning)
		{
			if (time(NULL) - this->heartbeat < 30)
				return 0;
			this->heartbeat = time(NULL);
			char data[64] = { 0x0 };
			snprintf(data, sizeof(data), "{\"timestamp\":%lld}", time(NULL));

			int len = int(strlen(data));
			t_stress_protocol_head head = {0x0};
			head.msgLen = sizeof(t_stress_protocol_head) + len;
			head.cmdNo = 0;
			char buf[128] = { 0x0 };
			memcpy(buf, (char*)&head, sizeof(t_stress_protocol_head));
			memcpy(buf + sizeof(t_stress_protocol_head), data, len);
			HsocketSend(this->StressHsocket, buf, len + 8);
		}
	}
	return 0;
}

int ClientProtocol::Destroy()
{
	if (this->StressHsocket != NULL)
	{
		HsocketClose(this->StressHsocket);
	}
	return  0;
}


//自定义类成员函数

void ClientProtocol::CheckReq(HSOCKET hsock, const char* data, int len)
{
	int packlen = 0;
	while (len > 0)
	{
		packlen = this->CheckRequest(hsock, data, len);
		if (packlen > 0)
		{
			len = HsocketSkipBuf(hsock, packlen);
		}
		else if (packlen < 0)
			HsocketClose(hsock);
		else
			break;
	}
}

int ClientProtocol::CheckRequest(HSOCKET hsock, const char* data, int len)
{
	if (len < (int)sizeof(t_stress_protocol_head))
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
	if ( head.cmdNo > 0)
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
