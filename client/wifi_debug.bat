set Path=C:\Users\roder\AppData\Local\Android\Sdk\platform-tools;%Path%
adb tcpip 5555

adb shell ifconfig

adb connect 192.168.3.17:5555