#include "pch.h"
#include "ReplayProtocol.h"


int Init(HTASKCFG task)
{	/*任务初始化，task为任务结构指针*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]\r\n", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

int UnInit(HTASKCFG task)
{	/*反任务初始化*/
	TaskLog(task, LOG_DEBUG, "%s:%d project[%d] env[%d]\r\n", __func__, __LINE__, task->projectID, task->envID);
	return 0;
}

int ThreadInit(HTASKCFG task)
{	/*工作线程初始化，用于初始化线程变量，工作线程开始执行时被调用一次*/
	TaskLog(task, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	return 0;
}

int ThreadUnInit(HTASKCFG task)
{	/*反工作线程初始化，用于释放线程资源*/
	TaskLog(task, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	return 0;
}

ReplayProtocol* CreateUser(void)
{	/*此函数创建一个继承自UserProtocol的对象实例，并返回其指针*/
	ReplayProtocol* hdl = new ReplayProtocol;
	return hdl;
}

void DestoryUser(ReplayProtocol* hdl)
{	/*此函数用于销毁CreateUser函数创建的对象实例*/
	if (hdl != NULL)
		delete hdl;
}


//类函数定义
ReplayProtocol::ReplayProtocol()
{	/*构造函数*/
}


ReplayProtocol::~ReplayProtocol()
{	/*析构函数*/
}

void ReplayProtocol::ProtoInit()
{	/*用户初始化，用户实例被创建后会被调用一次*/
}

bool ReplayProtocol::ConnectionMade(HSOCKET hsock, char* ip, int port)
{	/*当用户连接目标ip端口成功后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port）*/
	TaskLog(this->Task, LOG_DEBUG, "%s:%d [%s:%d] socket = %lld\r\n", __func__, __LINE__, ip, port, hsock->sock);
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
{	/*当用户连接目标ip端口失败后，调用此函数，并传递对应网络地址（ip）和端口（port）*/
	TaskLog(this->Task, LOG_FAULT,"%s:%d [%s:%d]\r\n", __func__, __LINE__, ip, port);
	this->MsgPointer = { 0x0 };
	this->pause = false;
	TaskUserDead(this, "connection failed");
	return true;
}

bool ReplayProtocol::ConnectionClosed(HSOCKET hsock, char* ip, int port)   //类销毁后，可能导致野指针
{	/*当用户连接关闭后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port）*/
	TaskLog(this->Task, LOG_FAULT, "%s:%d [%s:%d] socket = %lld\r\n", __func__, __LINE__, ip, port, hsock->sock);
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
{	/*无标准定义*/
	return true;
}

int ReplayProtocol::Recv(HSOCKET hsock, char* ip, int port, char* data, int len)
{	/*当用户连接收到消息后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port），以及数据指针（data）和消息长度（len）*/
	TaskLog(this->Task, LOG_DEBUG, "%s:%d [%s:%d][%.*s]\r\n", __func__, __LINE__, ip, port, len, data);
	return this->CheckReq(hsock->sock, data, len);
}

int ReplayProtocol::Loop()
{	/*任务运行过程中循环调用次函数，执行用户主动行为*/
	if (this->pause == true)
		return 0;

	this->message = TaskGetNextMessage(this);  /*获取下一步用户需要处理的事件消息*/
	if (this->message == NULL)
		return 0;
	HSOCKET* hsock;
	switch (this->message->type)
	{
	case TYPE_CONNECT: /*连接事件*/
		TaskLog(this->Task, LOG_DEBUG, "user connect[%s:%d]\r\n", this->message->ip.c_str(), this->message->port);
		this->pause = true;
		TaskUserSocketConnet(this->message->ip.c_str(), this->message->port, this->self);
		break;
	case TYPE_CLOSE:	/*关闭连接事件*/
		TaskLog(this->Task, LOG_DEBUG, "user conclose[%s:%d]\r\n", this->message->ip.c_str(), this->message->port);
		hsock = this->GetScokFromConnection(this->message->ip.c_str(), this->message->port);
		if (hsock != NULL)
		{
			TaskUserSocketClose(*hsock);
			*hsock = NULL;
		}
		break;
	case TYPE_SEND:	/*向连接发送消息事件*/
		TaskLog(this->Task, LOG_DEBUG, "user send[%s:%d [%s]]\r\n", this->message->ip.c_str(), this->message->port, this->message->content.c_str());
		hsock = this->GetScokFromConnection(this->message->ip.c_str(), this->message->port);
		if (hsock != NULL)
		{
			TaskUserSocketSend(*hsock, (char*)(this->message->content.c_str()), (int)this->message->content.size());
		}
		break;
	case TYPE_REINIT:	/*用户重置事件，关闭所有连接，初始化用户资源*/
		TaskLog(this->Task, LOG_NORMAL, "user loop[%d]\r\n", this->message->isloop);
		this->ReInit();
		break;
	default:
		break;
	}
	return 0;
}

int ReplayProtocol::ReInit()
{	/*用户重置到初始状态*/
	TaskLog(this->Task, LOG_NORMAL, "%s:%d\r\n", __func__, __LINE__);
	this->CloseAllConnection();
	return 0;
}

int ReplayProtocol::Destroy()
{	/*任务终止时，调用次函数，关闭所有连接，并且HSOCKET 句柄变量置为NULL*/
	TaskLog(this->Task, LOG_NORMAL, "%s:%d\r\n", __func__, __LINE__);
	this->CloseAllConnection();
	return  0;
}

//自定义类成员函数
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