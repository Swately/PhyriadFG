# r4b_waitable.ps1 — the mechanism test. Hypothesis (from the 14 R4b runs): the warp batch's ~one-period completion
# latency is the keyed-mutex hand-off chained behind the previous present's CopyResource, which the flip-model
# swapchain (BufferCount 2, Present(0), no waitable) queues behind the flip; the loop is flip-locked by Present
# blocking. The existing default-off knob --present-waitable (SetMaximumFrameLatency(1) + the waitable wait BEFORE
# each present) makes the copy run immediately after the previous frame was consumed — if the hypothesis holds, the
# shipping async path must rise from 49.9 % fresh to ~100 %. Two runs (DI-3) on the default path, one with the
# 4000-us shallow queue. Made with my soul - Swately <3
param([int]$Seconds = 60)
$root = 'F:\Phyriad\projects\PhyriadFG'
$sp   = $PSScriptRoot   # outputs under tools/r4b/r4b/ (the session ran it from its scratchpad)
$out  = Join-Path $sp 'r4b'
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
Run-One 'waitable_run1' @('--present-waitable') $Seconds
Run-One 'waitable_run2' @('--present-waitable') $Seconds
Run-One 'waitable_sq4000_run1' @('--present-waitable','--shallow-queue','--shallow-queue-budget-us','4000') $Seconds
"=== done ===" | Out-File -Encoding utf8 (Join-Path $sp 'r4b_waitable_done.txt')
"=== done ==="
