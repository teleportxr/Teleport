# RemotePlay

## Getting the source

Clone the repository with submodules:

    git clone --recurse-submodules git@github.com:simul/RemotePlay.git

## Prerequisites

1. Visual Studio 2019 (with Visual C++ tools for CMake)
2. Android Studio
3. Unreal Engine 4.22 incorporating the patch to move SceneCaptureSource from USceneCaptureComponent2D to USceneCaptureComponent
4. NVIDIA CUDA Toolkit 10.
5. NVIDIA Video Codec SDK
6. Recent CMake, and get ninja.exe and put it in C:\Program Files\CMake\bin
7. Edit local.properties to contain cmake.dir=C\:\\Program Files\\CMake
8. Ubuntu on Windows
8. Nasm, to build OpenSSL. Get it from https://www.nasm.us/.

## Building the PC Client

1. Build pthread.2015.sln in "\thirdparty\srt\submodules\pthread-win32" in Release x64.
2. Check out libavstream submodule to master (if needed)
3. Using CMakeGUI, set src: (RemotePlay Folder) and bin: (RemotePlay Folder)/build/x64
4. Configure for x64 platform with default native compiler
5. In the Advanced CMake config settings, search for CXX_FLAGS and ensure that the configurations use the /MT and /MTd runtimes.
6. Uncheck 'BUILD_SHARED_LIBS', and 'USE_DYNAMIC_RUNTIME'.
7. Uncheck 'LIBAV_BUILD_SHARED_LIBS', and 'LIBAV_USE_DYNAMIC_RUNTIME'.
8. Uncheck 'ENABLE_ENCRYPTION' option from srt.
9. Check PLATFORM_D3D11_SFX.
10. Generate, open and build the Visual Studio project.

## Building UE4 plugin

1. Build pthread.2015.sln in "\thirdparty\srt\submodules\pthread-win32" in Release x64.
    * You will need to target pthread_lib to the same toolset as your Unreal.
    * You may need to disable Whole Program Optimisation.
2. Using CMakeGUI: 
    * Set src: (RemotePlay Folder) and bin: (RemotePlay Folder)/plugins/UnrealDemo/Plugins/RemotePlay/Libraries.
    * Your platform, and toolset, **must** match your Unreal Engine configuration.
    * Enable **Advanced** view in CMake-GUI, if you can't find any of the following settings. 
    * Ensure LIBAV_USE_DYNAMIC_RUNTIME is checked: Unreal uses the dynamic runtimes so this is needed for compatibility.
    * Make sure REMOTEPLAY_SERVER is checked: this removes the client and test projects from the build.
    * For Basis, you can just set STATIC to unchecked, this will make it use the dynamic runtimes.
    * Ensure BUILD_AS_LIBRARY is checked for Basis.
    * Uncheck ENABLE_ENCRYPTION.
    * Remove RelWithDebInfo and MinSizeRelease configurations.
3. Right-click UnrealDemo.uproject and select Generate Visual Studio project files and then Switch Unreal Engine version to Simul's private 4.22 branch.
4. Add the created projects to the solution at plugins/UnrealDemo/UnrealDemo.sln. Make sure that the release build of libavstream is configured to compile in Development Editor solution config. The projects needed are:
    * basisu
	* efp
	* enet
	* haicrypt_virtual
	* libavstream
    * SimulCasterServer
    * srt_static
    * srt_virtual

5. Ensure cuda_kernels project in libavstream solution is at least toolset Visual Studio 2019.
6. Build the projects, this creates static libraries for UnrealDemo to link.
7. Open and build the UE4 project in `Development Editor` configuration.
8. Go to Edit->Editor Preferences, General->Performance and disable "Use Less CPU When in Background". This is to prevent UE switching to a slow low-power mode when the Editor window is not in focus.
9. Put r.ShaderDevelopmentMode=1 in your UE4 directory\Engine\Config\ConsoleVariables.ini
10. (OPTIONAL) Package the project for `Windows 64-bit` platform. This is recommended for best performance during testing.

