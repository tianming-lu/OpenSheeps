/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
*
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#include "ReplayProtocol.h"


int TaskStart(HTASKCFG task)
{	/*任务开始，task为任务结构指针*/
	TaskLog(task, LOG_DEBUG, "%s:%d taskid[%d] project[%d] machineid[%d]", __func__, __LINE__, task->taskID, task->projectID, task->machineID);
	return 0;
}

int TaskStop(HTASKCFG task)
{	/*任务结束*/
	TaskLog(task, LOG_DEBUG, "%s:%d taskid[%d] project[%d] machineid[%d]", __func__, __LINE__, task->taskID, task->projectID, task->machineID);
	return 0;
}

ReplayProtocol* CreateUser(void)
{	/*此函数创建一个继承自UserProtocol的对象实例，并返回其指针*/
	return new UserProtocol;
}

void DestoryUser(ReplayProtocol* hdl)
{	/*此函数用于销毁CreateUser函数创建的对象实例*/
	if (hdl != NULL)
		delete (UserProtocol*)hdl;
}


//类函数定义
UserProtocol::UserProtocol()
{	/*构造函数*/
}


UserProtocol::~UserProtocol()
{	/*析构函数*/
}

void UserProtocol::EventInit()
{	/*用户初始化，用户实例被创建后会被调用一次 */
	TaskUserLog(this, LOG_DEBUG, "%s:%d", __func__, __LINE__);
}

void UserProtocol::ConnectionMade(HSOCKET hsock)
{	/*当用户连接目标ip端口成功后，调用此函数，hsock为连接句柄 */
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d] socket = %lld", __func__, __LINE__, hsock->peer_ip, hsock->peer_port, hsock->fd);
	this->PlayMode = PLAY_NORMAL;
}

void UserProtocol::ConnectionFailed(HSOCKET hsock, int err)
{	/*当用户连接目标ip端口失败后，调用此函数, hsock为连接句柄, err为错误码 */
	TaskUserLog(this, LOG_FAULT,"%s:%d [%s:%d]", __func__, __LINE__, hsock->peer_ip, hsock->peer_port);
	this->PlayMode = PLAY_NORMAL;
	TaskUserDead(this, "connection failed");
	std::map<int, t_connection_info>::iterator it = this->Connection.find(hsock->peer_port);
	if (it != this->Connection.end())
	{
		if (hsock == it->second.hsock)
		{
			it->second.hsock = NULL;
		}
	}
}

void UserProtocol::ConnectionClosed(HSOCKET hsock, int err)   //类销毁后，可能导致野指针
{	/*当用户连接关闭后，调用此函数， hsock为连接句柄, err为错误码 */
	TaskUserLog(this, LOG_FAULT, "%s:%d [%s:%d] socket = %lld", __func__, __LINE__, hsock->peer_ip, hsock->peer_port, hsock->fd);
	std::map<int, t_connection_info>::iterator it = this->Connection.find(hsock->peer_port);
	if (it != this->Connection.end())
	{
		if (hsock == it->second.hsock)
		{
			it->second.hsock = NULL;
		}
	}
}

void UserProtocol::ConnectionRecved(HSOCKET hsock, const char* data, int len)
{	/*当用户连接收到消息后，调用此函数，hsock为连接句柄，以及数据指针（data）和消息长度（len）*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d [%s:%d][%.*s]", __func__, __LINE__, hsock->peer_ip, hsock->peer_port, len, data);
	TaskUserSocketSkipBuf(hsock, len);
	this->PlayMode = PLAY_NORMAL;
}

void UserProtocol::EventConnectOpen(const char* ip, int port, bool udp)
{	/*连接事件通知，用户需要连接指定的ip和端口，udp == true表示这是一个udp连接*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d  %s:%d", __func__, __LINE__, ip, port);
	t_connection_info info = { 0x0 };
	std::map<int, t_connection_info>::iterator it;
	
	this->PlayMode = PLAY_PAUSE;
	HSOCKET conn_hsock = TaskUserSocketConnet(this, ip, port, TCP_CONN);
	if (conn_hsock == NULL)
	{
		TaskUserDead(this, "%s:%d conect failed!", __func__, __LINE__);
		return;
	}
	it = this->Connection.find(port);
	info.hsock = conn_hsock;
	if (it == this->Connection.end())
	{
		this->Connection.insert(std::pair<int, t_connection_info>(port, info));
	}
	else
	{
		if (it->second.hsock != NULL)
		{
			TaskUserSocketClose(it->second.hsock);
			it->second.hsock = NULL;
		}
		it->second.hsock = conn_hsock;
	}
}
void UserProtocol::EventConnectClose(const char* ip, int port, bool udp) 
{	/*连接关闭事件通知，用户需要关闭指定的ip和端口的连接（如果连接存在），udp == true表示这是一个udp连接*/
	TaskUserLog(this, LOG_DEBUG, "user conclose[%s:%d]", ip, port);
	HSOCKET* hsock = this->GetScokFromConnection(ip, port);
	if (hsock != NULL)
	{
		TaskUserSocketClose(*hsock);
		*hsock = NULL;
	}
}

void UserProtocol::EventConnectSend(const char* ip, int port, const char* content, int clen, bool udp)
{	/*连接发送消息事件通知，用户需要向指定的ip和端口的连接发送消息（如果连接存在），udp == true表示这是一个udp连接*/
	TaskUserLog(this, LOG_DEBUG, "user send[%s:%d [%s]]", ip, port, content);
	HSOCKET* hsock = this->GetScokFromConnection(ip, port);
	if (hsock != NULL)
	{
		TaskUserSocketSend(*hsock, (char*)content, clen);
		this->PlayMode = PLAY_PAUSE;
	}
}

void UserProtocol::EventTimeOut()
{	/*定时器超时事件通知*/
	TaskUserLog(this, LOG_DEBUG, "%s:%d", __func__, __LINE__);
}

void UserProtocol::EventReset()
{	/*用户状态重置事件，重置用户数据*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d %s", __func__, __LINE__, this->LastError);
	this->CloseAllConnection();
}

void UserProtocol::EventDestroy()
{	/*用户销毁事件，清理用户数据，关闭所有连接*/
	TaskUserLog(this, LOG_NORMAL, "%s:%d %s", __func__, __LINE__, this->LastError);
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
	for (iter = this->Connection.begin(); iter != this->Connection.end(); ++iter)
	{
		if (iter->second.hsock != NULL)
		{
			TaskUserSocketClose(iter->second.hsock);
			iter->second.hsock = NULL;
		}
	}
	return true;
}