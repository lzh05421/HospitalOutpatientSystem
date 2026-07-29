$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot

Get-Process hospital_server,hospital_client -ErrorAction SilentlyContinue | Stop-Process -Force

$serverOut = Join-Path $ProjectRoot "server.out.log"
$serverErr = Join-Path $ProjectRoot "server.err.log"
Remove-Item $serverOut,$serverErr -ErrorAction SilentlyContinue

$env:Path = "D:\Qt\6.11.0\mingw_64\bin;D:\Qt\Tools\mingw1310_64\bin;$env:Path"
& "$ProjectRoot\build-codex\launcher\hospital_launcher.exe"
