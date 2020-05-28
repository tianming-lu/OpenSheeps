#
#   @author:lutianming,create date:2019-04-15, QQ641471957
#
from socket import *
import selectors
import threading
import traceback
import time

class MyRPCServer():
    def __init__(self):
        self.Running = False
        self.RunningThread = 0
        self.MsgList = []
        self.WorkerCount = 4
        for i in range(self.WorkerCount):
            self.MsgList.append([])
        self.sel = selectors.DefaultSelector()

    def RunForever(self):
        if self.RunningThread != 0:
            return False
        self.th = threading.Thread(target=self.Run)
        self.th.start()
        return True

    def Stop(self):
        self.Running = False

    def Run(self):
        self.Running = True
        for i in range(1):
            th = threading.Thread(target=self.SubThread)
            th.start()
        for i in range(self.WorkerCount):
            th = threading.Thread(target=self.WorkThread, args=(i,))
            th.start()

    def factory_run(self, factory):
        factory.listensock = socket(AF_INET, SOCK_STREAM)
        factory.listensock.setsockopt(SOL_SOCKET, SO_REUSEADDR, 1)
        factory.listensock.bind(('0.0.0.0', factory.listen_port))
        factory.listensock.listen(10)
        factory.listensock.setblocking(1)  # 设置socket为阻塞模式

        factory.hsock = HandSocket()
        factory.hsock.sock = factory.listensock
        factory.hsock.nettype = 1
        factory.hsock.user = factory.protocol

        self.sel.register(factory.listensock, selectors.EVENT_READ, factory.hsock)

    def SubThread(self):
        self.RunningThread += 1
        while self.Running:
            try:
                events = self.sel.select(None)  # 检测所有的fileobj，是否有完成wait data的
            except:
                # print(traceback.format_exc())
                continue
            for sel_obj, event in events:
                hsock = sel_obj.data
                # print(hsock.nettype, event, hsock.sock.fileno())
                if hsock.nettype == 0:
                    self.read(sel_obj.fileobj, hsock, hsock.user)  # read
                elif hsock.nettype == 1:
                    self.accept(sel_obj.fileobj, hsock)   # accpet
                elif hsock.nettype == 2:
                    self.write(sel_obj.fileobj, hsock, hsock.user)
        self.RunningThread -= 1

    def WorkThread(self, number):
        msg_list = self.MsgList[number]
        while self.Running:
            time.sleep(0.001)
            if len(msg_list) == 0:
                continue
            type, protocol, conn, ip, port, data = msg_list[0]
            del msg_list[0]
            if type == 1:
                protocol.Recv(conn, ip, port, data)
            elif type == 0:
                protocol.ConnectionMade(conn, ip, port)
            elif type == 2:
                protocol.ConnectionClosed(conn)
        msg_list.clear()

    def accept(self, listensock, listenhsock):
        try:
            conn, addr = listensock.accept()
            conn.setblocking(1)
        except:
            # print(0, traceback.format_exc())
            return
        protocol = listenhsock.user(self)
        hsock = HandSocket()
        hsock.sock = conn
        hsock.nettype = 0
        hsock.type = 1
        hsock.user = protocol
        hsock.ip = addr[0]
        hsock.port = addr[1]
        self.sel.register(conn, selectors.EVENT_READ, hsock)

        index = hash(protocol) % self.WorkerCount
        self.MsgList[index].append((0, protocol, conn, addr[0], addr[1], None))

    def close(self, conn, protocol):
        if conn is None:
            return
        try:
            self.sel.unregister(conn)
        except:
            # print(0, traceback.format_exc())
            return
        index = hash(protocol) % self.WorkerCount
        self.MsgList[index].append((2, protocol, conn, None, None, None))
        conn.close()
        pass

    def write(self, conn, hsock, protocol):
        print("do connex")
        protocol.ConnectionMade(conn, hsock.ip, hsock.port)
        hsock.sock.setblocking(1)
        hsock.nettype = 0
        self.sel.modify(hsock.sock, selectors.EVENT_READ, hsock)
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
            index = hash(protocol) % self.WorkerCount
            self.MsgList[index].append((1, protocol, conn, hsock.ip, hsock.port, data))

        elif hsock.type == 2:
            try:
                data, addr = conn.recvfrom(40960)
                index = hash(protocol) % self.WorkerCount
                self.MsgList[index].append((1, protocol, conn, addr[0], addr[1], data))
            except:
                self.close(conn, protocol)

    def TCPConnect(self, host, port, proto):
        conn = socket(AF_INET, SOCK_STREAM)
        conn.setblocking(1)

        hsock = HandSocket()
        hsock.sock = conn
        # hsock.nettype = 2
        hsock.type = 1
        hsock.user = proto
        hsock.ip = host
        hsock.port = port
        try:
            conn.connect_ex((host, port))
        except:
            print(0, traceback.format_exc())
            return None
        self.sel.register(conn, selectors.EVENT_READ, hsock)
        return conn

    def UDPConnect(self, host, port, proto):
        conn = socket(AF_INET, SOCK_DGRAM)
        conn.setblocking(1)
        conn.bind((host, port))

        hsock = HandSocket()
        hsock.sock = conn
        hsock.type = 2
        hsock.user = proto
        self.sel.register(conn, selectors.EVENT_READ, hsock)
        return conn


class HandSocket():
    def __init__(self):
        self.sock = None
        self.nettype = 0    #0 read  1 accept 2 connectex
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


class BaseFactory():
    def __init__(self, listen_port, protocol):
        self.listen_port = listen_port
        self.protocol = protocol
        self.hsock = None

    def factory_forever(self):
        pass


if __name__ == '__main__':
    fc = BaseFactory(1080, BaseProtocol)
    rpc = MyRPCServer()
    rpc.factory_run(fc)
    rpc.RunForever()