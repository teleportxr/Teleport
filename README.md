# Teleport VR

Teleport VR is an open, native network protocol for virtual and augmented reality.
This repository contains the reference Client/Server software and SDK for Teleport VR.
Comments, bug reports and pull requests are welcome.

## Getting the source

Clone the repository with submodules:

    git clone --recurse-submodules git@github.com:simul/Teleport.git

or if you have already cloned the main repo,

    git submodule update --init --recursive

## Prerequisites

1. Visual Studio 2019 or later (with Visual C++ tools for CMake)
2. Android Studio
3. Unreal Engine 4.22 incorporating the patch to move SceneCaptureSource from USceneCaptureComponent2D to USceneCaptureComponent
4. NVIDIA CUDA Toolkit 11 https://developer.nvidia.com/cuda-downloads?target_os=Windows&target_arch=x86_64.
5. NVIDIA Video Codec SDK https://developer.nvidia.com/nvidia-video-codec-sdk
6. Recent CMake. Edit local.properties to contain cmake.dir=C\:\\Program Files\\CMake
7. ninja.exe: put it in C:\Program Files\CMake\bin
8. Ubuntu on Windows
9. Nasm, to build OpenSSL. Get it from https://www.nasm.us/.
10. OpenXR, for the client. Get it from https://github.com/KhronosGroup/OpenXR-SDK.

## Building the PC Client

1. Build pthread.2019.sln in "\thirdparty\srt\submodules\pthread-win32\windows\VS2019" in Release x64.
	* You may retarget the projects to a more recent version of the build tools.
2. In firstparty/Platform, run Setup.py to build required libraries fmt and glfw.
3. Using CMakeGUI:
    * Set source code location to (Teleport Folder) and build the binaries at (Teleport Folder)/build_pc_client
    * Configure for x64 platform with default native compiler
    * In the Advanced CMake config settings, search for CXX_FLAGS and ensure that the configurations use the /MT and /MTd runtimes.
    * Uncheck 'BUILD_SHARED_LIBS', and 'USE_DYNAMIC_RUNTIME'.
    * Uncheck 'LIBAV_BUILD_SHARED_LIBS', and 'LIBAV_USE_DYNAMIC_RUNTIME'.
    * Uncheck 'ENABLE_ENCRYPTION' option from srt.
    * Set CMAKE_CUDA_COMPILER, LIBAV_CUDA_DIR and LIBAV_CUDA_SAMPLES_DIR to your installed Cuda version
4. Configure, generate, open and build the Visual Studio project *in Release Configuration first*.

## Building Unity plugin

The Unity server plugin is currently the main testbed for Teleport servers.

1. Using CMakeGUI:
	* Build to a subfolder of your Unity project, e.g. Project/teleport_build. *Don't* build inside the Assets folder.
	* Check TELEPORT_UNITY.
	* Check TELEPORT_SERVER.
	* Uncheck LIBAV_USE_DYNAMIC_RUNTIME.
	* Uncheck USE_DYNAMIC_RUNTIME.
	* Click 'Configure' solution button.
	* Set TELEPORT_UNITY_EDITOR_DIR to "Editor" sub-directory where Unity is installed; e.g. "C:/Program Files/Unity/2019.4.15f1/Editor".
	* Click 'Generate' solution button.
2. Open Project, and set to "Release" build mode.
3. Set TeleportServer as startup project.
	* Right-click TeleportServer project in 'Solution Explorer' pane.
	* Click "Set as Startup Project".

## Building the Android Vulkan OpenXR Client with Visual Studio

1. Build the PC client as above.
2. Use Android Studio to install the appropriate NDK (see release.properties).
3. Install the appropriate JDK from [Java Archive](https://www.oracle.com/java/technologies/javase/jdk14-archive-downloads.html) (see release.properties) and set JAVA_HOME
4. Install the current stable [AGDE](https://developer.android.com/games/agde#downloads)
5. Open the sln in build_android_vs in Visual Studio.
6. Go to Tools -> Settings -> Cross Platform -> C++ -> Android
7. Build and run.

## Building UE4 plugin

The UE4 plugin is not currently functional, it will be updated in mid-2022.

1. Build pthread.2015.sln in "\thirdparty\srt\submodules\pthread-win32" in Release x64.
    * You will need to target pthread_lib to the same toolset as your Unreal.
    * You may need to disable Whole Program Optimisation.
2. Using CMakeGUI: 
    * Set src: (Teleport Folder) and bin: (Teleport Folder)/plugins/UnrealDemo/Plugins/RemotePlay/Libraries.
    * Your platform, and toolset, **must** match your Unreal Engine configuration.
    * Enable **Advanced** view in CMake-GUI, if you can't find any of the following settings. 
    * Ensure LIBAV_USE_DYNAMIC_RUNTIME is checked: Unreal uses the dynamic runtimes so this is needed for compatibility.
    * Make sure TELEPORT_SERVER is checked: this removes the client and test projects from the build.
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
    * TeleportServer
    * srt_static
    * srt_virtual

5. Ensure cuda_kernels project in libavstream solution is at least toolset Visual Studio 2019.
6. Build the projects, this creates static libraries for UnrealDemo to link.
7. Open and build the UE4 project in `Development Editor` configuration.
8. Go to Edit->Editor Preferences, General->Performance and disable "Use Less CPU When in Background". This is to prevent UE switching to a slow low-power mode when the Editor window is not in focus.
9. Put r.ShaderDevelopmentMode=1 in your UE4 directory\Engine\Config\ConsoleVariables.ini
10. (OPTIONAL) Package the project for `Windows 64-bit` platform. This is recommended for best performance during testing.

## Firewall setup
1. Go to Windows Security->Firewall & Network->Advanced Settings.
2. Choose Inbound Rules->New Rule->Port->UDP.
3. Enter the Discovery Port and create the rule.
Repeat 2-3 for the the Service Port.

## Running

1. Connect your Android device to your local WiFi network (for a local server) or the internet (for a remote server).
2. On the server machine, make sure Unity or UE4 editor is not blocked by the Windows firewall.
3. Find the IP address of your server, either a local IP or a global IP or domain name.
4. Run the game in UE or Unity editor and then launch the client application on your Android or PCVR device.

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
