$ErrorActionPreference = 'Stop'
$labRoot = Split-Path $PSScriptRoot -Parent
$statePath = Join-Path $labRoot 'lab-zigbee2mqtt/lab-process.json'
if (Test-Path -LiteralPath $statePath) {
    $labState = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    $existing = Get-CimInstance Win32_Process -Filter ('ProcessId=' + [int]$labState.pid)
    if ($existing -and $existing.CommandLine -like '*lab_host.cjs*') {
        Write-Output 'Lab is already running: http://127.0.0.1:8080'
        exit 0
    }
}
$node = (Get-Command node -ErrorAction Stop).Source
$entry = Join-Path $PSScriptRoot 'lab_host.cjs'
$logs = Join-Path $labRoot 'artifacts'
New-Item -ItemType Directory -Force -Path $logs | Out-Null
$process = Start-Process -FilePath $node -ArgumentList ('"' + $entry + '"') -WorkingDirectory $labRoot -WindowStyle Hidden -RedirectStandardOutput (Join-Path $logs 'lab-host.stdout.log') -RedirectStandardError (Join-Path $logs 'lab-host.stderr.log') -PassThru
for ($attempt=0; $attempt -lt 60; $attempt++) {
    $process.Refresh()
    if ($process.HasExited) { throw ('Lab startup failed. Read ' + (Join-Path $logs 'lab-host.stderr.log')) }
    try {
        $page = Invoke-WebRequest -UseBasicParsing -Uri 'http://127.0.0.1:8080' -TimeoutSec 1
        if ($page.StatusCode -eq 200) {
            Write-Output 'Mosquitto + Zigbee2MQTT ready: http://127.0.0.1:8080'
            exit 0
        }
    } catch { }
    Start-Sleep -Milliseconds 500
}
throw ('Startup timed out. Read logs in ' + $logs)
