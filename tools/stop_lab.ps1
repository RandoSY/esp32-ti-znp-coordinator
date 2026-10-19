$ErrorActionPreference = 'Stop'
$labRoot = Split-Path $PSScriptRoot -Parent
$statePath = Join-Path $labRoot 'lab-zigbee2mqtt/lab-process.json'
if (!(Test-Path -LiteralPath $statePath)) { Write-Output 'No managed lab process is running.'; exit 0 }
$labState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
$process = Get-CimInstance Win32_Process -Filter ('ProcessId=' + [int]$labState.pid)
if (!$process -or $process.CommandLine -notlike '*lab_host.cjs*') { Write-Output 'Saved lab process is no longer running.'; exit 0 }
Set-Content -LiteralPath (Join-Path $labRoot 'lab-zigbee2mqtt/stop.request') -Value 'stop' -Encoding ascii
for ($attempt=0; $attempt -lt 60; $attempt++) {
    if (!(Get-Process -Id ([int]$labState.pid) -ErrorAction SilentlyContinue)) { Write-Output 'Lab stopped cleanly; COM17 is available for Python.'; exit 0 }
    Start-Sleep -Milliseconds 500
}
throw 'Graceful shutdown is still pending. Check artifacts/lab-host logs before opening COM17.'
