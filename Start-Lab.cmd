@echo off
powershell.exe -NoProfile -File "%~dp0tools\start_lab.ps1"
if errorlevel 1 pause
