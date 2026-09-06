# r4b_beat.ps1 — the beat test for the shallow-queue regime oscillation: the three 4000-us runs alternate between a
# ~240/s hit regime and a ~88/s one with a ~16.5-17 s period. The panel flips at 239.755 Hz (CSV flip stats), the
# ball zoo's timer runs at 60.000 Hz: their beat is 1/(60 - 239.755/4) = 16.3 s; the timer/panel beat (240.000 vs
# 239.755) is 4.1 s. If the source phase drives the regimes, a zoo at 61 or 59 fps must show a ~1 s period
# (1/(61-59.939) = 0.94 s; 1/(59.939-59) = 1.06 s). Two runs, 60 s each, same flags as sq4000. Made with my soul - Swately <3
param([int]$Seconds = 60)
$root = 'F:\Phyriad\projects\PhyriadFG'
$sp   = $PSScriptRoot   # outputs under tools/r4b/r4b/ (the session ran it from its scratchpad)
$out  = Join-Path $sp 'r4b'
$exe = Join-Path $root 'build-release\phyriad_fg.exe'
$zooScript = Join-Path $root 'tools\ball_zoo.ps1'
function Run-One([string]$tag, [int]$fps, [string[]]$fgArgs, [int]$secs) {
    $zoo = Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',$zooScript,'-Fps',"$fps",'-W','1920','-H','1080','-X','0','-Y','0' -PassThru
    Start-Sleep -Seconds 4
    $csv = Join-Path $out ($tag + '.csv'); $log = Join-Path $out ($tag + '.log')
    $argv = @('--window','RA Ball Zoo','--exit-after',"$secs",'--csv',$csv,'--warp-timing') + $fgArgs
    & $exe @argv > $log 2>&1
    $rc = $LASTEXITCODE
    Stop-Process -Id $zoo.Id -Force -ErrorAction SilentlyContinue
    $exit = Select-String -Path $log -Pattern 'bounded-run clean exit' | Select-Object -Last 1 | ForEach-Object { $_.Line }
    "${tag}: rc=$rc | $exit"
    Start-Sleep -Seconds 3
}
Run-One 'beat61_run1' 61 @('--shallow-queue','--shallow-queue-budget-us','4000') $Seconds
Run-One 'beat59_run1' 59 @('--shallow-queue','--shallow-queue-budget-us','4000') $Seconds
"=== done ===" | Out-File -Encoding utf8 (Join-Path $sp 'r4b_beat_done.txt')
"=== done ==="
