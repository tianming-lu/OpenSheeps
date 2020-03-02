#
#   @author:lutianming,create date:2019-04-15, QQ641471957
#
from socket import *
from multiprocessing import cpu_count
import selectors,threading,traceback

ThreadData = threading.local()

class MyRPCServer():
    def __init__(self, port, protocol, timeout):
        self.Running = False
        self.RunningThread = 0
        self.port = port
        self.protocol = protocol
        self.time = timeout

    def RunForever(self):
        if self.RunningThread != 0:
            return False
        self.th = threading.Thread(target=self.Run)
        self.th.start()
        return True

    def Stop(self):
        self.Running = False

    def Run(self):
        self.server_conn = socket(AF_INET, SOCK_STREAM)
        self.server_conn.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        self.server_conn.bind(('0.0.0.0', self.port))
        self.server_conn.listen(10)
        self.server_conn.setblocking(1)  # 设置socket的接口为非阻塞
        # for i in range(1):
        self.Running = True
        # for i in range(cpu_count()*2):
        for i in range(1):
            th = threading.Thread(target=self.SubThread)
            th.start()

    def SubThread(self):
        self.RunningThread += 1
        ThreadData.ProtocolList = {}
        ThreadData.sel = selectors.DefaultSelector()
        ThreadData.sel.register(self.server_conn, selectors.EVENT_READ,None)
        while self.Running:
            try:
                events = ThreadData.sel.select(None)  # 检测所有的fileobj，是否有完成wait data的
            except:
                continue
            for sel_obj, event in events:
                if sel_obj.fileobj.fileno() == self.server_conn.fileno():
                    self.accept(sel_obj.fileobj)   # accpet
                else:
                    self.read(sel_obj.fileobj, sel_obj.data)  # read
            self.timeout()
        self.ClearThread()
        self.RunningThread -= 1

    def ClearThread(self):
        ThreadData.sel.unregister(self.server_conn)
        self.server_conn.close()
        while True:
            count = len(ThreadData.ProtocolList)
            if count <= 0:
                break
            for protocol in ThreadData.ProtocolList.values():
                self.close(protocol.conn, protocol)
                break


    def accept(self, server_fileobj):
        try:
            conn,addr=server_fileobj.accept()
            conn.setblocking(1)
        except:
            # print(0, traceback.format_exc())
            return
        protocol = self.protocol(conn, self)
        protocol.ConnectionMade()
        ThreadData.ProtocolList[f'{conn.fileno()}'] = protocol
        ThreadData.sel.register(conn,selectors.EVENT_READ,protocol)

    def close(self, conn, protocol):
        try:
            ThreadData.sel.unregister(conn)
        except:
            return
        try:
            del ThreadData.ProtocolList[f'{conn.fileno()}']
        except:
            pass
        protocol.ConnectionClosed()
        conn.close()
        pass

    def read(self, conn, protocol):
        data = ''
        try:
            data = conn.recv(40960)
        except:
            pass
        if not data:
            self.close(conn, protocol)
            return
        protocol.Recv(data)

    def TCPConnect(self, host, port, protocol):
        conn = socket(AF_INET, SOCK_STREAM)
        conn.setblocking(1)
        try:
            # start = time.clock()
            conn.connect((host, port))
            proto = protocol(conn, self)
            proto.ConnectionMade()
            ThreadData.ProtocolList[f'{conn.fileno()}'] = proto
            ThreadData.sel.register(conn, selectors.EVENT_READ, proto)
            # end = time.clock()
            # print(end - start)
            return proto
        except:
            conn.close()
            return None
        pass

    def timeout(self):
        pass

class BaseProtocol():
    def __init__(self, conn, server):
        self.conn = conn
        self.fd = self.conn.fileno()
        self.service = server
        pass
    def ConnectionMade(self):
        pass
    def ConnectionClosed(self):
        pass
    def Recv(self, date):
        pass
    def Send(self, data):
        try:
            self.conn.send(data)
            return True
        except:
            print(traceback.format_exc())
            return False
    pass

if __name__ == '__main__':
    rpc = MyRPCServer(1080, BaseProtocol, None)
    rpc.RunForever()