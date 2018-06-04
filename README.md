# RemotePlay

## Getting the source

Clone the repository with submodules:

    git clone --recurse-submodules https://github.com/simul/RemotePlay.git

## Prerequisites

1. Visual Studio 2017 (with Visual C++ tools for CMake)
2. Android Studio
3. Unreal Engine 4.19 (launcher version)
	
## Building UE4 plugin

1. Build `libavstream` in `x64-Release` configuration, either with Visual Studio or using standalone CMake. The resulting binaries should end up in `libavstream/build/x64-Release` directory.
2. Deploy `libavstream` to `UnrealPlugin` project by running `deploy.ps1` script with PowerShell:
`.\deploy.ps1 -Config x64-Release -Project UnrealPlugin`
3. Build `UnrealPlugin` UE4 project in `Development Editor` configuration.

## Building GearVR client application

1. Follow [Oculus Mobile SDK software setup guide](https://developer.oculus.com/documentation/mobilesdk/latest/concepts/mobile-studio-setup-android/).
2. [Generate an osig file](https://dashboard.oculus.com/tools/osig-generator/) for your device and place it in `client/VrProjects/Native/RemotePlayClient/assets` directory.
3. Connect your Android device to your PC and make sure that ADB debugging is enabled and the PC is authorized.
4. Go to `client/VrProjects/Native/RemotePlayClient/Projects/Android` and run `build.bat` to build and deploy.

## Running

1. Connect your Android device to the same WiFi network your development PC is on.
2. Set a known, static IP address on your Android device. Make sure you can ping it from your PC.
3. Open `UnrealPlugin.uproject` in the UE4 editor and then open `Content/FirstPersonCPP/Blueprints/FirstPersonCharacter` blueprint.
4. Select `RemotePlayCapture` component and set `RemoteIP` property to the IP address of the Android device.
5. Make sure UE4 editor is not blocked by the Windows firewall.
6. Run the game in the editor and then launch `RemotePlayClient` application on your Android device.
