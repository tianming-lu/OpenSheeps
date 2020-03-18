#
#   @author:lutianming,create date:2018-06-15, QQ641471957
#
from UI.myMainQWidget import *
from UI.myStressTestServerPortWidget import *
from common.runLog import *
import sys
from Res.res import *

subForm = {'StressServer': Form_StressTestServerPort}


class RunThread(QThread):
    _showsignal = pyqtSignal(tuple)
    _appexitsignal = pyqtSignal()
    def __init__(self, parent=None):
        self.RUNNING = True
        super(RunThread, self).__init__(parent=parent)

    def stop(self):
        self.RUNNING = False

    def run(self):
        while (self.RUNNING):
            time.sleep(0.05)
            if len(ShowForm) >= 1:
                self._showsignal.emit(ShowForm[0])
                del ShowForm[0]
            if get_AppExit() == True:
                self._appexitsignal.emit()

def AppExit():
    os._exit(0)

def showWindow(msg):
    try:
        stats = AllForm[msg[0]].isVisible()
        if stats == False or True:
            if msg[1] == 0:
                AllForm[msg[0]].hide()
            else:
                AllForm[msg[0]].show()
        return
    except:
        pass

    try:
        if msg[1] == 0:
            AllForm[msg[0]].close()
        else:
            AllForm[msg[0]] = subForm[msg[0]]()
            AllForm[msg[0]].show()
    except:
        LOG(1, traceback.format_exc())

if __name__ == "__main__":
    InitRunLog()
    app = QApplication(sys.argv)
    app.setWindowIcon(QIcon(QPixmap(":/Res/logo.ico")))
    app.setQuitOnLastWindowClosed(False)
    AllForm['Main'] = Form_mainWidget()
    AllForm['StressServer'] = Form_StressTestServerPort()
    AllForm['StressServer'].show()

    th = RunThread()
    th._showsignal.connect(showWindow)
    th._appexitsignal.connect(AppExit)
    th.start()

    LOG(1,"程序启动")
    sys.exit(app.exec_())