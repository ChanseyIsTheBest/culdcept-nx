# culdcept-nx (2026-11-19): watches an emulator run that tools/run_ryujinx.ps1 started (run it beside that script, in
# the background): every -Every seconds, Ryujinx's CPU time used since the last look (in cores: 1.0 = one core busy)
# and the game log's last line; when the log first says there is no game image to capture, it asks the stall watch
# for an on-demand dump (the file sdmc:/switch/stall_dump; profiler/switch_stall_watch.h) and keeps measuring. Writes
# <out>/emu_watch.txt. Reads only; stops nothing.
#   powershell -File tools/emu_watch.ps1 -Name emu4 -Seconds 300 [-Every 5]
param([string]$Name = "emu", [int]$Seconds = 300, [int]$Every = 5)
$root = Split-Path -Parent $PSScriptRoot
$sdRoot = "D:\New folder\ryu-data\culdcept\sdcard\switch"
$log = Join-Path $sdRoot "culdcept-nx\logs\renut_001.log"
$out = Join-Path $root "out\emu_runs\$Name"
New-Item -ItemType Directory -Force $out | Out-Null
$report = Join-Path $out "emu_watch.txt"
"emu_watch: $Name, every $Every s" | Set-Content -Path $report -Encoding utf8
$deadline = (Get-Date).AddSeconds($Seconds)
$lastCpu = $null; $lastTime = $null; $asked = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds $Every
    $p = Get-Process -Name Ryujinx -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $p) { continue }
    $now = Get-Date
    $cpu = $p.TotalProcessorTime.TotalSeconds
    $cores = if ($lastCpu -ne $null) { ($cpu - $lastCpu) / ($now - $lastTime).TotalSeconds } else { 0 }
    $lastCpu = $cpu; $lastTime = $now
    $tail = ""
    if (Test-Path $log) {
        $tail = (Get-Content $log -Tail 1 -ErrorAction SilentlyContinue)
        if ($tail) { $tail = $tail.Substring(0, [Math]::Min(140, $tail.Length)) }
        if (-not $asked -and (Select-String -Path $log -Pattern 'no hay imagen del juego' -Quiet)) {
            Set-Content -Path (Join-Path $sdRoot "stall_dump") -Value "dump" -Encoding ascii
            $asked = $true
            "{0:HH:mm:ss} asked for an on-demand dump (sdmc:/switch/stall_dump)" -f $now | Add-Content -Path $report -Encoding utf8
        }
    }
    ("{0:HH:mm:ss} cores {1:N2} | {2}" -f $now, $cores, $tail) | Add-Content -Path $report -Encoding utf8
}
