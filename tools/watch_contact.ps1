$ErrorActionPreference = 'Stop'
$subscriber = Join-Path $env:LOCALAPPDATA 'ESP32-ZNP-Lab/mosquitto/mosquitto_sub.exe'
Write-Output 'Open/close UZ-6D. contact=true means closed; false means open. Ctrl+C stops watching.'
& $subscriber -h 127.0.0.1 -p 1883 -v -t esp32_znp_lab/UZ-6D_Door
exit $LASTEXITCODE
