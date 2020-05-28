#include "pch.h"
#include "ClientProtocol.h"
#include "ClientProtocolSub.h"
#include "../common/cJSON.h"
#include "../common/common.h"
#include "./../common/log.h"
#include "./../common/TaskManager.h"

StressProtocol::StressProtocol()
{
}


StressProtocol::~StressProtocol()
{
}


//固有函数，继承自基类
void StressProtocol::ConnectionMade(HSOCKET hsock, const char* ip, int port)
{
	LOG(clogId, LOG_DEBUG, "stress server connection made：[%s:%d]\r\n", ip, port);
	StressHsocket = hsock;

	SYSTEM_INFO sysInfor;
	GetSystemInfo(&sysInfor);
	char data[64] = {0x0};
	snprintf(data, sizeof(data), "{\"CPU\":%d}", sysInfor.dwNumberOfProcessors);

	int len = int(strlen(data));
	t_stress_protocol_head head;
	head.msgLen = sizeof(t_stress_protocol_head) + len;
	head.cmdNo = 1;
	char buf[128] = { 0x0 };
	::memcpy(buf, (char*)&head, sizeof(t_stress_protocol_head));
	::memcpy(buf + sizeof(t_stress_protocol_head), data, len);
	IOCPPostSendEx(hsock, buf, len + 8);
	//send(sock, buf, len + 8, 0);

	subfactory = this->_factory;
}

void StressProtocol::ConnectionFailed(HSOCKET sock, const char* ip, int port)
{
	//UDPLOG("stress server connection failed:[%s:%d]\n", ip, port);
	//LOG(logId, LOG_DEBUG, "stress server connection failed:[%s:%d]\r\n", ip, port);
	this->StressHsocket = NULL;
}

void StressProtocol::ConnectionClosed(HSOCKET hsock, const char* ip, int port)
{
	LOG(clogId, LOG_DEBUG, "stress server connection closed：[%s:%d] socket = %lld\r\n", ip, port, hsock->sock);
	this->StressHsocket = NULL;
	/*if (this->SelfDead == TRUE)
		IOCPDestroyProto(this);*/
}

void StressProtocol::Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len)
{
	return this->CheckReq(hsock, data, len);
}

int StressProtocol::Loop()
{
	if (this->StressHsocket == NULL )
	{
		char ip[20] = { 0x0 };
		GetHostByName((char*)this->StressSerIP, ip, sizeof(ip));
		this->StressHsocket = IOCPConnectEx(ip, this->StressSerPort, this, IOCP_TCP);
	}

	destroyTask();   //定期清理已经结束的任务，释放资源
	this->ReportError();
	return 0;
}

int StressProtocol::Destroy()
{
	if (this->StressHsocket != NULL)
	{
		IOCPCloseHsocket(this->StressHsocket);
	}
	return  0;
}


//自定义类成员函数
bool StressProtocol::ReportError()
{
	if (this->StressHsocket == NULL)
		return false;
	t_task_error* err;
	while (err = get_task_error_front())
	{
		char data[1024] = { 0x0 };
		int len = snprintf(data, sizeof(data), "{\"TaskID\":%d,\"UserID\":%d,\"Timestamp\":%d,\"detail\":\"%s\"}", err->taskID, err->userID, err->timestamp, err->errMsg);
		t_stress_protocol_head head;
		head.msgLen = sizeof(t_stress_protocol_head) + len;
		head.cmdNo = 13;
		char buf[256] = { 0x0 };
		::memcpy(buf, (char*)&head, sizeof(t_stress_protocol_head));
		::memcpy(buf + sizeof(t_stress_protocol_head), data, len);
		IOCPPostSendEx(this->StressHsocket, buf, len + 8);
		//LOG("Report error %d %d", err->taskID, err->taskErrId);
		delete_task_error(err);
	}
	return true;
}

void StressProtocol::CheckReq(HSOCKET sock, const char* data, int len)
{
	this->recvBuff += std::string(data, len);
	int clen = 0;
	while (true)
	{
		clen = this->CheckRequest(sock, this->recvBuff.c_str(), (int)this->recvBuff.size());
		if (clen > 0)
			this->recvBuff = this->recvBuff.substr(clen);
		else
			return;
	}
}

int StressProtocol::CheckRequest(HSOCKET sock, const char* data, int len)
{
	if (len < sizeof(t_stress_protocol_head))
		return 0;
	t_stress_protocol_head head;
	memcpy(&head, data, sizeof(t_stress_protocol_head));
	if (len < head.msgLen)
		return 0;
	int clen = head.msgLen - sizeof(t_stress_protocol_head);
	char* body = (char*)malloc(clen + 1);
	if (body == NULL)
		return -1;
	memset(body, 0, clen);
	memcpy(body, data + sizeof(t_stress_protocol_head), clen);
	LOG(clogId, LOG_TRACE, "stress client recv [%d %d:%s]\r\n", head.msgLen, head.cmdNo, body);

	cJSON * root = cJSON_Parse(body);
	if (root == NULL)
	{
		LOG(clogId, LOG_ERROR, "json encoding error\r\n");
		return -1;
	}
	do_client_func_by_cmd(sock, head.cmdNo, root);
	free(body);
	cJSON_Delete(root);
	return head.msgLen;
}
