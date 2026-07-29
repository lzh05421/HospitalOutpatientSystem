$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$running = Get-Process hospital_server,hospital_client -ErrorAction SilentlyContinue
if ($running) {
    try {
        $running | Stop-Process -Force -ErrorAction Stop
        Start-Sleep -Milliseconds 500
    } catch {
        Write-Host "无法自动关闭 hospital_server/hospital_client。" -ForegroundColor Yellow
        Write-Host "请先关闭医院系统窗口，或在任务管理器中结束 hospital_server.exe 和 hospital_client.exe，然后重新运行本脚本。" -ForegroundColor Yellow
        exit 1
    }
}
& "D:\Qt\Tools\CMake_64\bin\cmake.exe" --build "$ProjectRoot\build-codex" -j
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
