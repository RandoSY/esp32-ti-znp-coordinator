@echo off
powershell.exe -NoProfile -File "%~dp0tools\watch_contact.ps1"
if errorlevel 1 pause
