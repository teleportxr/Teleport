<#
.PARAMETER Config
    Build configuration to use.
.PARAMETER Shader
    Name of HLSL shader file to use by the server process.
#>
param(
    [String]$Config = "x64-Debug",
    [String]$Shader = "rotate.hlsl"
)

$BuildPath = Join-Path "build" -ChildPath $Config | Resolve-Path
$ShaderPath = Join-Path "tests\test_server\shaders\hlsl" -ChildPath $Shader

$Env_OldPath = $env:Path
$env:Path = "$env:Path;$BuildPath;"

Start-Process -FilePath "$BuildPath\tests\test_client\test_client.exe"
Start-Sleep -Seconds 2
Start-Process -FilePath "$BuildPath\tests\test_server\test_server.exe" -ArgumentList "$ShaderPath"

$env:Path = $Env_OldPath
