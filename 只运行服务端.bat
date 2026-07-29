@echo off
cd /d "%~dp0"
powershell -ExecutionPolicy Bypass -File scripts\windows_run_server.ps1
pause
