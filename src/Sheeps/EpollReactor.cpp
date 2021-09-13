/*
*	Copyright(c) 2020 lutianming email：641471957@qq.com
* 
*	Sheeps may be copied only under the terms of the GNU Affero General Public License v3.0
*/

#include "Reactor.h"
#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/timerfd.h>

#define DATA_BUFSIZE 5120

enum{
    ACCEPT = 0,
    READ,
	WRITE,
    CONNECT,
};

static HSOCKET new_hsockt()
{
	HSOCKET hsock = (HSOCKET)malloc(sizeof(EPOLL_SOCKET));
	if (hsock == NULL) return NULL;
	memset(hsock, 0, sizeof(EPOLL_SOCKET));
	hsock->recv_buf = (char*)malloc(DATA_BUFSIZE);
	if (hsock->recv_buf == NULL)
	{
		free(hsock);
		return NULL;
	}
	memset(hsock->recv_buf, 0, DATA_BUFSIZE);
	hsock->_recv_buf.offset = 0;
	hsock->_recv_buf.size = DATA_BUFSIZE;
	return hsock;
}

static void release_hsock(HSOCKET hsock)
{
	if (hsock)
	{
		if (hsock->recv_buf) free(hsock->recv_buf);
		if (hsock->_sendbuff) free(hsock->_sendbuff);
		free(hsock);
	}
}

static int get_listen_sock(const char* ip, int port)
{
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	//addr.sin_addr.s_addr = htonl(INADDR_ANY);
	inet_pton(AF_INET, ip, &addr.sin_addr);
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0) {
		return -1;
	}

	int optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	struct linger linger = {0, 0};
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (int *)&linger, sizeof(linger));

	if(bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) 
    {
		return -1;
	}

	if (listen(fd, 10)) 
    {
		return -1;
	}

	fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	return fd;
}

static void epoll_add_accept(HSOCKET hsock, int type) 
{
    hsock->_epoll_type = type;
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR | EPOLLET;
	ev.data.ptr = hsock;
	epoll_ctl(hsock->factory->reactor->epfd, EPOLL_CTL_ADD, hsock->fd, &ev);	
}

static void epoll_add_read(HSOCKET hsock, int type) 
{
    hsock->_epoll_type = type;
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR | EPOLLET | EPOLLONESHOT;
	ev.data.ptr = hsock;
	epoll_ctl(hsock->factory->reactor->epfd, EPOLL_CTL_ADD, hsock->fd, &ev);	
}

static void epoll_add_write(HSOCKET hsock, int type)
{
	hsock->_epoll_type = type;
	struct epoll_event ev;
	ev.events = EPOLLOUT | EPOLLERR | EPOLLET | EPOLLONESHOT;
	ev.data.ptr = hsock;
	epoll_ctl(hsock->factory->reactor->epfd, EPOLL_CTL_ADD, hsock->fd, &ev);
}

static void epoll_mod_read(HSOCKET hsock, int type) 
{
    hsock->_epoll_type = type;
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLERR | EPOLLET | EPOLLONESHOT;
	ev.data.ptr = hsock;
	epoll_ctl(hsock->factory->reactor->epfd, EPOLL_CTL_MOD, hsock->fd, &ev);	
}

static void epoll_mod_write(HSOCKET hsock, int type)
{
	hsock->_epoll_type = type;
	struct epoll_event ev;
	ev.events = EPOLLOUT | EPOLLERR | EPOLLET | EPOLLONESHOT;
	ev.data.ptr = hsock;
	epoll_ctl(hsock->factory->reactor->epfd, EPOLL_CTL_MOD, hsock->fd, &ev);
}

static void epoll_del(HSOCKET hsock) 
{
	struct epoll_event ev;
	epoll_ctl(hsock->factory->reactor->epfd, EPOLL_CTL_DEL, hsock->fd, &ev);	
}

static void set_linger_for_fd(int fd)
{
	return;
    struct linger linger;
	linger.l_onoff = 0;
	linger.l_linger = 0;
	setsockopt(fd, SOL_SOCKET, SO_LINGER, (const void *) &linger, sizeof(struct linger));
}

