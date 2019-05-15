set Path=C:\Users\roder\AppData\Local\Android\Sdk\platform-tools;%Path%
adb kill-server
adb tcpip 5555
pause
adb ifconfig
adb connect 192.168.3.17:5555
pause