from UI.myMainTrayIcon import *
from PyQt5.QtCore import *
from common.runLog import *
import time

class mainFormThread(QThread):
    _logsignal = pyqtSignal(tuple)
    def __init__(self, parent=None):
        self.RUNNING = True
        super(mainFormThread, self).__init__()

    def stop(self):
        self.RUNNING = False

    def run(self):
        while (self.RUNNING):
            time.sleep(0.001)
            if len(ShowMessage) >= 1:
                self._logsignal.emit(ShowMessage[0])
                del ShowMessage[0]

class Form_mainWidget(QWidget):
    def __init__(self, parent=None):
        super(Form_mainWidget, self).__init__(parent)
        self.setWindowFlags(
            Qt.WindowMinimizeButtonHint |
            Qt.WindowMaximizeButtonHint |
            Qt.WindowCloseButtonHint |
            Qt.WindowStaysOnTopHint)
        # self.setWindowIcon(QIcon(QPixmap(":/Res/logo.ico")))
        self.init_UI()
        self.ti = TrayIcon(self)
        self.ti.show()
        self.th = mainFormThread()
        self.th._logsignal.connect(self.runThreadCallBack)
        self.th.start()

    def init_UI(self):
        self.setWindowTitle("日志信息")
        self.resize(877, 303)
        self.textEdit = QTextEdit()
        self.textEdit.setReadOnly(True)
        layout = QVBoxLayout()
        layout.addWidget(self.textEdit)
        self.setLayout(layout)

    def show(self):
        self.setWindowOpacity(0)
        QWidget.show(self)
        desktop = QDesktopWidget()
        self.move(desktop.availableGeometry().width() - self.frameGeometry().width(),
                desktop.availableGeometry().height() - self.frameGeometry().height())
        self.setWindowOpacity(100)

    def runThreadCallBack(self, msg):
        self.textEdit.append(LogDebug(msg[1]))
        if msg[0] == 1:
            self.ti.showMessage("提示", msg[1], icon=0)