static void do_close(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	uint8_t left_count = 0;
	if (hsock->_is_close == 0)
	{

		proto->Lock();
		if (hsock->_is_close == 0)
		{
			left_count = __sync_sub_and_fetch (&proto->sockCount, 1);
			if (hsock->_epoll_type == READ)
    			proto->ConnectionClosed(hsock, errno);
			else
				proto->ConnectionFailed(hsock, errno);	
		}
		proto->UnLock();
	}
    
    epoll_del(hsock);
    close(hsock->fd);

	if (left_count == 0 && proto->protoType == SERVER_PROTOCOL) fc->DeleteProtocol(proto);
    release_hsock(hsock);
}

static void do_connect(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	if (hsock->_is_close == 0)
	{
		proto->Lock();
		if (hsock->_is_close == 0)
			proto->ConnectionMade(hsock);
		proto->UnLock();
	}

	if (hsock->_send_buf.offset > 0)
		epoll_mod_write(hsock, WRITE);
	else
		epoll_mod_read(hsock, READ);
}

static void check_connect_events(HSOCKET hsock, int events,  BaseFactory* fc, BaseProtocol* proto)
{
	if (events == EPOLLOUT)
		do_connect(hsock, fc, proto);
	else
		do_close(hsock, fc, proto);
}

static void do_write_udp(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	if (hsock->_is_close == 0)
	{
		//hsock->_sendlock->lock();
		char* data = hsock->_sendbuff;
		int len = hsock->_send_buf.offset;
		
		socklen_t socklen = sizeof(hsock->peer_addr);
		if (len > 0)
		{
			sendto(hsock->fd, data, len, 0, (struct sockaddr*)&hsock->peer_addr, socklen);
			hsock->_send_buf.offset = 0;
		}
		//hsock->_sendlock->unlock();
	
		epoll_mod_read(hsock, READ);
	}
	else
	{
		do_close(hsock, fc, proto);
	}
}

static void do_write_tcp(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	if (hsock->_is_close == 0)
	{
		while (__sync_fetch_and_or(&hsock->_send_buf.lock_flag, 1)) usleep(0);

		char* data = hsock->_sendbuff;
		int len = hsock->_send_buf.offset;
		int not_over = 0;
		if (len > 0)
		{
			int n = send(hsock->fd, data, len, MSG_DONTWAIT | MSG_NOSIGNAL);
			if(n > 0) 
			{
				hsock->_send_buf.offset -= n;
				memmove(data, data + n, hsock->_send_buf.offset);
				if (n < len)
					not_over = 1;
			}
			else if(errno == EINTR || errno == EAGAIN) 
				not_over = 1;
		}
		__sync_fetch_and_and(&hsock->_send_buf.lock_flag, 0);
	
		if (not_over)
			epoll_mod_write(hsock, WRITE);
		else
			epoll_mod_read(hsock, READ);
	}
	else
	{
		do_close(hsock, fc, proto);
	}
}

static void do_write(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	if (hsock->_conn_type == TCP_CONN)
		do_write_tcp(hsock, fc, proto);
	else
		do_write_udp(hsock, fc, proto);
}

static int do_read_udp(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	char* buf = NULL;
	size_t buflen = 0;
	int n = -1;
	int addr_len=sizeof(struct sockaddr_in);

	buf = hsock->recv_buf + hsock->_recv_buf.offset;
	buflen = hsock->_recv_buf.size - hsock->_recv_buf.offset;
	n = recvfrom(hsock->fd, buf, buflen, 0, (sockaddr*)&(hsock->peer_addr), (socklen_t*)&addr_len);
	if (n > 0)
	{
		hsock->_recv_buf.offset += n;
		if (hsock->peer_port == 0)
		{
			inet_ntop(AF_INET, &hsock->peer_addr.sin_addr, hsock->peer_ip, sizeof(hsock->peer_ip));
			hsock->peer_port = ntohs(hsock->peer_addr.sin_port);
		}
		return 0;
	}
	else
	{
		return -1;
	}
	return -1;
}

