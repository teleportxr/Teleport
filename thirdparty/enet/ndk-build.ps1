$TOOLCHAIN="$Env:ANDROID_NDK_HOME\build\cmake\android.toolchain.cmake"
$MAKE="$Env:ANDROID_NDK_HOME\prebuilt\windows-x86_64\bin\make.exe"

New-Item -ItemType Directory -Force ndk-build | Out-Null
cd ndk-build

foreach($ABI in "armeabi-v7a","arm64-v8a") {
    New-Item -ItemType Directory -Force $ABI | Out-Null
    cd $ABI
    cmake -G"MinGW Makefiles" `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" `
        -DCMAKE_MAKE_PROGRAM="$MAKE" `
        -DANDROID_ABI="$ABI" `
        ..\..
    Invoke-Expression "$MAKE all"
    cd ..
}

cd ..
