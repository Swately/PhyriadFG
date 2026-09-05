# tools/fg_core_ab.ps1 — the R3 regression harness: the fg_core.comp kernel against the legacy warp on the ball zoo.
#
# Sides (same binary, same source, interleaved where paired, DI-3 two runs per number):
#   AB_packed   --fg-core-ab                      both kernels every tick, mv_guided.sim PACKED (XR1)
#   AB_clean    --fg-core-ab --fg-core-clean-sim  the exact --mv-sim (the 1-ulp band)
#   RED_nost    --fg-core-ab --no-single-track    the CONTROL that must be red (EMPIRICAL_TEST §3.3): the legacy takes its
#                                                 soft-gate path, fg_core the hard select -> diff_px >> 0
#   M3_legacy   (default) / M3_fgcore --fg-core   presents/60 s, the product on each kernel
# Backgrounds: grid (the lattice), noise (aperiodic texture), grid + pan 120 px/s (gme != 0; |mv| = 2.0 px/frame sits
# ON the inertia gate's `length(mv) > 2.0` knife-edge -- the content where a 1-ulp difference flips decisions).
# What to expect on the product build (records/R3_GATE.md §5): <= ~100 one-level pixels per 60 s on grid/noise, column
# bursts with max_delta up to ~13 on pan -- all from the driver's FMA contraction (proved with build-nocontract.bat:
# 0 pixels on 24,227 ticks). Anything with max_delta > 1 on grid/noise, or a rate far above 1e-8, is a REAL difference.
# Made with my soul - Swately <3
param([int]$Seconds = 60, [switch]$Quick, [string]$Out = (Join-Path $env:TEMP 'fg_core_ab'))
$root = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force $Out | Out-Null
$exe = Join-Path $root 'build-release\phyriad_fg.exe'
$zooScript = Join-Path $root 'tools\ball_zoo.ps1'

function Run-One([string]$tag, [string[]]$fgArgs, [string[]]$zooArgs) {
    $zooBase = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$zooScript,'-Fps','60','-W','1920','-H','1080','-X','0','-Y','0')
    $zoo = Start-Process powershell -ArgumentList ($zooBase + $zooArgs) -PassThru
    Start-Sleep -Seconds 4
    $csv = Join-Path $Out ($tag + '.csv'); $log = Join-Path $Out ($tag + '.log')
    $argv = @('--window','RA Ball Zoo','--exit-after',"$Seconds",'--csv',$csv) + $fgArgs
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & $exe @argv > $log 2>&1
    $rc = $LASTEXITCODE
    $sw.Stop()
    Stop-Process -Id $zoo.Id -Force -ErrorAction SilentlyContinue
    $exit = Select-String -Path $log -Pattern 'bounded-run clean exit|\[ra\] done' | Select-Object -Last 1 | ForEach-Object { $_.Line }
    $r3   = Select-String -Path $log -Pattern '^\[layertab\] R3' | Select-Object -First 1 | ForEach-Object { $_.Line }
    $ab   = Select-String -Path $log -Pattern '^\[fg-core-ab\] TOTAL' | Select-Object -Last 1 | ForEach-Object { $_.Line }
    $env  = Select-String -Path $log -Pattern '^\[layertab\] R3 envelope' | Select-Object -First 1 | ForEach-Object { $_.Line }
    "{0}: rc={1} wall={2:N1}s" -f $tag, $rc, $sw.Elapsed.TotalSeconds
    "    exit: $exit"
    if ($r3)  { "    r3:   $r3" }
    if ($env) { "    env:  $env" }
    if ($ab)  { "    ab:   $ab" }
    Start-Sleep -Seconds 3
}

$grid  = @()
$noise = @('-BgClass','noise')
$pan   = @('-PanPx','120')

"=== 1. the M4 instrument: both kernels per tick, PACKED sim ==="
Run-One 'AB_packed_grid_run1'  @('--fg-core-ab') $grid
Run-One 'AB_packed_noise_run1' @('--fg-core-ab') $noise
if (-not $Quick) {
  Run-One 'AB_packed_grid_run2'  @('--fg-core-ab') $grid
  Run-One 'AB_packed_noise_run2' @('--fg-core-ab') $noise
  Run-One 'AB_packed_pan_run1'   @('--fg-core-ab') $pan
}
"=== 2. the control that must be RED (the instrument can see a difference) ==="
Run-One 'RED_nost_grid_run1' @('--fg-core-ab','--no-single-track') $grid
"=== 3. the clean sim (XR1's second row) ==="
Run-One 'AB_clean_grid_run1' @('--fg-core-ab','--fg-core-clean-sim') $grid
if (-not $Quick) { Run-One 'AB_clean_noise_run1' @('--fg-core-ab','--fg-core-clean-sim') $noise }
"=== 4. M3: presents/60 s, the product on each kernel, interleaved, 2 runs/side ==="
if (-not $Quick) {
  Run-One 'M3_legacy_run1' @() $grid
  Run-One 'M3_fgcore_run1' @('--fg-core') $grid
  Run-One 'M3_legacy_run2' @() $grid
  Run-One 'M3_fgcore_run2' @('--fg-core') $grid
}
"=== done: $Out ==="