static int do_read_tcp(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	char* buf = NULL;
	size_t buflen = 0;
	ssize_t n = 0;
	while (1)
	{
		buf = hsock->recv_buf + hsock->_recv_buf.offset;
		buflen = hsock->_recv_buf.size - hsock->_recv_buf.offset;
		n = recv(hsock->fd, buf, buflen, MSG_DONTWAIT);
		if (n > 0)
		{
			hsock->_recv_buf.offset += n;
			if (size_t(n) == buflen)
			{
				size_t newsize = hsock->_recv_buf.size*2;
				char* newbuf = (char*)realloc(hsock->recv_buf , newsize);
				if (newbuf == NULL) return -1;
				hsock->recv_buf = newbuf;
				hsock->_recv_buf.size = newsize;
				continue;
			}
			break;
		}
		else if (n == 0)
		{
			return -1;
		}
		if (errno == EINTR)
		{
			continue;
		}
		else if (errno == EAGAIN)
		{
			break;
		}
		else
		{
			return -1;
		}
	}
	return 0;
}

static void do_timer_callback(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	uint64_t value;
	read(hsock->fd, &value, sizeof(uint64_t)); //必须读取，否则定时器异常
	if (hsock->_is_close == 0)
	{
		hsock->_callback(proto);
		epoll_mod_read(hsock, READ);
	}
	else
	{
		epoll_del(hsock);
		close(hsock->fd);
		release_hsock(hsock);
	}
	
}

static void do_read(HSOCKET hsock, BaseFactory* fc, BaseProtocol* proto)
{
	if (hsock->_conn_type == ITMER)
		return do_timer_callback(hsock, fc, proto);

	if (__sync_fetch_and_or(&hsock->_recv_buf.lock_flag, 1)) return;
	int ret = 0;
	if(hsock->_conn_type == UDP_CONN)
		ret = do_read_udp(hsock, fc, proto);
	else
		ret = do_read_tcp(hsock, fc, proto);
	
	if (ret == 0 && hsock->_is_close == 0)
	{
		proto->Lock();
		if (hsock->_is_close == 0)
			proto->ConnectionRecved(hsock, hsock->recv_buf, hsock->_recv_buf.offset);
		proto->UnLock();

	}
	else
	{
		return do_close(hsock, fc, proto);
	}
	

	if (hsock->_send_buf.offset > 0)
		epoll_mod_write(hsock, WRITE);
	else
		epoll_mod_read(hsock, READ);
	__sync_fetch_and_and(&hsock->_recv_buf.lock_flag, 0);
}

static void check_read_write_events(HSOCKET hsock, int events,  BaseFactory* fc, BaseProtocol* proto)
{
	if(events == EPOLLIN) 
		do_read(hsock, fc, proto);
	else if(events == EPOLLOUT)
		do_write(hsock, fc, proto);
	else
		do_close(hsock, fc, proto);
}

static void do_accpet(HSOCKET listenhsock, BaseFactory* fc)
{
    struct sockaddr_in addr;
	socklen_t len;
   	int fd = 0;

	while (1)
	{
	    fd = accept(fc->Listenfd, (struct sockaddr *)&addr, (len = sizeof(addr), &len));
		if (fd < 0)
			break;
        HSOCKET hsock = new_hsockt();
		if (hsock == NULL) 
		{
			close(fd);
			continue;
		}
		memcpy(&hsock->peer_addr, &addr, sizeof(addr));

        BaseProtocol* proto = fc->CreateProtocol();
		if (proto->factory == NULL)
			proto->SetFactory(fc, SERVER_PROTOCOL);

        hsock->fd = fd;
		hsock->factory = fc;
		hsock->peer_port = ntohs(addr.sin_port);
		inet_ntop(AF_INET, (void *)&addr, hsock->peer_ip, sizeof(hsock->peer_ip));
		hsock->_user = proto;
		
		set_linger_for_fd(fd);
        fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);
		__sync_add_and_fetch(&proto->sockCount, 1);

		proto->Lock();
        proto->ConnectionMade(hsock);
        proto->UnLock();

		if (hsock->_send_buf.offset > 0)
			epoll_add_write(hsock, WRITE);
		else
			epoll_add_read(hsock, READ);
	}
}

