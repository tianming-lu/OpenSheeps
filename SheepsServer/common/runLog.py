#
#   @author:lutianming,create date:2018-06-15, QQ641471957
#
import datetime,os,threading,sys
from common.gdata import *
from common.config import *

mu = threading.Lock()
log_file = config['LOG'].get('server_file')

def getLogFile():
    return log_file.replace('"','')


def LOG(showtips, message):
    msg = "[{}]:{}".format(datetime.datetime.now().strftime('%H:%M:%S'), message)
    WriteLogFile(msg)
    try:
        if AllForm['Main'] != None:
            ShowMessage.append((showtips,message))
    except:
        pass

def LogDebug(message):
    msg = "[{}]:{}".format(datetime.datetime.now().strftime('%H:%M:%S'), message)
    return msg

def WriteLogFile(message):
    if mu.acquire(True):
        with open(getLogFile(),'a',encoding='utf-8') as f:
            f.write("{}\n".format(message))
        mu.release()

def CleanLogFile():
    try:
        os.remove(getLogFile())
    except:
        pass


def InitRunLog():
    try:
        os.mkdir("./log")
    except:
        pass
    CleanLogFile()      #清除旧日志
    myerrcs = myErrorConsole()  # 重定向标准错误输出
    sys.stderr = myerrcs
    # myoutcs = myOutConsole()
    # sys.stdout = myoutcs


class myErrorConsole():
    def __init__(self):
        self.__console_err__ = sys.stderr
        self.__console_out__ = sys.stdout

    def write(self, stream):
        self.__console_err__.write(stream)
        LOG(0, stream)

    def flush(self):
        pass

class myOutConsole():
    def __init__(self):
        self.__console_out__ = sys.stdout

    def write(self, stream):
        if len(stream) == 1:
            return
        self.__console_out__.write(stream+'\n')
        LOG(0, stream)

    def flush(self):
        pass