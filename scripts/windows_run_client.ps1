$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$env:Path = "D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;$env:Path"
Set-Location $ProjectRoot
& "$ProjectRoot\build-codex\client\hospital_client.exe"
