@echo off
cd /d "%~dp0"
powershell -ExecutionPolicy Bypass -File scripts\windows_run_client.ps1