static void check_accpet_events(HSOCKET hsock, int events,  BaseFactory* fc)
{
	if (events == EPOLLIN)
		do_accpet(hsock, fc);
}

static void sub_work_thread(void* args)
{
    Reactor* reactor = (Reactor*)args;
    int i = 0,n = 0;
    struct epoll_event *pev = (struct epoll_event*)malloc(sizeof(struct epoll_event) * reactor->maxevent);
	if(pev == NULL) 
	{
		return ;
	}
    volatile HSOCKET hsock = NULL;
	while (1)
	{
		n = epoll_wait(reactor->epfd, pev, reactor->maxevent, -1);
		for(i = 0; i < n; i++) 
		{
            hsock = (HSOCKET)pev[i].data.ptr;
			//printf("%s:%d [%d:%d] hsock[%lld] fd[%d] event[%d] value[%d]\n", __func__, __LINE__, n, i, hsock, hsock->fd, hsock->_epoll_type, pev[i].events);
            switch (hsock->_epoll_type)
            {
            case ACCEPT:
                check_accpet_events(hsock, pev[i].events, hsock->factory);
                break;
            case READ:
				check_read_write_events(hsock, pev[i].events, hsock->factory, hsock->_user);
				break;
			case WRITE:
				check_read_write_events(hsock, pev[i].events, hsock->factory, hsock->_user);
				break;
            case CONNECT:
                check_connect_events(hsock, pev[i].events, hsock->factory, hsock->_user);
                break;
            default:
                break;
            }
		}
	}
}

static void main_work_thread(void* args)
{
	Reactor* reactor = (Reactor*)args;
    int i = 0;

    for (; i < reactor->CPU_COUNT*2; i++)
	{
		pthread_attr_t attr;
   		pthread_t tid;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 1024*1024*16);
		int rc;

		if((rc = pthread_create(&tid, &attr, (void*(*)(void*))sub_work_thread, reactor)) != 0)
		{
			return;
		}
	}
	while (reactor->Run)
	{
		std::map<uint16_t, BaseFactory*>::iterator iter;
		for (iter = reactor->FactoryAll.begin(); iter != reactor->FactoryAll.end(); ++iter)
		{
			iter->second->FactoryLoop();
		}
		usleep(1000);
	}
}

int ReactorStart(Reactor* reactor)
{
    reactor->epfd = epoll_create(reactor->maxevent);
	if(reactor->epfd < 0)
		return -1;

	reactor->CPU_COUNT = get_nprocs_conf();
	pthread_attr_t attr;
   	pthread_t tid;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 1024*1024*16);
	int rc;

	if((rc = pthread_create(&tid, &attr, (void*(*)(void*))main_work_thread, reactor)) != 0)
	{
		return -1;
	}
	return 0;
}

int	FactoryRun(BaseFactory* fc)
{
	fc->FactoryInit();
	
	if (fc->ServerPort > 0)
	{
		fc->Listenfd = get_listen_sock(fc->ServerAddr, fc->ServerPort);
		HSOCKET hsock = new_hsockt();
		if (hsock == NULL) return -1;
		hsock->fd = fc->Listenfd;
		hsock->factory = fc;

		epoll_add_accept(hsock, ACCEPT);
	}
	fc->reactor->FactoryAll.insert(std::pair<uint16_t, BaseFactory*>(fc->ServerPort, fc));
	return 0;
}

int FactoryStop(BaseFactory* fc)
{
	std::map<uint16_t, BaseFactory*>::iterator iter;
	iter = fc->reactor->FactoryAll.find(fc->ServerPort);
	if (iter != fc->reactor->FactoryAll.end())
	{
		fc->reactor->FactoryAll.erase(iter);
	}
	fc->FactoryClose();
	return 0;
}

