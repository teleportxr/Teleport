$ABIs=@('armeabi-v7a', 'arm64-v8a')
$Platform='android-21'

$RootDir     = Join-Path $PSScriptRoot -ChildPath "..\..\..\libavstream"
$IncludeDir  = Join-Path $PSScriptRoot -ChildPath "Include"
$LibsDirBase = Join-Path $PSScriptRoot -ChildPath "Libs\Android"

$ToolchainFile = Join-Path $env:ANDROID_NDK_HOME -ChildPath "build\cmake\android.toolchain.cmake"
$MakeProgram = Join-Path $env:ANDROID_NDK_HOME -ChildPath "prebuilt\windows-x86_64\bin\make.exe"
$BuildType = "RelWithDebInfo"
#$BuildType = "Debug"

New-Item -ItemType Directory -Force -Path $IncludeDir | Out-Null
New-Item -ItemType Directory -Force -Path $LibsDirBase | Out-Null

Remove-Item "$IncludeDir\libavstream" -Recurse -Force
Copy-Item "$RootDir\include\libavstream" -Destination "$IncludeDir\libavstream" -Recurse

Push-Location

foreach($abi in $ABIs) {
    $BuildDirectory = Join-Path $LibsDirBase -ChildPath $abi
    New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
    Set-Location -Path $BuildDirectory

    echo "Building for $abi ..."
    cmake -G"MinGW Makefiles" -DCMAKE_TOOLCHAIN_FILE="$ToolchainFile" -DCMAKE_MAKE_PROGRAM="$MakeProgram" -DCMAKE_BUILD_TYPE="$BuildType" -DANDROID_PLATFORM="$Platform" -DANDROID_ABI="$abi" "$RootDir"
    & "$MakeProgram"
}

Pop-Location
