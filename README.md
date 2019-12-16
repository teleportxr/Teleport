# RemotePlay

## Getting the source

Clone the repository with submodules:

    git clone --recurse-submodules git@github.com:simul/RemotePlay.git

## Prerequisites

1. Visual Studio 2017 (with Visual C++ tools for CMake)
2. Android Studio
3. Unreal Engine 4.22 incorporating the patch to move SceneCaptureSource from USceneCaptureComponent2D to USceneCaptureComponent
4. NVIDIA CUDA Toolkit 10.
5. NVIDIA Video Codec SDK
6. Recent CMake, and get ninja.exe and put it in C:\Program Files\CMake\bin
7. edit local.properties to contain cmake.dir=C\:\\Program Files\\CMake

## Building the PC Client

1. Using CMakeGUI, set src: (RemotePlay Folder) and bin: (RemotePlay Folder)/build/x64
2. Set Simul Directory and uncheck BUILD_SHARED_LIBS and USE_DYNAMIC_RUNTIME.
3. In the Advanced CMake config settings, search for CXX_FLAGS and ensure that the configurations use the /MT and /MTd runtimes. Also Check "STATIC" - option from Basis Universal.
4. set BUILD_AS_LIBRARY to checked - option from Basis Universal module.
5. Check out libavstream submodule to master (if needed)
6. Configure for Visual Studio 15 2017 with x64 with default native compiler
7. Generate, open and build the visual studio project

## Building UE4 plugin

1. Using CMakeGUI, set src: (RemotePlay Folder) and bin: (RemotePlay Folder)/plugins/UnrealDemo/Plugins/RemotePlay/Libraries. In the Advanced config settings, ensure LIBAV_USE_DYNAMIC_RUNTIME is checked: Unreal uses the dynamic runtimes so this is needed for compatibility. Make sure REMOTEPLAY_SERVER is checked: this removes the client and test projects from the build.
2. Add the created projects to the solution at plugins/UnrealDemo/UnrealDemo.sln. Make sure that the release build of libavstream is configured to compile in Development Editor solution config.
3. Build libavstream, this creates libavstream.lib inplugins\UnrealDemo\Plugins\RemotePlay\Libraries\libavstream\lib\(CONFIG).
4. Repeat steps 1-3 for thirdparty/enet and thirdparty/basis_universal. For Basis, you can just set STATIC to unchecked, this will make it use the dynamic runtimes. Ensure BUILD_AS_LIBRARY is checked for Basis.
5. Right-click UnrealDemo.uproject and select Generate Visual Studio project files and then Switch Unreal Engine version to Simul's private 4.22 branch. Open and build the UE4 project in `Development Editor` configuration.
6. Go to Edit->Editor Preferences, General->Performance and disable "Use Less CPU When in Background". This is to prevent UE switching to a slow low-power mode when the Editor window is not in focus.
7. Put r.ShaderDevelopmentMode=1 in your UE4 directory\Engine\Config\ConsoleVariables.ini
8. (OPTIONAL) Package the project for `Windows 64-bit` platform. This is recommended for best performance during testing.

## Building Android client application

1. Follow [Oculus Mobile SDK software setup guide](https://developer.oculus.com/documentation/mobilesdk/latest/concepts/mobile-studio-setup-android/).
2. [Generate an osig file](https://dashboard.oculus.com/tools/osig-generator/) for your device and place it in `client/VrProjects/Native/RemotePlayClient/assets` directory.
3. Connect your Android device to your PC and make sure that ADB debugging is enabled and the PC is authorized.
4. (old method) Go to `client/VrProjects/Native/RemotePlayClient/Projects/Android` and run `build.bat` to build and deploy.
5. New method: In Android Studio open RemotePlay/build.gradle.
6. On the top right, click the Sync Project with Gradle Files button (has a shark icon) to load the config for your device.
7. In Android Studio, go to Build->Generate Signed Bundle / APK and generate a key named android.debug.keystore in your RemotePlay/client/SimulCasterClient directory.
8. Go to File->Project Structure->Modules->SimulCasterClient->Signing Configs and in debug config, put in the details of the key you created. 
9. Click the build icon to compile and then you should be able to run.

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
