# RemotePlay

## Getting the source

Clone the repository with submodules:

    git clone --recurse-submodules https://github.com/simul/RemotePlay.git

## Prerequisites

1. Visual Studio 2017 (with Visual C++ tools for CMake)
2. Android Studio
3. Unreal Engine 4.20 (launcher version)
4. NVIDIA CUDA Toolkit
	
## Building UE4 plugin

1. Build `libavstream` in `x64-Release` configuration, either with Visual Studio or using standalone CMake. The resulting binaries should end up in `libavstream/build/x64-Release` directory.
2. Deploy `libavstream` to `UnrealPlugin` project by running `deploy.ps1` script with PowerShell:
`.\deploy.ps1 -Config x64-Release -Project UnrealPlugin`
3. Build `UnrealPlugin` UE4 project in `Development Editor` configuration.

## Building UE4 demo project

This is ShooterGame project modified to work with RemotePlay.

1. Make sure you have built the UE4 plugin project first.
2. Copy `plugins/UnrealPlugin/Plugins/RemotePlay` to `plugins/UnrealDemo/Plugins/RemotePlay`. Delete `Binaries` & `Intermediate` directories if present.
3. Build `UnrealDemo` UE4 project in `Development Editor` configuration.
4. Package the project for `Windows 64-bit` platform. This is recommended for best performance during testing.

## Building GearVR client application

1. Follow [Oculus Mobile SDK software setup guide](https://developer.oculus.com/documentation/mobilesdk/latest/concepts/mobile-studio-setup-android/).
2. [Generate an osig file](https://dashboard.oculus.com/tools/osig-generator/) for your device and place it in `client/VrProjects/Native/RemotePlayClient/assets` directory.
3. Connect your Android device to your PC and make sure that ADB debugging is enabled and the PC is authorized.
4. Go to `client/VrProjects/Native/RemotePlayClient/Projects/Android` and run `build.bat` to build and deploy.

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
