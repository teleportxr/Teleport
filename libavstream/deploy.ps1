<#
.PARAMETER Config
    Build configuration to deploy.
.PARAMETER Project
    Destination project.
#>
param(
    [String]$Config = "Release",

    [String]$Project="UnrealPlugin"
)

$Root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$ProjectLocation = Join-Path $Root -ChildPath 'plugins' | Join-Path -ChildPath $Project
$Destination = Join-Path $ProjectLocation -ChildPath 'Plugins/RemotePlay/Libraries/libavstream'

$SrcBinaries = Join-Path $Root -ChildPath 'libavstream/build' | Join-Path -ChildPath $Config
$SrcInclude  = Join-Path $Root -ChildPath 'libavstream/include/libavstream'

$DstBinaries = Join-Path $Destination 'Win64'
$DstInclude  = Join-Path $Destination 'Include'

echo "Deploying to $ProjectLocation"

if(Test-Path $DstBinaries) {
    Remove-Item $DstBinaries -Force -Recurse
}
if(Test-Path $DstInclude) {
    Remove-Item $DstInclude -Force -Recurse
}

New-Item -ItemType Directory -Force -Path $Destination | Out-Null
New-Item -ItemType Directory -Force -Path $DstBinaries | Out-Null
New-Item -ItemType Directory -Force -Path $DstInclude  | Out-Null

Copy-Item (Join-Path $SrcBinaries 'libavstream.dll') $DstBinaries
Copy-Item (Join-Path $SrcBinaries 'libavstream.lib') $DstBinaries
Copy-Item -Recurse $SrcInclude $DstInclude
