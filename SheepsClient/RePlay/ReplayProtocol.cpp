#include "pch.h"
#include "ReplayProtocol.h"


int Init(HTASKCFG task)
{	/*�����ʼ����taskΪ����ṹָ��*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]\r\n", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

int UnInit(HTASKCFG task)
{	/*�������ʼ��*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]\r\n", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

int ThreadInit(HTASKCFG task)
{	/*�����̳߳�ʼ�������ڳ�ʼ���̱߳����������߳̿�ʼִ��ʱ������һ��*/
	TaskLog(task, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	return 0;
}

int ThreadUnInit(HTASKCFG task)
{	/*�������̳߳�ʼ���������ͷ��߳���Դ*/
	TaskLog(task, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	return 0;
}

ReplayProtocol* CreateUser(void)
{	/*�˺�������һ���̳���UserProtocol�Ķ���ʵ������������ָ��*/
	ReplayProtocol* hdl = new ReplayProtocol;
	return hdl;
}

void DestoryUser(ReplayProtocol* hdl)
{	/*�˺�����������CreateUser���������Ķ���ʵ��*/
	if (hdl != NULL)
		delete hdl;
}


//�ຯ������
ReplayProtocol::ReplayProtocol()
{	/*���캯��*/
}


ReplayProtocol::~ReplayProtocol()
{	/*��������*/
}

void ReplayProtocol::ProtoInit()
{	/*�û���ʼ�����û�ʵ����������ᱻ����һ��*/
}

bool ReplayProtocol::ConnectionMade(HSOCKET hsock, char* ip, int port)
{	/*���û�����Ŀ��ip�˿ڳɹ��󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d] socket = %lld\r\n", __func__, __LINE__, ip, port, hsock->sock);
	map<int, t_connection_info>::iterator it = this->Connection.find(port);
	t_connection_info info = { 0x0 };
	info.hsock = hsock;
	if (it == this->Connection.end())
	{
		this->Connection.insert(pair<int, t_connection_info>(port, info));
	}
	else
	{
		if (it->second.hsock != NULL)
			TaskUserSocketClose(it->second.hsock);
		it->second.hsock = hsock;
		it->second.sendbuf.clear();
		it->second.recvbuf.clear();
	}
	this->PlayPause = false;
	return true;
}

bool ReplayProtocol::ConnectionFailed(HSOCKET hsock, char* ip, int port)
{	/*���û�����Ŀ��ip�˿�ʧ�ܺ󣬵��ô˺����������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_FAULT,"%s:%d [%s:%d]\r\n", __func__, __LINE__, ip, port);
	this->PlayPause = false;
	TaskUserDead(this, "connection failed");
	return true;
}

bool ReplayProtocol::ConnectionClosed(HSOCKET hsock, char* ip, int port)   //�����ٺ󣬿��ܵ���Ұָ��
{	/*���û����ӹرպ󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_FAULT, "%s:%d [%s:%d] socket = %lld\r\n", __func__, __LINE__, ip, port, hsock->sock);
	map<int, t_connection_info>::iterator it = this->Connection.find(port);
	if (it != this->Connection.end())
	{
		if (hsock == it->second.hsock)
		{
			it->second.hsock = NULL;
			it->second.sendbuf.clear();
			it->second.recvbuf.clear();
		}
	}
	return true;
}

int ReplayProtocol::Send(HSOCKET hsock, char* ip, int port, char* data, int len)
{	/*�ޱ�׼����*/
	return true;
}

int ReplayProtocol::Recv(HSOCKET hsock, char* ip, int port, char* data, int len)
{	/*���û������յ���Ϣ�󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port�����Լ�����ָ�루data������Ϣ���ȣ�len��*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d][%.*s]\r\n", __func__, __LINE__, ip, port, len, data);
	return this->CheckReq(hsock->sock, data, len);
}

int ReplayProtocol::TimeOut()
{
	TaskUserLog(this, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	return 0;
}

int ReplayProtocol::Event(uint8_t event_type, string ip, int port, string content)
{
	TaskUserLog(this, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	HSOCKET* hsock;
	switch (event_type)
	{
	case TYPE_CONNECT: /*�����¼�*/
		TaskUserLog(this, LOG_DEBUG, "user connect[%s:%d]\r\n", ip.c_str(), port);
		this->PlayPause = true;
		TaskUserSocketConnet(ip.c_str(), port, this, IOCP_TCP);
		break;
	case TYPE_CLOSE:	/*�ر������¼�*/
		TaskUserLog(this, LOG_DEBUG, "user conclose[%s:%d]\r\n", ip.c_str(), port);
		hsock = this->GetScokFromConnection(ip.c_str(), port);
		if (hsock != NULL)
		{
			TaskUserSocketClose(*hsock);
			*hsock = NULL;
		}
		break;
	case TYPE_SEND:	/*�����ӷ�����Ϣ�¼�*/
		TaskUserLog(this, LOG_DEBUG, "user send[%s:%d [%s]]\r\n", ip.c_str(), port, content.c_str());
		hsock = this->GetScokFromConnection(ip.c_str(), port);
		if (hsock != NULL)
		{
			TaskUserSocketSend(*hsock, (char*)(content.c_str()), (int)content.size());
		}
		break;
	default:
		break;
	}
	return 0;
}

int ReplayProtocol::ReInit()
{	/*�û����õ���ʼ״̬*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d\r\n", __func__, __LINE__);
	this->CloseAllConnection();
	return 0;
}

int ReplayProtocol::Destroy()
{	/*������ֹʱ�����ôκ������ر��������ӣ�����HSOCKET ���������ΪNULL*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d\r\n", __func__, __LINE__);
	this->CloseAllConnection();
	return  0;
}

//�Զ������Ա����
HSOCKET* ReplayProtocol::GetScokFromConnection(const char* ip, int port)
{
	map<int, t_connection_info>::iterator it = this->Connection.find(port);
	if (it != this->Connection.end())
	{
		return &(it->second.hsock);
	}
	return NULL;
}

bool ReplayProtocol::CloseAllConnection()
{
	map<int, t_connection_info>::iterator iter;
	for (iter = this->Connection.begin(); iter != this->Connection.end(); iter++)
	{
		if (iter->second.hsock != NULL)
		{
			TaskUserSocketClose(iter->second.hsock);
			iter->second.hsock = NULL;
		}
	}
	return true;
}

int ReplayProtocol::CheckReq(SOCKET sock, char* data, int len)
{
	int clen = len;// this->CheckRequest(sock, this->recvBuff.data, this->recvBuff.offset);
	if (clen < 0)
		return CLOSE;
	if (clen == 0)
		return RECV;
	return RECV;
}

int ReplayProtocol::CheckRequest(SOCKET sock, char* data, int len)
{
	return 0;
}