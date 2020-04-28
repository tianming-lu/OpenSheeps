#
#   @author:lutianming,create date:2019-04-15, QQ641471957
#
from socket import *
import selectors
import threading
import traceback

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
        self.Running = True
        # for i in range(cpu_count()*2):
        for i in range(12):
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
                    self.read(sel_obj.fileobj, sel_obj.data, sel_obj.data.user)  # read
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
            conn, addr = server_fileobj.accept()
            conn.setblocking(1)
        except:
            # print(0, traceback.format_exc())
            return
        protocol = self.protocol(self)
        hsock = HandSocket()
        hsock.sock = conn
        hsock.type = 1
        hsock.user = protocol
        hsock.ip = addr[0]
        hsock.port = addr[1]
        ThreadData.ProtocolList[f'{conn.fileno()}'] = protocol
        ThreadData.sel.register(conn, selectors.EVENT_READ, hsock)
        protocol.ConnectionMade(conn, addr[0], addr[1])

    def close(self, conn, protocol):
        if conn is None:
            return
        try:
            ThreadData.sel.unregister(conn)
        except:
            # print(0, traceback.format_exc())
            return
        try:
            del ThreadData.ProtocolList[f'{conn.fileno()}']
        except:
            pass
        protocol.ConnectionClosed(conn)
        conn.close()
        pass

    def read(self, conn, hsock, protocol):
        data = ''
        if hsock.type == 1:
            try:
                data = conn.recv(40960)
            except:
                pass
            if not data:
                self.close(conn, protocol)
                return
            protocol.Recv(conn, hsock.ip, hsock.port, data)
        elif hsock.type == 2:
            data, addr = conn.recvfrom(1024)
            protocol.Recv(conn, addr[0], addr[1], data)

    def TCPConnect(self, host, port, proto):
        conn = socket(AF_INET, SOCK_STREAM)
        conn.setblocking(1)
        try:
            # start = time.clock()
            conn.connect((host, port))
            #proto.ConnectionMade(conn, host, port)

            hsock = HandSocket()
            hsock.sock = conn
            hsock.type = 1
            hsock.user = proto
            hsock.ip = host
            hsock.port = port
            ThreadData.ProtocolList[f'{conn.fileno()}'] = proto
            ThreadData.sel.register(conn, selectors.EVENT_READ, hsock)
            # end = time.clock()
            # print(end - start)
            return conn
        except:
            conn.close()
            return None

    def UDPConnect(self, host, port, proto):
        conn = socket(AF_INET, SOCK_DGRAM)
        conn.setblocking(1)
        conn.bind((host, port))
        #proto.ConnectionMade(conn, host, port)

        hsock = HandSocket()
        hsock.sock = conn
        hsock.type = 2
        hsock.user = proto
        ThreadData.ProtocolList[f'{conn.fileno()}'] = proto
        ThreadData.sel.register(conn, selectors.EVENT_READ, hsock)
        return conn

    def timeout(self):
        pass


class HandSocket():
    def __init__(self):
        self.sock = None
        self.type = 0    # 0未知 1TCP 2UDP 3BIND
        self.user = None
        self.ip = ''
        self.port = 0

class BaseProtocol():
    def __init__(self, server):
        self.service = server
        pass

    def ConnectionMade(self, conn, ip, port):
        pass

    def ConnectionClosed(self, conn):
        pass

    def Recv(self, conn, ip, port, date):
        pass

    def Send(self, conn, data):
        try:
            conn.send(data)
            return True
        except:
            # print(traceback.format_exc())
            return False
    pass


if __name__ == '__main__':
    rpc = MyRPCServer(1080, BaseProtocol, None)
    rpc.RunForever()