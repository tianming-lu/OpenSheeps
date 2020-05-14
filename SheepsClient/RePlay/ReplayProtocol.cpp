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

UserProtocol* CreateUser(void)
{	/*�˺�������һ���̳���UserProtocol�Ķ���ʵ������������ָ��*/
	UserProtocol* hdl = new UserProtocol;
	return hdl;
}

void DestoryUser(UserProtocol* hdl)
{	/*�˺�����������CreateUser���������Ķ���ʵ��*/
	if (hdl != NULL)
		delete hdl;
}


//�ຯ������
UserProtocol::UserProtocol()
{	/*���캯��*/
}


UserProtocol::~UserProtocol()
{	/*��������*/
}

void UserProtocol::ProtoInit()
{	/*�û���ʼ�����û�ʵ����������ᱻ����һ��*/
}

bool UserProtocol::ConnectionMade(HSOCKET hsock, const char* ip, int port)
{	/*���û�����Ŀ��ip�˿ڳɹ��󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d] socket = %lld\r\n", __func__, __LINE__, ip, port, hsock->sock);
	std::map<int, t_connection_info>::iterator it = this->Connection.find(port);
	t_connection_info info = { 0x0 };
	info.hsock = hsock;
	if (it == this->Connection.end())
	{
		this->Connection.insert(std::pair<int, t_connection_info>(port, info));
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

bool UserProtocol::ConnectionFailed(HSOCKET hsock, const char* ip, int port)
{	/*���û�����Ŀ��ip�˿�ʧ�ܺ󣬵��ô˺����������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_FAULT,"%s:%d [%s:%d]\r\n", __func__, __LINE__, ip, port);
	this->PlayPause = false;
	TaskUserDead(this, "connection failed");
	return true;
}

bool UserProtocol::ConnectionClosed(HSOCKET hsock, const char* ip, int port)   //�����ٺ󣬿��ܵ���Ұָ��
{	/*���û����ӹرպ󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_FAULT, "%s:%d [%s:%d] socket = %lld\r\n", __func__, __LINE__, ip, port, hsock->sock);
	std::map<int, t_connection_info>::iterator it = this->Connection.find(port);
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

int UserProtocol::Send(HSOCKET hsock, char* ip, int port, char* data, int len)
{	/*�ޱ�׼����*/
	return true;
}

int UserProtocol::Recv(HSOCKET hsock, const char* ip, int port, const char* data, int len)
{	/*���û������յ���Ϣ�󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port�����Լ�����ָ�루data������Ϣ���ȣ�len��*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d][%.*s]\r\n", __func__, __LINE__, ip, port, len, data);
	return this->CheckReq(hsock->sock, data, len);
}

int UserProtocol::TimeOut()
{
	TaskUserLog(this, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	return 0;
}

int UserProtocol::Event(uint8_t event_type, const char* ip, int port, const char* content, int clen)
{
	TaskUserLog(this, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	HSOCKET* hsock;
	switch (event_type)
	{
	case TYPE_CONNECT: /*�����¼�*/
		TaskUserLog(this, LOG_DEBUG, "user connect[%s:%d]\r\n", ip, port);
		this->PlayPause = true;
		TaskUserSocketConnet(ip, port, this, IOCP_TCP);
		break;
	case TYPE_CLOSE:	/*�ر������¼�*/
		TaskUserLog(this, LOG_DEBUG, "user conclose[%s:%d]\r\n", ip, port);
		hsock = this->GetScokFromConnection(ip, port);
		if (hsock != NULL)
		{
			TaskUserSocketClose(*hsock);
			*hsock = NULL;
		}
		break;
	case TYPE_SEND:	/*�����ӷ�����Ϣ�¼�*/
		TaskUserLog(this, LOG_DEBUG, "user send[%s:%d [%s]]\r\n", ip, port, content);
		hsock = this->GetScokFromConnection(ip, port);
		if (hsock != NULL)
		{
			TaskUserSocketSend(*hsock, (char*)content, clen);
		}
		break;
	default:
		break;
	}
	return 0;
}

int UserProtocol::ReInit()
{	/*�û����õ���ʼ״̬*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d\r\n", __func__, __LINE__);
	this->CloseAllConnection();
	return 0;
}

int UserProtocol::Destroy()
{	/*������ֹʱ�����ôκ������ر��������ӣ�����HSOCKET ���������ΪNULL*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d\r\n", __func__, __LINE__);
	this->CloseAllConnection();
	return  0;
}

//�Զ������Ա����
HSOCKET* UserProtocol::GetScokFromConnection(const char* ip, int port)
{
	std::map<int, t_connection_info>::iterator it = this->Connection.find(port);
	if (it != this->Connection.end())
	{
		return &(it->second.hsock);
	}
	return NULL;
}

bool UserProtocol::CloseAllConnection()
{
	std::map<int, t_connection_info>::iterator iter;
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

int UserProtocol::CheckReq(SOCKET sock, const char* data, int len)
{
	int clen = len;// this->CheckRequest(sock, this->recvBuff.data, this->recvBuff.offset);
	if (clen < 0)
		return CLOSE;
	if (clen == 0)
		return RECV;
	return RECV;
}

int UserProtocol::CheckRequest(SOCKET sock, char* data, int len)
{
	return 0;
}