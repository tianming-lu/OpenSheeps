# Sheeps

#### 介绍

此系统是通用于TCP服务器的压力测试工具，初始是为了完成对游戏服务器的压力测试，探知游戏服务器所能承载的最大在线人数，经历多次迭代，最终演化成型，并命名为Sheeps（羊群）。


#### 架构图示

![架构图示](https://images.gitee.com/uploads/images/2020/0319/210948_ce304e88_1564139.png "framework.png")


#### 工作原理

本系统采用协议录制、回放的方式进行压力测试。分为控制端和受控端，控制端负责录制、回放及消息分发，受控端完成项目接入后，接收控制端命令及消息，建立大量连接至目标服务器，作为压力发起端，并支持分布式部署。此过程中受控端通过调用项目接入层，对网络封包进行解析、修改、再打包发送至服务器，实现压测每个压测用户的差异化处理。


#### 开发环境

控制端：python3.7,pyqt5  
受控端：Visual Studio 2019, C++


#### 如何接入项目？
请查看项目示例：example，该项目中每个函数均有备注其作用说明
此项目为Visual Studio 2019创建，使用C++开发，压力测试框架不支持32位程序，所以此项目应该编译为64位程序。
将编译好的Replay.dll文件放到0目录


#### 接入项目需要掌握哪些API？

Task_API HMESSAGE	TaskGetNextMessage(UserProtocol* proto);  
Task_API bool		TaskUserDead(UserProtocol* proto, const char* fmt, ...);  
Task_API bool		TaskUserSocketClose(HSOCKET hsock);  
Task_API void		TaskUserLog(UserProtocol* proto, int level, const char* fmt, ...);  
Task_API void		TaskLog(t_task_config* task, int level, const char* fmt, ...);  
#define		TaskUserSocketConnet(ip, port, proto)	IOCPConnectEx(ip, port, proto)  
#define		TaskUserSocketSend(hsock, data, len)	IOCPPostSendEx(hsock, data, len)  

以上是完成项目接入所必须了解的API函数；

TaskGetNextMessage：用于从Task Manager中获取当前消息，详细请看示例项目中的代码处理；  
TaskUserDead：用于向Task Manager报告用户自毁，Task Manager会将其从队列中移除；  
TaskUserSocketConnet，TaskUserSocketSend，TaskUserSocketClose：分别用于用户发起连接、发送消息、关闭连接等网络操作，其中连接为异步连接，IO Manger在连接成功或者失败后会通知用户，IO Manager会调用用户成员函数ConnectionMade或者ConnectionFailed；但是通过调用TaskUserSocketClose关闭连接时，网络连接会立即关闭，IO Manager不会再通知连接关闭事件；  
TaskUserLog，TaskLog：均用于输出日志，不同的是一个传递参数用户类指针，一个传递任务结构指针，TaskUserLog输出时会携带用户序号，用于区分不同用户的日志流



#### 使用说明

如何使用？  
 ![输入图片说明](https://images.gitee.com/uploads/images/2020/0319/212425_4f13ccde_1564139.png "界面.png")

完成了项目接入，如何进行压力测试呢？总共分为两步；一，录制；二，回放。

#录制
1.	启动控制端程序，程序会自动启动socks5代理服务，端口为1080；
2.	启动需要录制网络消息的项目客户端，并设置为通过代理连接（Android系统推荐使用ProxyDroid，windows系统推荐使用proxifer），连接IP为控制端运行机器的IP地址，端口为1080，代理类型为socks5，确认代理设置完成后，关闭需要录制的项目客户端程序，准备工作就绪。
3.	在控制端程序窗口点击“启动录制”按钮，此时按钮文字变为“关闭录制”
4.	启动需要录制的项目客户端，进行正常的使用操作，需要录制的操作都完成后，关闭该项目客户端，
5.	在控制端程序窗口点击“关闭录制”，请注意按钮旁的剩余数字，显示0说明所有消息均已保存入库，目标文件data.db
6.	在控制端程序“录制列表”中查看录制的所有网络连接，确认所需要录制的网络连接存在列表中

#回放
1.	启动控制端程序，确认需要的网络连接存在于录制列表中
2.	右击需要回放的连接，选择“添加到回放”，此时在回放栏可以看到
3.	在回放栏中选择默认项目，需要压测用户的总数，单次数量，时间间隔，循环模式可以选择循环或者不循环，如果不希望用户因为意外错误而自毁，可以勾选“忽略错误”
4.	配置完成后，点击“添加任务”，此时在“任务列表”中可以刚刚配置的回放任务
5.	点击控制端程序右上角“启动本地受控端”，此时在“受控端列表”中可以看到刚刚启动的受控端
6.	在任务列表中右击刚刚配置好的回放任务，点击开始，压测开始进行，可以在日志目录中查看压测过程是否正常
7.	如要停止回放，在任务列表中右击回放任务，点击停止即可


#### 代码引用说明

1. 本系统使用了开源的cjson库：https://sourceforge.net/projects/cjson/
2. 本系统使用以前项目积累的sha1和md5算法源码，时间久远已记不清出处
3. 感谢以上开源代码或项目，如有侵犯版权，请与我联系


#### 赞助
如果此项目对您有所帮助，你可以选择赞助，以激励作者开发

![输入图片说明](https://images.gitee.com/uploads/images/2020/0319/213236_e02ab520_1564139.png "zhifubao.png")![输入图片说明](https://images.gitee.com/uploads/images/2020/0319/213251_ffdd4528_1564139.png "weixin.png")
