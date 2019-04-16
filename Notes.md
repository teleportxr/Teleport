1. libavstream when initialized has an https git address, this should be modified to ssh.
2. Build errors with VS 2017 for libavstream. Upgrading from Cmake 3.9 to 3.11 fixed these.
3. By default, if the build directory is libavstream/build, binaries go to build/Release/ not build/x64-Release
4. We have a project named UnrealPlugin, which isn't a plugin, but which contains a plugin called RemotePlay...? This seems confusing.
5. We have a project called UnrealDemo in the plugins folder, which isn't a plugin, but a demo.
6. On loading UnrealDemo, I have Load Errors in the message log:
/Game/Blueprints/Pawns/PlayerPawn : Can't find file for asset. /Script/RemotePlay
Failed to load /Script/RemotePlay.RemotePlayCaptureComponent Referenced by SCS_Node_0
/Game/Blueprints/PlayerController : Can't find file for asset. /Script/RemotePlay
Failed to load /Script/RemotePlay.RemotePlaySessionComponent Referenced by SCS_Node_1
Opening the UnrealDemo.sln, I have messages:
7. C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\Common7\IDE\VC\VCTargets\Microsoft.Cpp.props(31,3): warning MSB4011: "C:\Program Files (x86)\Microsoft Visual Studio\2017\Professional\Common7\IDE\VC\VCTargets\Microsoft.Makefile.props" cannot be imported again. 