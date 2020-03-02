#
#   @author:lutianming,create date:2018-06-15, QQ641471957
#
'''
    此模块用于存放全局变量，用于线程间通信
'''
ShowMessage = []        #日志显示队列

ShowForm = []           #窗口开关队列
AllForm = {}            #窗口全局存储

SyncMessage = {}        #线程消息同步

AppExit = False        #app退出标志，各个窗口信号线程中都应该检查这个标志，当值为True时，结束线程，清理窗口对象并关闭

# AppExit func
def set_AppExit(value):
    #value is bool
    global AppExit
    AppExit = value

def get_AppExit():
    return AppExit
