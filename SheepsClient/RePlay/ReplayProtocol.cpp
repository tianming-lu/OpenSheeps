#include "pch.h"
#include "ReplayProtocol.h"


int Init(void* parm)
{	/*任务初始化，parm为当前任务配置指针，需要强制转换HTASKCFG类型*/
	TaskLog((HTASKCFG)parm, LOG_DEBUG, "dll init\r\n");
	return 0;
}

int UnInit(void* parm)
{	/*任务初始化,动态库被释放时调用一次，parm为当前任务配置指针，需要强制转换HTASKCFG类型*/
	TaskLog((HTASKCFG)parm, LOG_DEBUG, "dll uninit\r\n");
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

void ReplayProtocol::ProtoInit(void* parm, int index)
{	/*用户初始化，用户实例被创建后会被调用一次*/
	this->task = (HTASKCFG)parm;
}

bool ReplayProtocol::ConnectionMade(HSOCKET hsock, char* ip, int port)
{	/*当用户连接目标ip端口成功后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port）*/
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
{	/*当用户连接目标ip端口失败后，调用此函数，并传递对应网络地址（ip）和端口（port）*/
	TaskLog(this->task, LOG_FAULT,"connecetion fail：[%s:%d]\n", ip, port);
	this->MsgPointer = { 0x0 };
	this->pause = false;
	TaskUserDead(this, "connection failed");
	return true;
}

bool ReplayProtocol::ConnectionClosed(HSOCKET hsock, char* ip, int port)   //类销毁后，可能导致野指针
{	/*当用户连接关闭后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port）*/
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
{	/*无标准定义*/
	return true;
}

int ReplayProtocol::Recv(HSOCKET hsock, char* ip, int port, char* data, int len)
{	/*当用户连接收到消息后，调用此函数，hsock为连接句柄，并传递对应网络地址（ip）和端口（port），以及数据指针（data）和消息长度（len）*/
	TaskLog(this->task, LOG_DEBUG, "user recv [%s:%d][%.*s]\r\n", ip, port, len, data);
	return this->CheckReq(hsock->sock, data, len);
}

int ReplayProtocol::Loop()
{	/*任务运行过程中循环调用次函数，执行用户主动行为*/
	if (this->pause == true)
		return 0;
	if (this->message == NULL)
		this->message = TaskGetNextMessage(this);  /*用任务句柄获取用户行为，并传递用户实例指针，以及用户t_message_pointer结构指针*/
	if (this->message == NULL)
		return 0;
	HSOCKET* hsock;
	switch (this->message->type)
	{
	case TYPE_CONNECT: /*连接事件*/
		TaskLog(this->task, LOG_DEBUG, "user connect[%s:%d]\r\n", this->message->ip.c_str(), this->message->port);
		this->pause = true;
		TaskUserSocketConnet(this->message->ip.c_str(), this->message->port, this->self);
		this->message = NULL;
		break;
	case TYPE_CLOSE:	/*关闭连接事件*/
		TaskLog(this->task, LOG_DEBUG, "user conclose[%s:%d]\r\n", this->message->ip.c_str(), this->message->port);
		hsock = this->GetScokFromConnection(this->message->ip.c_str(), this->message->port);
		if (hsock != NULL)
		{
			TaskUserSocketClose(*hsock);
			*hsock = NULL;
		}
		this->message = NULL;
		break;
	case TYPE_SEND:	/*向连接发送消息事件*/
		TaskLog(this->task, LOG_DEBUG, "user send[%s:%d [%s]]\r\n", this->message->ip.c_str(), this->message->port, this->message->content.c_str());
		hsock = this->GetScokFromConnection(this->message->ip.c_str(), this->message->port);
		if (hsock != NULL)
		{
			TaskUserSocketSend(*hsock, (char*)(this->message->content.c_str()), (int)this->message->content.size());
		}
		this->message = NULL;
		break;
	case TYPE_REINIT:	/*用户重置事件，关闭所有连接，初始化用户资源*/
		TaskLog(this->task, LOG_NORMAL, "user loop[%d]\r\n", this->message->isloop);
		this->ReInit();
		break;
	default:
		break;
	}
	return 0;
}

int ReplayProtocol::ReInit()
{	/*用户重置到初始状态*/
	TaskLog(this->task, LOG_NORMAL, "user reinit\r\n");
	this->CloseAllConnection();
	this->message = NULL;
	return 0;
}

int ReplayProtocol::Destroy()
{	/*任务终止时，调用次函数，关闭所有连接，并且HSOCKET 句柄变量置为NULL*/
	TaskLog(this->task, LOG_NORMAL, "user destroy\r\n");
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