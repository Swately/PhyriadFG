# r4b_measure.ps1 — the fresh-frame question, measured with the R4b instruments (--warp-timing --csv, 60 s, DI-3 two
# runs per configuration): async (the shipping default) vs sync (--no-async-present) vs --shallow-queue at its
# default budget and at a wide one. Outputs: fresh/s, rdrop/s, the warp batch's GPU time and its submit->fence
# latency (per fresh present in the CSV; EMAs in the stats line), presents, lat. Made with my soul - Swately <3
param([int]$Seconds = 60)
$root = 'F:\Phyriad\projects\PhyriadFG'
$sp   = $PSScriptRoot   # outputs under tools/r4b/r4b/ (the session ran it from its scratchpad)
$out  = Join-Path $sp 'r4b'
New-Item -ItemType Directory -Force $out | Out-Null
# NOTE: launched from the PowerShell host, `> $log` writes UTF-16; r4b_parse.py reads both encodings.
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
foreach ($r in 1, 2) {
    Run-One "async_run$r"  @() $Seconds
    Run-One "sync_run$r"   @('--no-async-present') $Seconds
}
Run-One 'sq_default_run1' @('--shallow-queue') $Seconds
Run-One 'sq_wide_run1'    @('--shallow-queue','--shallow-queue-budget-us','4000') $Seconds
"=== done ==="
