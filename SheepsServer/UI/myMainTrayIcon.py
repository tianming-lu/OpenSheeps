from PyQt5.QtWidgets import *
from PyQt5.QtGui import *
from common.gdata import *


class TrayIcon(QSystemTrayIcon):
    def __init__(self, papa):
        super(TrayIcon, self).__init__(parent=papa)
        self.setToolTip("Sheeps服务器压力测试")
        self.setIcon(QIcon(QPixmap(":/Res/logo.ico")))
        self.initMenu()
        self.initUIEvent()

    def initServerStressTestMenu(self):
        self.serverStressManager = QAction("主界面", self, triggered=lambda :self.iconClicked(2))
        self.logoActon = QAction("日志", self, triggered=self.logoClicked)
        self.quitAction = QAction("退出", self, triggered=self.Appquit)


    def initMenu(self):
        self.menu = QMenu()
        self.initServerStressTestMenu()
        self.menu.addAction(self.serverStressManager)
        self.menu.addSeparator()
        self.menu.addAction(self.logoActon)
        self.menu.addAction(self.quitAction)
        self.setContextMenu(self.menu)

    def initUIEvent(self):
        self.activated.connect(self.iconClicked)

    def logoClicked(self):
        win = AllForm['Main']
        if win.isVisible():
            win.hide()
        else:
            win.show()

    def iconClicked(self, reason):
        if reason == 2 or reason == 3:
            win = AllForm['StressServer']
            if win.isVisible():
                win.hide()
            else:
                win.show()

    def msgClicked(self):
        self.showMessage("提示", "请稍后……", icon=0)

    def Appquit(self):
        self.setVisible(False)
        set_AppExit(True)