#
#   @author:lutianming,create date:2019-05-15, QQ641471957
#
from BaseClass.myRPCServer import MyRPCServer,BaseProtocol
from common.mySqlite import *
import socket,struct,threading,os,traceback,json,time,base64,hashlib

MaxCpuCount = 0
StressRecord = False
StressRecordMsg = []

ProxyClients = {}
StressClients = {}
StressTaskConfigs = {}
StressTasks = {}

ConfigIdIndex = 1
class StressTaskConfig():
    def __init__(self, projectId, projectname, envid, totalUsers, onceUsers, spaceTime, loopModel, ignoreErr, stressClients, desServers):
        global ConfigIdIndex
        self.configId = ConfigIdIndex
        ConfigIdIndex += 1
        self.projectId = projectId
        self.projectname = projectname
        self.envid = envid
        self.totalUsers = totalUsers
        self.onceUsers = onceUsers
        self.spaceTime = spaceTime
        self.loopModel = loopModel
        self.ignoreErr = ignoreErr
        self.StressClients = stressClients
        self.DesServers = desServers
        self.description = f'{self.projectname}-{self.totalUsers}/{self.onceUsers}/{self.spaceTime}-{self.loopModel}'
        self.configState = '未开始'
        self.Running = False

class TaskError():
    def __init__(self, errno):
        self.ErrNo = errno
        self.ErrList = []
        pass

class StressTask():
    def __init__(self, config):
        self.config = config
        self.TaskId =  config.configId
        self.userCount = 0
        self.loopModel = config.loopModel
        self.TaskState = 0  # 0未初始化  1初始化完成 2
        self.TaskTempMsg = []
        self.Err = {}

