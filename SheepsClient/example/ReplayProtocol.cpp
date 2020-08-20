#include "ReplayProtocol.h"


int Init(HTASKCFG task)
{	/*�����ʼ����taskΪ����ṹָ��*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

int UnInit(HTASKCFG task)
{	/*�������ʼ��*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

ReplayProtocol* CreateUser(void)
{	/*�˺�������һ���̳���UserProtocol�Ķ���ʵ������������ָ��*/
	UserProtocol* hdl = new UserProtocol;
	return hdl;
}

void DestoryUser(ReplayProtocol* hdl)
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

void UserProtocol::Init()
{	/*�û���ʼ�����û�ʵ����������ᱻ����һ��*/
}

void UserProtocol::ConnectionMade(HSOCKET hsock, const char* ip, int port)
{	/*���û�����Ŀ��ip�˿ڳɹ��󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d] socket = %lld", __func__, __LINE__, ip, port, hsock->fd);
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
	this->PlayState = PLAY_NORMAL;
}

void UserProtocol::ConnectionFailed(HSOCKET hsock, const char* ip, int port)
{	/*���û�����Ŀ��ip�˿�ʧ�ܺ󣬵��ô˺����������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_FAULT,"%s:%d [%s:%d]", __func__, __LINE__, ip, port);
	this->PlayState = PLAY_NORMAL;
	TaskUserDead(this, "connection failed");
}

void UserProtocol::ConnectionClosed(HSOCKET hsock, const char* ip, int port)   //�����ٺ󣬿��ܵ���Ұָ��
{	/*���û����ӹرպ󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskUserLog(this, LOG_FAULT, "%s:%d [%s:%d] socket = %lld", __func__, __LINE__, ip, port, hsock->fd);
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
}

void UserProtocol::Recved(HSOCKET hsock, const char* ip, int port, const char* data, int len)
{	/*���û������յ���Ϣ�󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port�����Լ�����ָ�루data������Ϣ���ȣ�len��*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d][%.*s]", __func__, __LINE__, ip, port, len, data);
	this->PlayState = PLAY_NORMAL;
}

void UserProtocol::TimeOut()
{
	//TaskUserLog(this, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
}

void UserProtocol::Event(uint8_t event_type, const char* ip, int port, const char* content, int clen, bool udp)
{
	TaskUserLog(this, LOG_DEBUG, "%s:%d", __func__, __LINE__);
	/*HSOCKET* hsock;
	switch (event_type)
	{
	case TYPE_CONNECT: /*�����¼�
		TaskUserLog(this, LOG_DEBUG, "user connect[%s:%d]", ip, port);
		this->PlayState = PLAY_PAUSE;
		TaskUserSocketConnet(this, ip, port, IOCP_TCP);
		break;
	case TYPE_CLOSE:	/*�ر������¼�
		TaskUserLog(this, LOG_DEBUG, "user conclose[%s:%d]", ip, port);
		hsock = this->GetScokFromConnection(ip, port);
		if (hsock != NULL)
		{
			TaskUserSocketClose(*hsock);
			*hsock = NULL;
		}
		break;
	case TYPE_SEND:	/*�����ӷ�����Ϣ�¼�
		TaskUserLog(this, LOG_DEBUG, "user send[%s:%d [%s]]", ip, port, content);
		hsock = this->GetScokFromConnection(ip, port);
		if (hsock != NULL)
		{
			TaskUserSocketSend(*hsock, (char*)content, clen);
			this->PlayState = PLAY_PAUSE;
		}
		break;
	default:
		break;
	}*/
}

void UserProtocol::ReInit()
{	/*�û����õ���ʼ״̬*/
	//TaskUserLog(this, LOG_NORMAL, "%s:%d", __func__, __LINE__);
	this->CloseAllConnection();
}

void UserProtocol::Destroy()
{	/*������ֹʱ�����ôκ������ر��������ӣ�����HSOCKET ���������ΪNULL*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d", __func__, __LINE__);
	this->CloseAllConnection();
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