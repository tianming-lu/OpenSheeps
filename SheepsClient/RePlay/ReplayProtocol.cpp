#include "pch.h"
#include "ReplayProtocol.h"


int Init(void* parm)
{	/*�����ʼ����parmΪ��ǰ��������ָ�룬��Ҫǿ��ת��HTASKCFG����*/
	TaskLog((HTASKCFG)parm, LOG_DEBUG, "dll init\r\n");
	return 0;
}

int UnInit(void* parm)
{	/*�����ʼ��,��̬�ⱻ�ͷ�ʱ����һ�Σ�parmΪ��ǰ��������ָ�룬��Ҫǿ��ת��HTASKCFG����*/
	TaskLog((HTASKCFG)parm, LOG_DEBUG, "dll uninit\r\n");
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

void ReplayProtocol::ProtoInit(void* parm, int index)
{	/*�û���ʼ�����û�ʵ����������ᱻ����һ��*/
	this->task = (HTASKCFG)parm;
}

bool ReplayProtocol::ConnectionMade(HSOCKET hsock, char* ip, int port)
{	/*���û�����Ŀ��ip�˿ڳɹ��󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskLog(this->task, LOG_DEBUG, "connecetion made: [%s:%d] socket = %lld\n", ip, port, hsock->sock);
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
	this->pause = false;
	return true;
}

bool ReplayProtocol::ConnectionFailed(HSOCKET hsock, char* ip, int port)
{	/*���û�����Ŀ��ip�˿�ʧ�ܺ󣬵��ô˺����������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	TaskLog(this->task, LOG_FAULT,"connecetion fail��[%s:%d]\n", ip, port);
	this->MsgPointer = { 0x0 };
	this->pause = false;
	TaskUserDead(this, "connection failed");
	return true;
}

bool ReplayProtocol::ConnectionClosed(HSOCKET hsock, char* ip, int port)   //�����ٺ󣬿��ܵ���Ұָ��
{	/*���û����ӹرպ󣬵��ô˺�����hsockΪ���Ӿ���������ݶ�Ӧ�����ַ��ip���Ͷ˿ڣ�port��*/
	//LOG("connecetion closed: [%s:%d] socket = %lld\n", ip, port, sock);
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
	TaskLog(this->task, LOG_DEBUG, "user recv [%s:%d][%.*s]\r\n", ip, port, len, data);
	return this->CheckReq(hsock->sock, data, len);
}

int ReplayProtocol::Loop()
{	/*�������й�����ѭ�����ôκ�����ִ���û�������Ϊ*/
	if (this->pause == true)
		return 0;
	if (this->message == NULL)
		this->message = TaskGetNextMessage(this);  /*����������ȡ�û���Ϊ���������û�ʵ��ָ�룬�Լ��û�t_message_pointer�ṹָ��*/
	if (this->message == NULL)
		return 0;
	HSOCKET* hsock;
	switch (this->message->type)
	{
	case TYPE_CONNECT: /*�����¼�*/
		TaskLog(this->task, LOG_DEBUG, "user connect[%s:%d]\r\n", this->message->ip.c_str(), this->message->port);
		this->pause = true;
		TaskUserSocketConnet(this->message->ip.c_str(), this->message->port, this->self);
		this->message = NULL;
		break;
	case TYPE_CLOSE:	/*�ر������¼�*/
		TaskLog(this->task, LOG_DEBUG, "user conclose[%s:%d]\r\n", this->message->ip.c_str(), this->message->port);
		hsock = this->GetScokFromConnection(this->message->ip.c_str(), this->message->port);
		if (hsock != NULL)
		{
			TaskUserSocketClose(*hsock);
			*hsock = NULL;
		}
		this->message = NULL;
		break;
	case TYPE_SEND:	/*�����ӷ�����Ϣ�¼�*/
		TaskLog(this->task, LOG_DEBUG, "user send[%s:%d [%s]]\r\n", this->message->ip.c_str(), this->message->port, this->message->content.c_str());
		hsock = this->GetScokFromConnection(this->message->ip.c_str(), this->message->port);
		if (hsock != NULL)
		{
			TaskUserSocketSend(*hsock, (char*)(this->message->content.c_str()), (int)this->message->content.size());
		}
		this->message = NULL;
		break;
	case TYPE_REINIT:	/*�û������¼����ر��������ӣ���ʼ���û���Դ*/
		TaskLog(this->task, LOG_NORMAL, "user loop[%d]\r\n", this->message->isloop);
		this->ReInit();
		break;
	default:
		break;
	}
	return 0;
}

int ReplayProtocol::ReInit()
{	/*�û����õ���ʼ״̬*/
	TaskLog(this->task, LOG_NORMAL, "user reinit\r\n");
	this->CloseAllConnection();
	this->message = NULL;
	return 0;
}

int ReplayProtocol::Destroy()
{	/*������ֹʱ�����ôκ������ر��������ӣ�����HSOCKET ���������ΪNULL*/
	TaskLog(this->task, LOG_NORMAL, "user destroy\r\n");
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