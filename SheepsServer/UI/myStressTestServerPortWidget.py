from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from PyQt5.QtCore import *
import PyQt5.sip
from UI.myStressTestServerPort import *
from Logic.StressTestServerPort import *
from common.mySqlite import *
from common.config import *
from ctypes import *

class StressThread(QThread):
    _signal_msg = pyqtSignal(int)
    def __init__(self, parent=None):
        self.RUNNING = True
        super(StressThread, self).__init__(parent=parent)

    def stop(self):
        self.RUNNING = False

    def run(self):
        while (self.RUNNING):
            self.msleep(1000)
            self._signal_msg.emit(0)

class Form_StressTestServerPort(QWidget, Ui_Form_StressTestServerPort):
    def __init__(self):
        QWidget.__init__(self)
        self.setupUi(self)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setFixedSize(self.width(), self.height())
        self.ClearThread = 0
        try:
            self.dll = CDLL("./Sheeps.dll")
        except:
            pass
        self.init_ui_content()
        self.init_ui_event()
        self.th = StressThread()
        self.th._signal_msg.connect(self.runThreadCallBack)
        self.th.start()
        self.st = StressTestServerPort()
        self.st.start()
        self.selectTaskid = None

    def closeEvent(self, QCloseEvent):
        if self.ClearThread > 0:
            QCloseEvent.ignore()
            self.hide()
        else:
            self.st.Stop()

    def init_ui_event(self):
        self.pushButton_openRecord.clicked.connect(self.pushButton_openRecord_clicked)
        self.pushButton_addTask.clicked.connect(self.pushButton_addTask_clicked)
        self.tableWidget_Tasks.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tableWidget_Tasks.customContextMenuRequested.connect(self.tableWidget_Tasks_right_menu)
        self.pushButton_addServer.clicked.connect(self.pushButton_addServer_clicked)
        self.pushButton_deleteServer.clicked.connect(self.pushButton_deleteServer_clicked)
        self.tableWidget_recordlists.setContextMenuPolicy(Qt.CustomContextMenu)
        self.tableWidget_recordlists.customContextMenuRequested.connect(self.tableWidget_recordlists_right_clicked)
        self.pushButton_stressClientRun.clicked.connect(self.pushButton_stressClientRun_clicked)

    def init_ui_content(self):
        for key in config['Project']:
            self.comboBox_projecet.addItem(config['Project'].get(key), int(key))
        for key in config['Environment']:
            self.comboBox_env.addItem(config['Environment'].get(key), int(key))

        self.tableWidget_stressClients.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tableWidget_stressClients.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.tableWidget_proxyClients.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tableWidget_proxyClients.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.tableWidget_recordlists.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tableWidget_recordlists.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.tableWidget_Tasks.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.tableWidget_Tasks.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        pass

    def pushButton_stressClientRun_clicked(self):
        if self.pushButton_stressClientRun.text() == '启动本地受控端':
            if self.startClient() == True:
                self.pushButton_stressClientRun.setText('关闭本地受控端')
        else:
            self.stopClient()
            self.pushButton_stressClientRun.setText('启动本地受控端')
        pass

    def startClient(self):
        addr = self.lineEdit_stressClient_addr.text().split(':')
        if len(addr) < 2:
            return False
        ip = addr[0]
        port = addr[1]
        if ip == '' or port =='' or port.isdigit() == False:
            return False
        ret = self.dll.StressClientRun(c_char_p(ip.encode('utf-8')), int(port), int(0))
        if ret == 0:
            self.ClearThread += 1
        else:
            return False
        return True

    def stopClient(self):
        ret = self.dll.StressClientStop()
        self.ClearThread -= 1
        pass

    def pushButton_openRecord_clicked(self):
        word = self.pushButton_openRecord.text()
        if word == '启动录制':
            self.st.StartStressRecord()
            self.ClearThread += 1
            self.pushButton_openRecord.setText('关闭录制')
        else:
            self.st.StopStressRecord()
            self.ClearThread -= 1
            self.pushButton_openRecord.setText('启动录制')

    def pushButton_addServer_clicked(self):
        self.tableWidget_desServer.insertRow(self.tableWidget_desServer.rowCount())
        self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 0, QTableWidgetItem('0.0.0.0'))
        self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 1, QTableWidgetItem('80'))
        self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 2, QTableWidgetItem(''))
        self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 3, QTableWidgetItem(''))
        pass

    def pushButton_deleteServer_clicked(self):
        row = self.tableWidget_desServer.currentRow()
        self.tableWidget_desServer.removeRow(row)
        pass

    def runThreadCallBack(self, code):
        self.refrushProxyClients(ProxyClients)
        self.refrushStressClients(StressClients)
        self.refrushStressTaskConfigs(StressTaskConfigs)
        self.refrushRecords()
        self.refrushTaskMonitor()
        self.label_MsgCount.setText(f'剩余：{len(StressRecordMsg)}')
        # pass

    def refrushTaskMonitor(self):
        row = self.tableWidget_Tasks.currentRow()
        if row < 0:
            return
        taskid = self.tableWidget_Tasks.item(row, 0).text()
        if taskid != self.selectTaskid:
            self.label_errconnect.setText("0")
            self.label_timeout.setText("0")
            self.label_errtotal.setText("0")
        self.selectTaskid = taskid
        task = None
        if f"{taskid}" in StressTasks.keys():
            task = StressTasks[f"{taskid}"]
        if task == None:
            return
        errtotal = 0
        for i in task.Err.keys():
            errobj = task.Err[i]
            errcount = len(errobj.ErrList)
            errtotal += errcount
            if int(i) == 1:
                self.label_errconnect.setText(f"{errcount}")
            if int(i) == 2:
                self.label_timeout.setText(f"{errcount}")
        self.label_errtotal.setText(f"{errtotal}")
        pass

    def refrushProxyClients(self, clients):
        if len(clients) == 0:
            while self.tableWidget_proxyClients.rowCount() > 0:
                self.tableWidget_proxyClients.removeRow(0)
        else:
            while True:
                rowCount = self.tableWidget_proxyClients.rowCount()
                if rowCount == 0:
                    break
                index = 0
                for i in range(rowCount):
                    index = i + 1
                    if self.tableWidget_proxyClients.item(i,0).text() not in clients.keys():
                        self.tableWidget_proxyClients.removeRow(i)
                        break
                if index == rowCount:
                    break
            for client in clients.values():
                if self.proxyClietExiest(client.fd) == False:
                    self.proxyClients_add_row(client)

    def proxyClietExiest(self, id):
        for i in range(self.tableWidget_proxyClients.rowCount()):
            # print(self.tableWidget_proxyClients.item(i, 0).text())
            if id == int(self.tableWidget_proxyClients.item(i, 0).text()):
                return True
        return False

    def proxyClients_add_row(self, client):
        self.tableWidget_proxyClients.insertRow(self.tableWidget_proxyClients.rowCount())
        self.tableWidget_proxyClients.setItem(self.tableWidget_proxyClients.rowCount() - 1, 0, QTableWidgetItem(f'{client.fd}'))
        self.tableWidget_proxyClients.setItem(self.tableWidget_proxyClients.rowCount() - 1, 1, QTableWidgetItem(f'{client.proxyAddr}'))
        self.tableWidget_proxyClients.setItem(self.tableWidget_proxyClients.rowCount() - 1, 2, QTableWidgetItem(f'{client.proxyPort}'))

    def refrushRecords(self):
        recordsList = get_record_message_servers()
        recordsList = self.recordCovertFormat(recordsList)
        # print(recordsList)
        if len(recordsList) == 0:
            while self.tableWidget_recordlists.rowCount() > 0:
                self.tableWidget_recordlists.removeRow(0)
        else:
            while True:
                rowCount = self.tableWidget_recordlists.rowCount()
                if rowCount == 0:
                    break
                index = 0
                for i in range(rowCount):
                    index = i + 1
                    if self.tableWidget_recordlists.item(i,0).text() not in recordsList:
                        self.tableWidget_recordlists.removeRow(i)
                        break
                if index == rowCount:
                    break
            for record in recordsList:
                if self.recordExiest(record) == False:
                    self.record_add_row(record)

    def recordCovertFormat(self, recordslist):
        recordformat = []
        basestr = 't_data_stress_record_'
        for record in recordslist:
            formatend = []
            format1 = record[len(basestr):len(record)]
            format2 = format1.split('_')
            formatend.append(f'{format2[0]}.{format2[1]}.{format2[2]}.{format2[3]}')
            formatend.append(format2[4])
            recordformat.append(formatend)
        return recordformat

    def recordExiest(self, record):
        for i in range(self.tableWidget_recordlists.rowCount()):
            # print(self.tableWidget_proxyClients.item(i, 0).text())
            if record[0] == self.tableWidget_recordlists.item(i, 1).text() and record[1] == self.tableWidget_recordlists.item(i, 2).text():
                return True
        return False

    def record_add_row(self, record):
        self.tableWidget_recordlists.insertRow(self.tableWidget_recordlists.rowCount())
        check = QtWidgets.QTableWidgetItem()
        num = get_record_message_saved(record[0], record[1])
        if num == 0:
            check.setCheckState(QtCore.Qt.Unchecked)
        else:
            check.setCheckState(QtCore.Qt.Checked)
        check.setText('是/否')
        # check.setFlags(Qt_ItemFlag=32)
        self.tableWidget_recordlists.setItem(self.tableWidget_recordlists.rowCount() - 1, 0, check)
        self.tableWidget_recordlists.setItem(self.tableWidget_recordlists.rowCount() - 1, 1, QTableWidgetItem(f'{record[0]}'))
        self.tableWidget_recordlists.setItem(self.tableWidget_recordlists.rowCount() - 1, 2, QTableWidgetItem(f'{record[1]}'))

    def refrushStressClients(self, clients):
        if len(clients) == 0:
            while self.tableWidget_stressClients.rowCount() > 0:
                self.tableWidget_stressClients.removeRow(0)
        else:
            while True:
                rowCount = self.tableWidget_stressClients.rowCount()
                if rowCount == 0:
                    break
                index = 0
                for i in range(rowCount):
                    index = i + 1
                    if self.tableWidget_stressClients.item(i,0).text() not in clients.keys():
                        self.tableWidget_stressClients.removeRow(i)
                        break
                if index == rowCount:
                    break
            for client in clients.values():
                if self.stressClietExiest(client.fd, client) == False:
                    self.stressClients_add_row(client)

    def stressClietExiest(self, id, client):
        for i in range(self.tableWidget_stressClients.rowCount()):
            if id == int(self.tableWidget_stressClients.item(i, 0).text()):
                self.tableWidget_stressClients.item(i, 3).setText(client.StressState)
                return True
        return False

    def stressClients_add_row(self, client):
        self.tableWidget_stressClients.insertRow(self.tableWidget_stressClients.rowCount())
        self.tableWidget_stressClients.setItem(self.tableWidget_stressClients.rowCount() - 1, 0, QTableWidgetItem(f'{client.fd}'))
        self.tableWidget_stressClients.setItem(self.tableWidget_stressClients.rowCount() - 1, 1, QTableWidgetItem(f'{client.PeerAddr}'))
        self.tableWidget_stressClients.setItem(self.tableWidget_stressClients.rowCount() - 1, 2, QTableWidgetItem(f'{client.CpuCount}'))
        self.tableWidget_stressClients.setItem(self.tableWidget_stressClients.rowCount() - 1, 3, QTableWidgetItem(f'{client.StressState}'))

    def refrushStressTaskConfigs(self, configs):
        if len(configs) == 0:
            while self.tableWidget_Tasks.rowCount() > 0:
                self.tableWidget_Tasks.removeRow(0)
        else:
            while True:
                rowCount = self.tableWidget_Tasks.rowCount()
                if rowCount == 0:
                    break
                index = 0
                for i in range(rowCount):
                    index = i + 1
                    if self.tableWidget_Tasks.item(i,0).text() not in configs.keys():
                        self.tableWidget_Tasks.removeRow(i)
                        break
                    else:
                        self.tableWidget_Tasks.item(i, 1).setText(configs[self.tableWidget_Tasks.item(i,0).text()].configState)
                        self.tableWidget_Tasks.item(i, 2).setText(configs[self.tableWidget_Tasks.item(i, 0).text()].description)
                if index == rowCount:
                    break
            for config in configs.values():
                if self.StressTaskConfigsExiest(config.configId) == False:
                    self.StressTaskConfigs_add_row(config)

    def StressTaskConfigsExiest(self, id):
        for i in range(self.tableWidget_Tasks.rowCount()):
            # print(self.tableWidget_proxyClients.item(i, 0).text())
            if id == int(self.tableWidget_Tasks.item(i, 0).text()):
                return True
        return False

    def StressTaskConfigs_add_row(self, config):
        self.tableWidget_Tasks.insertRow(self.tableWidget_Tasks.rowCount())
        self.tableWidget_Tasks.setItem(self.tableWidget_Tasks.rowCount() - 1, 0, QTableWidgetItem(f'{config.configId}'))
        self.tableWidget_Tasks.setItem(self.tableWidget_Tasks.rowCount() - 1, 1, QTableWidgetItem(f'{config.configState}'))
        self.tableWidget_Tasks.setItem(self.tableWidget_Tasks.rowCount() - 1, 2, QTableWidgetItem(f'{config.description}'))

    def pushButton_addTask_clicked(self):
        try:
            projectid = self.comboBox_projecet.currentData()
            projectname = self.comboBox_projecet.currentText()
            envid = self.comboBox_env.currentData()
            total = int(self.lineEdit_totalUsers.text())
            onec = int(self.lineEdit_onceUsers.text())
            space = int(self.lineEdit_spaceTime.text())
            loop = self.comboBox_loopType.currentIndex()
            ignoreErr = self.checkBox_ignoreErr.isChecked()
        except:
            LOG(0, '压力测试框架：任务参数错误')
            return
        # des = [("192.168.2.122", 9500), ("192.168.2.122", 9510)]
        des = []
        rowcount = self.tableWidget_desServer.rowCount()
        for i in range(rowcount):
            row_1 = self.tableWidget_desServer.item(i, 0).text()
            row_2 = self.tableWidget_desServer.item(i, 1).text()
            row_3 = self.tableWidget_desServer.item(i, 2).text()
            row_4 = self.tableWidget_desServer.item(i, 3).text()
            try:
                rowcontent = (row_1, int(row_2), row_3, row_4)
            except:
                LOG(0, '压力测试框架：任务参数错误')
                return
            des.append(rowcontent)
        # print(des)
        task = StressTaskConfig(projectid, projectname, envid, total, onec, space, loop, ignoreErr, StressClients, des)
        StressTaskConfigs[f'{task.configId}'] = task

    def tableWidget_Tasks_right_menu(self, pos):
        # row_num = self.tableWidget.currentRow()
        row_num = -1
        for i in self.tableWidget_Tasks.selectionModel().selection().indexes():
            row_num = i.row()
        if row_num < self.tableWidget_Tasks.rowCount() and row_num >= 0:
            menu = QMenu()
            item1 = menu.addAction(u'开始')
            item2 = menu.addAction(u'增减人数')
            item3 = menu.addAction(u'停止')
            item4 = menu.addAction(u'删除')
            action = menu.exec_(self.tableWidget_Tasks.mapToGlobal(pos))
            configid = self.tableWidget_Tasks.item(row_num, 0).text()
            if action == item1:
                self.st.StartTaskConfig(configid)
            if action == item2:
                count, ret = QInputDialog.getInt(self, '增减人数', '输入正数或负数：',step=10)
                if ret == True:
                    self.st.AddTaskUser(configid, count)
            if action == item3:
                StressTaskConfigs[configid].Running = False
                StressTaskConfigs[configid].configState = '已停止'
            if action == item4:
                StressTaskConfigs[configid].Running = False
                del StressTaskConfigs[configid]

    def tableWidget_recordlists_right_clicked(self, pos):
        row_num = -1
        for i in self.tableWidget_recordlists.selectionModel().selection().indexes():
            row_num = i.row()
        if row_num < self.tableWidget_recordlists.rowCount() and row_num >= 0:
            menu = QMenu()
            item1 = menu.addAction(u'保存')
            item2 = menu.addAction(u'删除')
            item3 = menu.addAction(u'添加到回放')
            action = menu.exec_(self.tableWidget_recordlists.mapToGlobal(pos))
            # configid = self.tableWidget_recordlists.item(row_num, 0).text()
            ip = self.tableWidget_recordlists.item(row_num, 1).text()
            port = self.tableWidget_recordlists.item(row_num, 2).text()
            if action == item1:
                set_record_message_saved(ip, port)
            if action == item2:
                delete_record_message_saved(ip, port)
            if action == item3:
                self.tableWidget_desServer.insertRow(self.tableWidget_desServer.rowCount())
                self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 0, QTableWidgetItem(ip))
                self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 1, QTableWidgetItem(port))
                self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 2, QTableWidgetItem(''))
                self.tableWidget_desServer.setItem(self.tableWidget_desServer.rowCount() - 1, 3, QTableWidgetItem(''))
                pass