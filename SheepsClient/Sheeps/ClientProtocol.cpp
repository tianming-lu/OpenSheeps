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
void StressProtocol::ProtoInit(void* parm, int index)
{
}

bool StressProtocol::ConnectionMade(HSOCKET hsock, char* ip, int port)
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

	subfactory = this->factory;
	return true;
}

bool StressProtocol::ConnectionFailed(HSOCKET sock, char* ip, int port)
{
	//UDPLOG("stress server connection failed:[%s:%d]\n", ip, port);
	//LOG(logId, LOG_DEBUG, "stress server connection failed:[%s:%d]\r\n", ip, port);
	this->StressHsocket = NULL;
	return true;
}

bool StressProtocol::ConnectionClosed(HSOCKET hsock, char* ip, int port)
{
	LOG(clogId, LOG_DEBUG, "stress server connection closed：[%s:%d] socket = %lld\r\n", ip, port, hsock->sock);
	this->StressHsocket = NULL;
	/*if (this->SelfDead == TRUE)
		IOCPDestroyProto(this);*/
	return true;
}

int StressProtocol::Send(HSOCKET hsock, char* ip, int port, char* data, int len)
{
	return true;
}

int StressProtocol::Recv(HSOCKET hsock, char* ip, int port, char* data, int len)
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

int StressProtocol::ReInit()
{
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
	t_task_error* err = get_task_error_front();
	if (err == NULL)
		return true;
	char data[128] = { 0x0 };
	int len = snprintf(data, sizeof(data), "{\"TaskID\":%d,\"ErrType\":%d,\"Timestamp\":%d,\"detail\":\"%s\"}", err->taskID, err->taskErrId, err->timestamp, err->errMsg);
	t_stress_protocol_head head;
	head.msgLen = sizeof(t_stress_protocol_head) + len;
	head.cmdNo = 13;
	char buf[256] = { 0x0 };
	::memcpy(buf, (char*)& head, sizeof(t_stress_protocol_head));
	::memcpy(buf + sizeof(t_stress_protocol_head), data, len);
	IOCPPostSendEx(this->StressHsocket, buf, len+8);
	//LOG("Report error %d %d", err->taskID, err->taskErrId);
	delete_task_error(err);
	return true;
}

bool StressProtocol::AddBuff(HSOCKET sock, char* data, int len)
{
	t_socket_buff* recvbuf = &(this->recvBuff);
	if (recvbuf->data == NULL)
	{
		recvbuf->data = (char*)GlobalAlloc(GPTR, INIT_BUFF_SIZE);
		if (recvbuf->data == NULL)
			return false;
		recvbuf->size = INIT_BUFF_SIZE;
	}
	int new_offset = len + recvbuf->offset;
	if (new_offset <= recvbuf->size)
	{
		::memcpy(recvbuf->data + recvbuf->offset, data, len);
		recvbuf->offset = new_offset;
	}
	else {
		char* new_data = (char*)GlobalReAlloc(recvbuf->data, new_offset, GMEM_ZEROINIT| GMEM_MOVEABLE);
		if (new_data == NULL)
			return false;
		recvbuf->data = new_data;
		::memcpy(recvbuf->data + recvbuf->offset, data, len);
		recvbuf->size = new_offset;
		recvbuf->offset = new_offset;
	}
	return true;
}

bool StressProtocol::DestroyBuff(HSOCKET sock, int len)
{
	t_socket_buff* recvbuf = &(this->recvBuff);
	if (len >= recvbuf->offset)
		recvbuf->offset = 0;
	else
	{
		memmove(recvbuf->data, recvbuf->data + len, (size_t)recvbuf->offset - len);
		recvbuf->offset -= len;
	}
	return TRUE;
}

int StressProtocol::CheckReq(HSOCKET sock, char* data, int len)
{
	this->AddBuff(sock, data, len);
	int clen = 0;
	while (true)
	{
		clen = this->CheckRequest(sock, this->recvBuff.data, this->recvBuff.offset);
		if (clen > 0)
			this->DestroyBuff(sock, clen);
		else if (clen == 0)
			return RECV;
		else
			return CLOSE;
	}
}

int StressProtocol::CheckRequest(HSOCKET sock, char* data, int len)
{
	//LOG("%s\n", data);
	if (len < sizeof(t_stress_protocol_head))
		return 0;
	t_stress_protocol_head head;
	::memcpy(&head, data, sizeof(t_stress_protocol_head));
	if (len < head.msgLen)
		return 0;
	char* body = (char*)GlobalAlloc(GPTR, head.msgLen - sizeof(t_stress_protocol_head) + 1);
	if (body == NULL)
		return -1;
	::memcpy(body, data + sizeof(t_stress_protocol_head), head.msgLen - sizeof(t_stress_protocol_head));
	//printf("serress client recv [%d %d:%s]\n", head.msgLen, head.cmdNo, body);
	LOG(clogId, LOG_TRACE, "stress client recv [%d %d:%s]\r\n", head.msgLen, head.cmdNo, body);

	cJSON * root = cJSON_Parse(body);
	if (root == NULL)
	{
		LOG(clogId, LOG_ERROR, "json encoding error\r\n");
		return -1;
	}
	do_client_func_by_cmd(sock, head.cmdNo, root);
	GlobalFree(body);
	cJSON_Delete(root);
	return head.msgLen;
}
