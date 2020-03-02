@echo off
echo               ===== 打包程序 =====

echo.
echo 第一步：打包exe
pyinstaller -w -F ./main.py -i ./res/logo.ico

echo.
echo 第二步：复制运行文件

echo.
echo 第三步：删除临时目录
rmdir /S/Q build
rmdir /S/Q __pycache__
del /Q *.spec
echo 打包完成，请查看dist目录
echo.
pause