## Building Unity plugin
1. In Cmake GUI, set REMOTEPLAY_UNITY to checked.

## Building Android client application

1. Follow [Oculus Mobile SDK software setup guide](https://developer.oculus.com/documentation/mobilesdk/latest/concepts/mobile-studio-setup-android/).
2. [Generate an osig file](https://dashboard.oculus.com/tools/osig-generator/) for your device and place it in `client\SimulCasterClient\assets` directory.
3. Connect your Android device to your PC and make sure that ADB debugging is enabled and the PC is authorized.
4. (old method) Go to `client/VrProjects/Native/RemotePlayClient/Projects/Android` and run `build.bat` to build and deploy.
5. New method: In Android Studio open RemotePlay/build.gradle.
6. On the top right, click the Sync Project with Gradle Files button (has a shark icon) to load the config for your device.
7. In Android Studio, go to Build->Generate Signed Bundle / APK and generate a key named android.debug.keystore in your RemotePlay/client/SimulCasterClient directory.
8. Go to File->Project Structure->Modules->client->SimulCasterClient->Signing Configs and in debug config, put in the details of the key you created. 
9. Click the build icon to compile and then you should be able to run.

## Firewall setup
1. Go to Windows Security->Firewall & Network->Advanced Settings.
2. Choose Inbound Rules->New Rule->Port->UDP.
3. Enter the Discovery Port and create the rule.
Repeat 2-3 for the Service Port.

## Running

1. Connect your Android device to the same WiFi network your development PC is on.
2. Make sure UE4 editor is not blocked by the Windows firewall.
3. Make sure the GearVR controller is paired with the Android device.
4. Run the game in the editor and then launch `RemotePlayClient` application on your Android device.

It may take up to a few seconds for GearVR controller to be recognized.

For best performance when testing with UE4 demo project run the packaged game in windowed mode, in 1024x768 resolution, with Low quality settings.

### Controls

| Control | Action |
|--|--|
| Swipe on trackpad up/down | Move forwards/backwards |
| Swipe on trackpad left/right | Strafe left/right |
| Click on trackpad | Jump |
| Trigger | Fire |

### Default network ports

| Protocol | Port  | Description |
| ---------|-------|-------------|
| UDP      | 10500 | Session control & player input
| UDP      | 10501 | Video stream
| UDP      | 10600 | Local network server discovery


## Building srt for Android
This is only necessary if we change NDK or modify srt in some way. Set up Ubuntu Linux subsystem for Windows, then from a bash shell:

    cd ~
    sudo cp -r /mnt/c/Users/[Username]/.ssh .
    apt-get install git
    sudo apt-get install git
    sudo apt-get update
    eval $(ssh-agent -s)
    ssh-add ~/.ssh/id_rsa

    mkdir SRT
    cd SRT
    git clone git@github.com:simul/srt.git
    apt-get install cmake
    sudo apt-get install cmake
    sudo apt-get install tclsh
    sudo apt install unzip
    sudo apt-get install zip
    sudo apt-get install patchelf
    sudo apt install gcc
    sudo apt install make
    sudo apt install git
    sudo apt install clang
    sudo apt install g++
    sudo apt install gcc

    chmod +x mkall
    chmod +x mkssl
    chmod +x mksrt
    chmod +x prepare_build 
    chmod +x packjni 

    unzip android-ndk-r20b-linux-x86_64.zip

    ./mkall > log.txt
    ./packjni

    zip -r arm64-v8a.zip arm64-v8a
    zip -r armeabi-v7a.zip armeabi-v7a
    zip -r x86.zip x86
    zip -r x86_64.zip x86_64

## Troubleshooting
1. If you can receive packets from the headset, but can't transmit to it, it may have an IP address conflict. Check no other device has the same IP.