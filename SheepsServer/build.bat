@echo off
echo               ===== ������� =====

echo.
echo ��һ�������exe
pyinstaller -w -F ./main.py -i ./res/logo.ico

echo.
echo �ڶ��������������ļ�

echo.
echo ��������ɾ����ʱĿ¼
rmdir /S/Q build
rmdir /S/Q __pycache__
del /Q *.spec
echo �����ɣ���鿴distĿ¼
echo.
pause