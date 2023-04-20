FOR /F "tokens=1,2 delims==" %%A IN (keystore.properties) DO (
    set %%A=%%B 
)
%USERPROFILE%\AppData\Local\Android\Sdk\build-tools\32.1.0-rc1\apksigner sign --ks %RELEASE_STORE_FILE% --ks-key-alias %RELEASE_KEY_ALIAS% --ks-pass pass:%RELEASE_STORE_PASSWORD% --key-pass pass:%RELEASE_KEY_PASSWORD% --out TeleportVRQuestClient-Release.apk ..\..\bin\Android-arm64-v8a\Release\TeleportVRQuestClientAGDE-Release.apk