static bool EpollConnectExUDP(BaseProtocol* proto, HSOCKET hsock)
{
	hsock->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if(hsock->fd < 0) {
		return false;
	}
	sockaddr_in local_addr;
	memset(&local_addr, 0, sizeof(sockaddr_in));
	local_addr.sin_family = AF_INET;
	
	if(bind(hsock->fd, (struct sockaddr *)&local_addr, sizeof(sockaddr_in)) < 0) 
    {
		close(hsock->fd);
		release_hsock(hsock);
		return false;
	}
	epoll_add_read(hsock, READ);
	return true;
}

static bool EpollConnectExTCP(BaseProtocol* proto, HSOCKET hsock)
{
	int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(fd < 0) {
		return false;
	}
	hsock->fd = fd;
	set_linger_for_fd(fd);
	fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);
	connect(fd, (struct sockaddr*)&hsock->peer_addr, sizeof(hsock->peer_addr));
	epoll_add_write(hsock, CONNECT);
	return  true;
}

HSOCKET HsocketConnect(BaseProtocol* proto, const char* ip, int port, CONN_TYPE type)
{
	if (proto == NULL || (proto->sockCount == 0 && proto->protoType == SERVER_PROTOCOL)) 
		return NULL;
	HSOCKET hsock = new_hsockt();
	if (hsock == NULL) 
		return NULL;
	hsock->peer_addr.sin_family = AF_INET;
	hsock->peer_addr.sin_port = htons(port);
	inet_pton(AF_INET, ip, &hsock->peer_addr.sin_addr);

	hsock->factory = proto->factory;
	hsock->_conn_type = type;
	memcpy(hsock->peer_ip, ip, strlen(ip));
	hsock->peer_port = port;
	hsock->_user = proto;

	bool ret = false;
	if (type == UDP_CONN)
		ret =  EpollConnectExUDP(proto, hsock);
	else
		ret = EpollConnectExTCP(proto, hsock);
	if (ret == false) 
	{
		release_hsock(hsock);
		return NULL;
	}
	__sync_add_and_fetch(&proto->sockCount, 1);
	return hsock;
}

HSOCKET TimerCreate(BaseProtocol* proto, int duetime, int looptime, timer_callback callback)
{
	HSOCKET hsock = new_hsockt();
	if (hsock == NULL) 
		return NULL;
	int tfd = timerfd_create(CLOCK_MONOTONIC, 0);   //创建定时器
    if(tfd == -1) {
		release_hsock(hsock);
        return NULL;
    }
	hsock->fd = tfd;
	hsock->_conn_type = ITMER;
	hsock->_callback = callback;
	hsock->_user = proto;
	hsock->factory = proto->factory;

    struct itimerspec time_intv; //用来存储时间
    time_intv.it_value.tv_sec = duetime/1000; //设定2s超时
    time_intv.it_value.tv_nsec = (duetime%1000)*1000000;
    time_intv.it_interval.tv_sec = looptime/1000;   //每隔2s超时
    time_intv.it_interval.tv_nsec = (looptime%1000)*1000000;

    timerfd_settime(tfd, 0, &time_intv, NULL);  //启动定时器
	epoll_add_read(hsock, READ);
    return hsock;
}

void TimerDelete(HSOCKET hsock)
{
	hsock->_is_close = 1;
	//epoll_del(hsock);
	//close(hsock->fd);
	//release_hsock(hsock);
}

static void HsocketSendUdp(HSOCKET hsock, const char* data, int len)
{
	socklen_t socklen = sizeof(hsock->peer_addr);
	sendto(hsock->fd, data, len, 0, (struct sockaddr*)&hsock->peer_addr, socklen);
}

