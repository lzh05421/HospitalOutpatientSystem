@echo off
cd /d "%~dp0"
powershell -ExecutionPolicy Bypass -File scripts\windows_configure.ps1
if errorlevel 1 pause & exit /b 1
powershell -ExecutionPolicy Bypass -File scripts\windows_build.ps1
if errorlevel 1 pause & exit /b 1
powershell -ExecutionPolicy Bypass -File scripts\windows_run_all.ps1
if errorlevel 1 pause & exit /b 1
