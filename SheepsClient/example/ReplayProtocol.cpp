#include "ReplayProtocol.h"


int Init(HTASKCFG task)
{	/*任务初始化，task为任务结构指针*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

int UnInit(HTASKCFG task)
{	/*反任务初始化*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

ReplayProtocol* CreateUser(void)
{	/*此函数创建一个继承自UserProtocol的对象实例，并返回其指针*/
	UserProtocol* hdl = new UserProtocol;
	return hdl;
}

void DestoryUser(ReplayProtocol* hdl)
{	/*此函数用于销毁CreateUser函数创建的对象实例*/
	if (hdl != NULL)
		delete hdl;
}


//类函数定义
UserProtocol::UserProtocol()
{	/*构造函数*/
}


UserProtocol::~UserProtocol()
{	/*析构函数*/
}

void UserProtocol::Init()
{	/*用户初始化，用户实例被创建后会被调用一次*/
}

void UserProtocol::ConnectionMade(HSOCKET hsock, const char* ip, int port)
{	/*当用户连接目标ip端口成功后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port）*/
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
{	/*当用户连接目标ip端口失败后，调用此函数，并传递对应网络地址（ip）和端口（port）*/
	TaskUserLog(this, LOG_FAULT,"%s:%d [%s:%d]", __func__, __LINE__, ip, port);
	this->PlayState = PLAY_NORMAL;
	TaskUserDead(this, "connection failed");
}

void UserProtocol::ConnectionClosed(HSOCKET hsock, const char* ip, int port)   //类销毁后，可能导致野指针
{	/*当用户连接关闭后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port）*/
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
{	/*当用户连接收到消息后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port），以及数据指针（data）和消息长度（len）*/
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
	case TYPE_CONNECT: /*连接事件
		TaskUserLog(this, LOG_DEBUG, "user connect[%s:%d]", ip, port);
		this->PlayState = PLAY_PAUSE;
		TaskUserSocketConnet(this, ip, port, IOCP_TCP);
		break;
	case TYPE_CLOSE:	/*关闭连接事件
		TaskUserLog(this, LOG_DEBUG, "user conclose[%s:%d]", ip, port);
		hsock = this->GetScokFromConnection(ip, port);
		if (hsock != NULL)
		{
			TaskUserSocketClose(*hsock);
			*hsock = NULL;
		}
		break;
	case TYPE_SEND:	/*向连接发送消息事件
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
{	/*用户重置到初始状态*/
	//TaskUserLog(this, LOG_NORMAL, "%s:%d", __func__, __LINE__);
	this->CloseAllConnection();
}

void UserProtocol::Destroy()
{	/*任务终止时，调用次函数，关闭所有连接，并且HSOCKET 句柄变量置为NULL*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d", __func__, __LINE__);
	this->CloseAllConnection();
}

//自定义类成员函数
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