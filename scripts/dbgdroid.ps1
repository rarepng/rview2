# - conf -
$PackageName = "red.rview2" 
$ActivityName = "red.rview2.RviewActivity" 
$LldbServerPath = "/data/local/tmp/lldb-server"
$LockFile = "/data/local/tmp/dbg_lock"
# --------



Write-Host "clearing previous debug sessions..." -ForegroundColor Cyan
adb -s 192.168.2.225:6546 shell "su -c 'killall lldb-server'" 2>$null
adb -s 192.168.2.225:6546 forward --remove tcp:5039 2>$null

Write-Host "c++ lock file..." -ForegroundColor Cyan
adb -s 192.168.2.225:6546 shell "su -c 'touch $LockFile && chmod 666 $LockFile'" 

Write-Host "LLDB port (5039)..." -ForegroundColor Cyan
adb -s 192.168.2.225:6546 forward tcp:5039 tcp:5039

Write-Host "launching $PackageName..." -ForegroundColor Cyan
adb -s 192.168.2.225:6546 shell am start -S -n "$PackageName/$ActivityName" | Out-Null

$AppPid = $null
$TimeoutAttempts = 50
$Attempts = 0

Write-Host "making sure process spawned successfully..." -NoNewline -ForegroundColor Cyan
while ([string]::IsNullOrWhiteSpace($AppPid) -and $Attempts -lt $TimeoutAttempts) {
    Start-Sleep -Milliseconds 200
    
    $rawPid = adb -s 192.168.2.225:6546 shell pidof $PackageName
    if ($rawPid -match '\b(\d+)\b') {
        $AppPid = $Matches[1]
    }
    
    Write-Host "." -NoNewline -ForegroundColor Cyan
    $Attempts++
}

if ([string]::IsNullOrWhiteSpace($AppPid)) {
    Write-Host "`ntimed out waiting for PID." -ForegroundColor Red
    exit
}

Write-Host "`nfound PID: $AppPid" -ForegroundColor Green

Write-Host "attaching lldb-server as root(required)..." -ForegroundColor Cyan
Start-Process -FilePath "adb" -ArgumentList "-s 192.168.2.225:6546 shell su -c '$LldbServerPath gdbserver *:5039 --attach $AppPid'" -WindowStyle Hidden

Write-Host " gotta wait a few seconds for symbols to load " -ForegroundColor Green

Read-Host

Write-Host "removing lock file to resume c++ execution..." -ForegroundColor Cyan
adb -s 192.168.2.225:6546 shell "su -c 'rm $LockFile'"

Write-Host "app released" -ForegroundColor Green

adb -s 192.168.2.225:6546 logcat --pid=$AppPid