class ServerProtocol(BaseProtocol):
    def __init__(self, conn,  server):
        BaseProtocol.__init__(self, conn, server)
        #common
        self.ClientType = 0     # 2代理客户端 1压力客户端
        peer = self.conn.getpeername()
        self.PeerAddr = peer[0]
        self.PeerPort = peer[1]
        #proxy
        self.ServerState = 0  # 0未验证 1验证成功 2代理成功
        self.remote = None
        self.RemoteProtocol = None
        #stress
        self.buff = b''
        self.CpuCount = 0
        self.StressState = '连接成功'
        self.dead = False

    def ConnectionMade(self):
        # print(self.PeerAddr, self.PeerPort)
        pass

    def ConnectionClosed(self):
        #stress
        if self.dead is True:
            return

        global MaxCpuCount
        MaxCpuCount -= self.CpuCount
        if f'{self.fd}' in StressClients.keys():
            del StressClients[f'{self.fd}']
        #proxy
        if self.ServerState == 2:
            if StressRecord == True:
                StressRecordMsg.append((self.proxyAddr, self.proxyPort, time.time(), 2, ''))
            if f'{self.fd}' in ProxyClients.keys():
                del ProxyClients[f'{self.fd}']
            self.service.close(self.remote, self.RemoteProtocol)
        # common
        self.ServerState = 0
        self.ClientType = 0
        self.dead = True

    def Recv(self, data):
        if self.ClientType == 0:
            self.auth(data)
        elif self.ClientType == 1:
            self.StressRecv(data)
        else:
            self.ProxyRecv(data)

    def auth(self, data):
        # print('登录验证：',data[0])
        if data[0] == 5:
            self.ClientType = 2
            self.conn.send(b'\x05\x00')
            self.ServerState = 1
            return True
        else:
            self.StressRecv(data)
            self.ClientType = 1

    def ProxyRecv(self, data):
        if self.ServerState == 1:
            self.connect_dst_server(data)
        else:
            self.remoteMsg(data)

    def connect_dst_server(self, data):
        # print("进行代理设置：")
        self.proxyVer = data[0]
        self.proxyCmd = data[1]
        self.proxyType = data[3]
        if self.proxyType == 0x01:
            self.proxyAddr = socket.inet_ntoa(data[4: len(data) - 2])
        if self.proxyType == 0x03:
            self.proxyAddr = str(data[5:len(data) - 2],encoding='utf-8')
        self.proxyPort = struct.unpack('>H', data[len(data) - 2: len(data)])[0]

        protocol = self.service.TCPConnect(self.proxyAddr, self.proxyPort, ClientProtocol)
        if protocol == None:
            return False
        # remote, protocol = res
        self.remote = protocol.conn
        self.RemoteProtocol = protocol
        protocol.remote = self.conn
        protocol.RemoteProtocol = self

        if StressRecord == True:
            StressRecordMsg.append((self.proxyAddr, self.proxyPort, time.time(), 0, ''))

        replay = b'\x05\x00\x00\x01'
        loaddr = self.conn.getsockname()
        replay += socket.inet_aton(loaddr[0]) + struct.pack(">H", loaddr[1])
        self.conn.send(replay)
        ProxyClients[f'{self.fd}'] = self
        self.ServerState = 2
        return True

    def remoteMsg(self, data):
        try:
            self.remote.send(data)
        except:
            return False
        if StressRecord == True:
            # StressRecordMsg.append((self.proxyAddr, self.proxyPort, time.time(), 1, data.hex()))
            StressRecordMsg.append((self.proxyAddr, self.proxyPort, time.time(), 1, str(base64.standard_b64encode(data), encoding='utf-8')))

    def StressRecv(self, data):
        self.buff += data
        while True:
            ret = self.StressRecvCheck()
            if ret == 0:
                break
            self.buff = self.buff[ret:len(self.buff)]
            pass

    def StressRecvCheck(self):
        if len(self.buff) < 8:
            return 0
        msglen, cmdNo = struct.unpack('2I', self.buff[0:8])
        if msglen > len(self.buff):
            return 0
        body, = struct.unpack(f'{msglen-8}s', self.buff[8:msglen])
        try:
            body = str(body, encoding="utf-8")
        except:
            body = str(body, encoding="gbk")
        try:
            body = body.replace('\\', '\\\\')
            body = json.loads(body)
        except:
            LOG(1, traceback.format_exc())
            return msglen
        # print(msglen, cmdNo, body)
        self.StressRequest(cmdNo, body)
        return msglen

    def StressRequest(self, cmdMo, body):
        try:
            func = getattr(self, f'StressRequestCmdNo_{cmdMo}')
            func(body)
            return True
        except:
            # LOG(1, traceback.format_exc())
            return False

    def StressRequestCmdNo_1(self, body):
        self.CpuCount = body.get('CPU')
        if self.CpuCount == None:
            return None
        global MaxCpuCount
        MaxCpuCount += self.CpuCount
        StressClients[f'{self.fd}'] = self
        self.Send(b'#\x00\x00\x00\x01\x00\x00\x00{"retCode":0,"retMsg":"OK"}')
        self.StressSendFilelist()

    def StressSendFilelist(self):
        jsonroot = {}
        filelist = []
        rootpath = ".\\"
        for root,dir,files in os.walk(rootpath):
            if root == rootpath or root.find("log") != -1:
                continue
            for file in files:
                item = {}
                filename = os.path.join(root,file)
                md5hash = self.getfilemd5(filename)
                # print(filename, os.path.getsize(filename), md5hash)
                item["File"] = filename
                # item["file"] = base64.standard_b64encode(str(filename))
                item["Size"] = os.path.getsize(filename)
                item["Md5"] = md5hash
                filelist.append(item)
        jsonroot["filelist"] = filelist
        data = bytes(str(jsonroot).replace('\'', '"'), encoding='utf-8')
        byte_msg = struct.pack(f'2I{len(data)}s', len(data) + 8, 9, data)
        print(byte_msg)
        self.Send(byte_msg)

    def getfilemd5(self, file):
        with open(file, 'rb') as f:
            md5obj = hashlib.md5()
            md5obj.update(f.read())
            hash = md5obj.hexdigest()
        return hash

    def StressRequestCmdNo_10(self, body):
        self.StressState = '资源同步中'
        file = body.get("File")
        offset = body.get("Offset")
        size = body.get("Size")
        self.StressSendFileData(file, offset, size)
        # th = threading.Thread(target=self.StressSendFileData, args=(file, offset, size))
        # th.start()

    def StressSendFileData(self, file, offset, size):
        file = file.replace('\\\\', '\\')
        f = open(file, 'rb')
        f.seek(offset, 0)
        minis = 10240
        count = int(size/minis) + 1
        lastdata = size%minis
        for i in range(count):
            if i == count - 1:
                cursize = lastdata
            else:
                cursize = minis
            content = f.read(cursize)
            content = base64.standard_b64encode(content)
            data = {}
            data["File"] = file
            data["Offset"] = offset
            data["Order"] = i
            data["Size"] = cursize
            data["Data"] = str(content,encoding='utf-8')
            data["retCode"] = 0
            data["retMsg"] = "OK"
            data = bytes(str(data).replace('\'', '"'), encoding='utf-8')
            byte_msg = struct.pack(f'2I{len(data)}s', len(data) + 8, 10, data)
            self.Send(byte_msg)
        f.close()

    def StressRequestCmdNo_11(self, body):
        self.StressState = '就绪'
        pass

    def StressRequestCmdNo_13(self, body):
        taskid = body.get("TaskID")
        errid = body.get("ErrType")
        task = StressTasks[f"{taskid}"]
        if f"{errid}" not in  task.Err.keys():
            task.Err[f"{errid}"] = TaskError(errid)
        errobj = task.Err[f"{errid}"]
        errobj.ErrList.append(body)
        # print(errobj)

    def StressSend(self, cmdNo, data):
        pass