static void HsocketSendTcp(HSOCKET hsock, const char* data, int len)
{
	while (__sync_fetch_and_or(&hsock->_send_buf.lock_flag, 1)) usleep(0);
	int slen = 0;
	int n = 0;
	while (len > slen)
	{
		n = send(hsock->fd, data + slen, len - slen, MSG_DONTWAIT | MSG_NOSIGNAL);
		if(n > 0) 
		{
			slen += n;
		}
		else if(errno == EINTR || errno == EAGAIN) 
			continue;
	}
	__sync_fetch_and_and(&hsock->_send_buf.lock_flag, 0);

	/*if (hsock->_sendbuff == NULL)
	{
		hsock->_sendbuff = (char*)malloc(len);
		if (hsock->_sendbuff == NULL) return;
		hsock->_send_buf.size = len;
		hsock->_send_buf.offset = 0;
	}

	while (__sync_fetch_and_or(&hsock->_send_buf.lock_flag, 1)) usleep(0);
	if (hsock->_send_buf.size < hsock->_send_buf.offset + len)
	{
		char* newbuf = (char*)realloc(hsock->_sendbuff, hsock->_send_buf.offset + len);
		if (newbuf == NULL) {__sync_fetch_and_and(&hsock->_send_buf.lock_flag, 0);;return;}
		hsock->_sendbuff = newbuf;
		hsock->_send_buf.size = hsock->_send_buf.offset + len;
	}
	memcpy(hsock->_sendbuff + hsock->_send_buf.offset, data, len);
	hsock->_send_buf.offset += len;
	__sync_fetch_and_and(&hsock->_send_buf.lock_flag, 0);
	epoll_mod_write(hsock, WRITE);*/
}

bool HsocketSend(HSOCKET hsock, const char* data, int len)
{
	if (hsock == NULL || hsock->fd < 0 ||len <= 0) return false;

	if (hsock->_conn_type == UDP_CONN)
		HsocketSendUdp(hsock, data, len);
	else
		HsocketSendTcp(hsock, data, len);
	return true;
}

EPOLL_BUFF* HsocketGetBuff()
{
	EPOLL_BUFF* epoll_Buff = (EPOLL_BUFF*)malloc(sizeof(EPOLL_BUFF));
	if (epoll_Buff){
		memset(epoll_Buff, 0x0, sizeof(EPOLL_BUFF));
		epoll_Buff->buff = (char*)malloc(DATA_BUFSIZE);
		if (epoll_Buff->buff){
			*(epoll_Buff->buff) = 0x0;
			epoll_Buff->size = DATA_BUFSIZE;
		}
	}
	return epoll_Buff;
}

bool HsocketSetBuff(EPOLL_BUFF* epoll_Buff, const char* data, int len)
{
	if (epoll_Buff == NULL) return false;
	int left = epoll_Buff->size - epoll_Buff->offset;
	if (left >= len)
	{
		memcpy(epoll_Buff->buff + epoll_Buff->offset, data, len);
		epoll_Buff->offset += len;
	}
	else
	{
		char* new_ptr = (char*)realloc(epoll_Buff->buff, epoll_Buff->size + len);
		if (new_ptr)
		{
			epoll_Buff->buff = new_ptr;
			memcpy(epoll_Buff->buff + epoll_Buff->offset, data, len);
			epoll_Buff->offset += len;
		}
	}
	return true;
}

bool HsocketSendBuff(EPOLL_SOCKET* hsock, EPOLL_BUFF* epoll_Buff)
{
	if (hsock == NULL || epoll_Buff == NULL) return false;
	if (hsock->_conn_type == UDP_CONN)
		HsocketSendUdp(hsock, epoll_Buff->buff, epoll_Buff->offset);
	else
		HsocketSendTcp(hsock, epoll_Buff->buff, epoll_Buff->offset);
	free(epoll_Buff->buff);
	free(epoll_Buff);
	return true;
}

bool HsocketClose(HSOCKET hsock)
{
	if (hsock == NULL || hsock->fd < 0) return false;
	shutdown(hsock->fd, SHUT_RD);
	//close(hsock->fd);  直接关闭epoll没有事件通知，所以需要shutdown
	return true;
}

int HsocketSkipBuf(HSOCKET hsock, int len)
{
	hsock->_recv_buf.offset -= len;
	memmove(hsock->recv_buf, hsock->recv_buf + len, hsock->_recv_buf.offset);
	return hsock->_recv_buf.offset;
}