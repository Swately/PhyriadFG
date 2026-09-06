# r4d_load.ps1 — the validation the present-policy decision (XR15) was missing: the shipping async default vs
# --present-waitable UNDER tools\gpu_load.exe (the R4 gate's synthetic saturation), DI-3 two runs each, 60 s,
# --csv --warp-timing. What it decides: whether the waitable's blocking wait costs presents or fresh frames when
# the device is busy (the P thread is held up to a full period per tick on that path). Made with my soul - Swately <3
param([int]$Seconds = 60)
$root = 'F:\Phyriad\projects\PhyriadFG'
$sp   = $PSScriptRoot   # outputs under tools/r4b/r4d/ (the session ran it from its scratchpad)
$out  = Join-Path $sp 'r4d'
New-Item -ItemType Directory -Force $out | Out-Null
$exe = Join-Path $root 'build-release\phyriad_fg.exe'
$zooScript = Join-Path $root 'tools\ball_zoo.ps1'
function Run-One([string]$tag, [string[]]$fgArgs, [int]$secs) {
    $zoo = Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',$zooScript,'-Fps','60','-W','1920','-H','1080','-X','0','-Y','0' -PassThru
    Start-Sleep -Seconds 4
    $csv = Join-Path $out ($tag + '.csv'); $log = Join-Path $out ($tag + '.log')
    $argv = @('--window','RA Ball Zoo','--exit-after',"$secs",'--csv',$csv,'--warp-timing') + $fgArgs
    & $exe @argv > $log 2>&1
    $rc = $LASTEXITCODE
    Stop-Process -Id $zoo.Id -Force -ErrorAction SilentlyContinue
    $exit = Select-String -Path $log -Pattern 'bounded-run clean exit' | Select-Object -Last 1 | ForEach-Object { $_.Line }
    "${tag}: rc=$rc | $exit"
    Select-String -Path $log -Pattern 'fps \(present\)' | Select-Object -Last 1 | ForEach-Object { "    " + $_.Line.Substring(0, [Math]::Min(330, $_.Line.Length)) }
    Start-Sleep -Seconds 3
}
$load = Start-Process (Join-Path $root 'tools\gpu_load.exe') -PassThru -WindowStyle Minimized
Start-Sleep -Seconds 3
try {
    foreach ($r in 1, 2) {
        Run-One "load_async_run$r"    @() $Seconds
        Run-One "load_waitable_run$r" @('--present-waitable') $Seconds
    }
} finally {
    Stop-Process -Id $load.Id -Force -ErrorAction SilentlyContinue
}
"=== done ===" | Out-File -Encoding utf8 (Join-Path $sp 'r4d_load_done.txt')
"=== done ==="
