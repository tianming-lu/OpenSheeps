# Sheeps

#### __QQ群：__ 113015080

#### 关于Sheeps

此系统是通用于TCP和UDP服务器的压力测试框架，基于数据包的录制和回放，集成时间控制、快进、暂停功能，初始是为了完成对游戏服务器的压力测试，探知游戏服务器所能承载的最大在线人数，经历多次迭代，最终演化成型，并命名为Sheeps（羊群）。


#### 架构图示

![输入图片说明](https://images.gitee.com/uploads/images/2020/0901/164845_f7597f15_1564139.png "QQ截图20200831162640.png")


#### 工作原理

本系统采用协议录制、回放的方式进行压力测试。分为控制端和负载端，控制端负责录制、回放及消息分发，负载端完成项目接入后，接收控制端命令及消息，建立大量连接至目标服务器，作为压力发起端，并支持分布式部署。此过程中负载端通过调用项目接入层，对网络封包进行解析、修改、再打包发送至服务器，实现压测每个压测用户的差异化处理。


#### 并发设计

本系统使用自有的一套基于windows IOCP实现的多线程Proactory网络模型，在尽可能获得高并发的同时，简化业务逻辑的开发难度。因此使用者应该了解两个概念HSOCKET、ReplayProtocol，HSOCKET为连接句柄，表示一个网络连接。ReplayProtocol为业务逻辑处理基类,在本系统中表示一个压测用户，每一个网络事件的产生，都会调用对应ReplayProtocol对象中的成员函数进行处理。每个连接都绑定一个ReplayProtocol对象，同时多个连接可以绑定到同一个对象，以应对单个用户在业务处理中需要连接多个服务，同时又不增加业务处理的难度。本系统现已支持在linux下运行，linux下采用epoll实现，并且API与windows下保持一致

#### 录制设计：

本系统内置了一个简单的SOCKS5代理服务器，只要将需要录制的目标客户端程序通过代理连接到网络，SOCKS5服务器就会将网络事件保存到文件中，网络事件类型包括连接、关闭和发送，每个事件都有对应的目标IP地址和端口号，发送事件会有相应的数据包，比较麻烦的是使用者需要自己寻找合适SOCKS5代理客户端，作者亲测安卓系统可以使用“代理机器人”（ProxyDroid，需要root），windows系统下可以使用ProxyCap，它支持代理UDP代理，不过它并不是免费的。

#### 回放设计

本系统的核心工作原理是基于录制与回放模式，采用类似播放音视频的设计，每个压测用户拥有自身的时间轴，并且在时间轴的基础上添加了播放状态的控制，比如播放、暂停、快进，默认情况下，用户处于播放状态，使用者根据业务需要随时变更用户状态，实现自身状态和服务器状态的同步。任务工作线程会循环调用ReplayProtocol对象成员函数，用户需要在此处理不同的回放事件，建立连接、发送消息，关闭连接等。

#### 如何接入项目？

1.请查看项目示例：example，该项目中每个函数均有备注其作用说明  
2.使用者需要着重理解ReplayProtocol每个成员函数的作用，每个接入的项目都需要实现一个继承ReplayProtocol的子类  
3.使用者必须编写四个回调函数，并且理解其作用，它们将作为启动压测框架的必须参数  
此项目为Visual Studio 2019创建，使用C++开发，目前本框架只提供64位的二进制程序下载，所以您的程序需要编译为64位程序，如需使用32位，请自行克隆源码并编译。


#### 接入项目需要掌握哪些API？


``` 
Task_API void		TaskManagerRun(int projectid, CREATEAPI create, DESTORYAPI destory, INIT taskstart, INIT taskstop);
Task_API bool		TaskUserDead(ReplayProtocol* proto, const char* fmt, ...);
Task_API bool		TaskUserSocketClose(HSOCKET hsock);
Task_API void		TaskUserLog(ReplayProtocol* proto, uint8_t level, const char* fmt, ...);
Task_API void		TaskLog(HTASKCFG task, uint8_t level, const char* fmt, ...);
#define		TaskUserSocketConnet(ip, port, proto, iotype)	IOCPConnectEx(ip, port, proto, iotype)
#define		TaskUserSocketSend(hsock, data, len)			IOCPPostSendEx(hsock, data, len)
#define		TaskUserSocketSkipBuf(hsock, len)				IOCPSkipHsocketBuf(hsock, len)  
```


以上是完成项目接入所必须了解的API函数，头文件 TaskManager.h；

TaskManagerRun：启动负载端必须调用，需要传入项目id及四个回调函数，分别为创建用户(ReplayProtocol对象)、销毁用户、任务开始、任务结束；  
TaskUserDead：用于向Task Manager报告用户自毁，Task Manager会将其从队列中移除；  
TaskUserSocketConnet，TaskUserSocketSend，TaskUserSocketClose：分别用于用户发起连接、发送消息、关闭连接等网络操作，其中连接为异步连接，
IO Manger在连接成功或者失败后会通知用户，IO Manager会调用用户成员函数ConnectionMade或者ConnectionFailed；但是通过调用TaskUserSocketClose关闭连接时，网络连接会立即关闭，IO Manager不会再通知连接关闭事件  
TaskUserSocketSkipBuf：需要从socket缓冲区中跳过的消息长度，用户已经处理过得网络消息需要调用此函数  
TaskUserLog，TaskLog：均用于输出日志，不同的是一个传递参数用户类指针，一个传递任务结构指针，TaskUserLog输出时会携带用户序号，用于区分不同用户的日志流  



#### 使用说明

如何使用？  
![输入图片说明](https://images.gitee.com/uploads/images/2020/0831/162547_d340cedd_1564139.png "图片1.png")

完成了项目接入，如何进行压力测试呢？总共分为两步；一，录制；二，回放。

__一、录制__

1.启动控制端程序，程序会自动启动socks5代理服务，端口为1080；  
2.启动需要录制网络消息的项目客户端，并设置为通过代理连接（Android系统推荐使用ProxyDroid，windows系统推荐使用proxifer），连接IP为控制端运行机器的IP地址，端口为1080，代理类型为socks5，确认代理设置完成后，关闭需要录制的项目客户端程序，准备工作就绪。  
3.本机访问http://127.0.0.1:1080/，在录制管理点击“开启”按钮，此时按钮文字变为“关闭”  
4.启动需要录制的项目客户端，进行正常的使用操作，需要录制的操作都完成后，关闭该项目客户端，  
5.在控制端程序窗口点击“关闭录制”，请注意按钮旁的剩余数字，显示0说明所有消息均已保存入库，目标文件data.db  
6.点击“展开数据表”查看录制的所有网络连接，确认所需要录制的网络连接存在列表中  

__二、回放__

1.本机访问http://127.0.0.1:1080/，点击任务管理  
2.填写任务配置信息，需要压测用户的总数，单次数量，时间间隔，选择需要回放的文件，以及连接，循环模式可以选择循环或者不循环，如果不希望用户因为意外错误而自毁，可以勾选“忽略错误”  
3.配置完成后，点击“添加任务”，此时在“任务列表”中可以刚刚配置的回放任务  
4.确定负载端启动运行，并连接成功  
5.在任务列表中点击开始，压测开始进行，可以在日志目录中查看压测过程是否正常  
6.如要停止回放，在任务列表中点击停止即可  


#### 代码引用说明

1.本系统使用了开源的cjson库：https://sourceforge.net/projects/cjson/  
2.本系统使用了开源的SQLite的C语言库：https://www.sqlite.org/index.html  
3.本系统使用以前项目积累的sha1和md5算法源码，时间久远已记不清出处  
4.感谢以上开源代码或项目，如有侵犯版权，请与我联系  

#### 下载二进制文件
[点击此处进入下载](https://gitee.com/lutianming/Sheeps/releases)