class ClientProtocol(BaseProtocol):
    def __init__(self, conn,  server):
        BaseProtocol.__init__(self, conn, server)
        self.remote = None
        self.RemoteProtocol = None
        self.dead = False
        peer = self.conn.getpeername()
        self.PeerAddr = peer[0]
        self.PeerPort = peer[1]

    def ConnectionMade(self):
        # print('made', self.conn)
        pass

    def ConnectionClosed(self):
        if self.dead is True:
            return
        self.service.close(self.remote, self.RemoteProtocol)
        self.dead = True
        pass

    def Recv(self, data):
        try:
            self.remote.send(data)
            if StressRecord == True:
                # StressRecordMsg.append((self.proxyAddr, self.proxyPort, time.time(), 1, data.hex()))
                StressRecordMsg.append((self.PeerAddr, self.PeerPort, time.time(), 3, str(base64.standard_b64encode(data), encoding='utf-8')))
        except:
            # print(traceback.format_exc())
            pass
        pass


class StressTestServerPort():
    def __init__(self):
        self.server = MyRPCServer(1080, ServerProtocol, None)

    def start(self):
        th = threading.Thread(target=self.Go)
        th.start()
        pass

    def Go(self):
        if self.server.RunForever():
            LOG(0, f'压力测试控制端:服务启动，IP-端口号：{socket.gethostbyname(socket.getfqdn(socket.gethostname()))}:1080')
        else:
            LOG(0, f'压力测试控制端:启动失败，请稍候再尝试……')
        pass

    def Stop(self):
        if self.server != None:
            self.server.Stop()
        LOG(0, '压力测试控制端:服务关闭')
        pass

    def RunInThread(self, funcAddr, agr=()):
        th = threading.Thread(target=funcAddr, args=agr)
        th.start()

    def SressRecordToDb(self):
        global StressRecord
        StressRecord = True
        del_old_stress_record_table()
        StressRecordMsg.clear()
        while StressRecord:
            for i in range(10):
                if len(StressRecordMsg) == 0:
                    break
                ip, port, intime, type, content = StressRecordMsg[0]
                write_stress_record_to_db(ip, port, intime, type, content)
                del StressRecordMsg[0]
            time.sleep(0.1)

    def StartStressRecord(self):
        self.RunInThread(self.SressRecordToDb, ())

    def StopStressRecord(self):
        global StressRecord
        StressRecord = False

    def StartTaskConfig(self, configid):
        self.RunInThread(self.SubStartTaskConfig, (configid,))

    def AddTaskUser(self, configid, count):
        config = StressTaskConfigs[configid]
        temp = config.totalUsers + count
        if temp < 0:
            temp = 0
        config.totalUsers = temp
        config.description = f'{config.projectname}-{config.totalUsers}/{config.onceUsers}/{config.spaceTime}-{config.loopModel}'
        # print(configid, count)
        pass

    def SubStartTaskConfig(self, configid):
        config = StressTaskConfigs[configid]
        config.Running = True
        config.configState = '执行中'
        task = StressTask(config)
        StressTasks[f'{task.TaskId}'] = task
        self.StressTaskRun(task)

    def StressTaskRun(self, task):
        self.StressTaskPublish(task)
        self.StressTaskLoop(task)
        self.StressTaskEnd(task)
        pass

    def get_task_onceuser(self, task):
        last_user = task.config.totalUsers - task.userCount
        if task.config.onceUsers > last_user:
            usercount = last_user
        else:
            usercount = task.config.onceUsers
        task.userCount += usercount
        return usercount

    def StressTaskPublish(self, task):
        clentCount = len(task.config.StressClients)
        if clentCount == 0:
            LOG(1, "压测受控端在线数量：0")
            return False
        usercount = self.get_task_onceuser(task)
        a = usercount / clentCount
        b = usercount % clentCount
        machineId = 0
        for proto in task.config.StressClients.values():
            data = {}
            data["TaskID"] = task.TaskId
            usercount = a
            if b > 0:
                usercount = a + 1
                b -= 1
            data["UserCount"] = int(usercount)
            data["projectID"] = task.config.projectId
            data["EnvID"] = task.config.envid
            data["MachineID"] = machineId
            if task.config.loopModel == 0 and task.config.ignoreErr == True:
                data["IgnoreErr"] = True
            data = bytes(str(json.dumps(data)).replace('\'','"'), encoding='utf-8')
            byte_msg = struct.pack(f'2I{len(data)}s', len(data) + 8, 2, data)
            proto.Send(byte_msg)
            machineId += 1
        pass

    def StressTaskLoop(self, task):
        last_change_user = time.time()
        start_record = 0
        start_real = 0
        start_row = 0
        stop_record = False
        while task.config.Running == True:
            '''分批增加用户'''
            cur_chang_user = time.time()
            if cur_chang_user - last_change_user >= task.config.spaceTime:
                user_chang = self.get_task_onceuser(task)
                clentCount = len(task.config.StressClients)
                if user_chang != 0 and clentCount > 0:
                    a = user_chang / clentCount
                    b = user_chang % clentCount
                    for proto in task.config.StressClients.values():
                        data = {}
                        data["TaskID"] = task.TaskId
                        usercount = a
                        if b > 0:
                            usercount = a + 1
                            b -= 1
                        data["Change"] = int(usercount)
                        data = bytes(str(data).replace('\'', '"'), encoding='utf-8')
                        byte_msg = struct.pack(f'2I{len(data)}s', len(data) + 8, 12, data)
                        proto.Send(byte_msg)
                last_change_user = cur_chang_user
            '''分批增加用户'''
            '''按时间顺序发送录制消息'''
            if stop_record == True:
                time.sleep(1)
                continue
            if len(task.TaskTempMsg) == 0:
                sqlcmd = self.CreateSQLCommand(task, start_row)
                if sqlcmd == None:
                    time.sleep(1)
                    continue
                list = get_stress_record_message(sqlcmd)
                start_row += len(list)
                task.TaskTempMsg.extend(list)

                #判断循环次数
                if len(list) == 0 and task.loopModel != 2:
                    self.ReInitTask(task)
                    stop_record = True
                    task.TaskTempMsg.clear()
                    task.loopModel = 2
                    continue

            if len(task.TaskTempMsg) == 0:
                time.sleep(0.1)
                continue
            recordtime, recordtype, ip, port, content = task.TaskTempMsg[0]
            if start_real == 0:
                start_real = time.time()
                start_record = recordtime
            should_time = recordtime - start_record
            real_time = time.time() - start_real

            if should_time <= real_time:
                self.PublishTaskMsg(task, recordtype, ip, port, content, recordtime)
                del task.TaskTempMsg[0]
            else:
                # time.sleep(should_time - real_time)
                time.sleep(0.01)
            '''按时间顺序发送录制消息'''
            pass

    def CreateSQLCommand(self,task, start_row):
        sqlcmd = ''
        for i in range(len(task.config.DesServers)):
            record_ip, record_port, play_ip, play_prot = task.config.DesServers[i]
            tablename = f't_data_stress_record_{record_ip.replace(".","_")}_{record_port}'
            if table_exiest(tablename):
                tem = 'select recordtime,recordtype,ip,port,content from {} where recordtype < 3'.format(tablename)
                if len(sqlcmd) > 0:
                    tem = ' union ' + tem
                sqlcmd = sqlcmd + tem
        if sqlcmd == '':
            return None
        return sqlcmd + ' order by recordtime limit {},100'.format(start_row)

    def ReInitTask(self, task):
        loop = False
        if task.config.loopModel == 0:
            loop = True
        data = {}
        data["TaskID"] = task.TaskId
        data["Loop"] = loop
        data = bytes(str(json.dumps(data)).replace('\'', '"'), encoding='utf-8')
        byte_msg = struct.pack(f'2I{len(data)}s', len(data) + 8,  8, data)
        for proto in task.config.StressClients.values():
            proto.Send(byte_msg)
        pass

    def PublishTaskMsg(self, task, recordtype, ip, port, content, recordtime):
        data = {}
        data["TaskID"] = task.TaskId
        data["Host"] = ip
        data["Port"] = port
        data["Timestamp"] = int(recordtime)
        data["Microsecond"] = int((recordtime - int(recordtime)) * 1000000)
        for row in task.config.DesServers:
            recordIP, recordPort, playIP, playPort = row
            if recordIP == ip and recordPort == port and playIP != '' and playPort != '':
                data["Host"] = playIP
                data["Port"] = int(playPort)
                break
        if recordtype == 1:
            data["Msg"] = content
        data = bytes(str(data).replace('\'','"'), encoding='utf-8')
        byte_msg = struct.pack(f'2I{len(data)}s', len(data) + 8, recordtype+3, data)
        for proto in task.config.StressClients.values():
            proto.Send(byte_msg)

    def StressTaskEnd(self, task):
        task.TaskTempMsg.clear()
        data = {}
        data["TaskID"] = task.TaskId
        data = bytes(str(data).replace('\'','"'), encoding='utf-8')
        byte_msg = struct.pack(f'2I{len(data)}s', len(data) + 8, 6, data)
        for proto in task.config.StressClients.values():
            proto.Send(byte_msg)
        pass

if __name__ == '__main__':
    # del_old_stress_record_table()
    stsp = StressTestServerPort()
    stsp.start()
    stsp.RunInThread(stsp.SressRecordToDb, ())