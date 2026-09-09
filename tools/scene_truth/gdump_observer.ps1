# gdump_observer.ps1 - gates G1 + G2 of docs/planning/GDUMP_PLAN.md: the observer effect of --gdump, measured.
#
#   tools\scene_truth\gdump_observer.ps1 -Run <corpus> -K 4 -Seconds 20 -Repeats 2 `
#       -BaseExe <pre-change phyriad_fg.exe> [-FgExe build-release\phyriad_fg.exe] [-Loop]
#
# Runs, interleaved A,B,C,D,A,B,C,D (EMPIRICAL_TEST §2: alternate, never pair), each for -Seconds on the same
# corpus shown by play_frames.ps1, keeping every FG log under <Run>\observer_k<K>\<arm>_r<i>.log:
#   base      the PRE-CHANGE binary, default flags                 (G1: byte-identical-off, the reference)
#   off       the NEW binary, default flags                        (G1: must sit inside base's spread)
#   nocopy    the NEW binary, --gdump <dir> --gdump-ring 0         (G2 third arm: armed, no copy — creation-time cost only, CR5)
#   gdump     the NEW binary, --gdump <dir>                        (G2: the tap)
# Every arm carries --wsub --warp-timing so the log has the `up` segment (CR4) and the GPU batch time.
# Then gdump_observer.py summarises: per-arm means of present fps / warp ms / fresh / rdrop / up / gpu, the
# run-to-run r per number (DI-3), and the arm deltas against base. The operator's screen is taken for
# 8 x (3 s + Seconds); nothing else. -Loop as in scene_live.ps1 (a looped corpus shows the FG a CUT per lap).
# Made with my soul - Swately <3
param(
  [Parameter(Mandatory=$true)][string]$Run,
  [int]$K = 4,
  [int]$Seconds = 20,
  [int]$Repeats = 2,
  [Parameter(Mandatory=$true)][string]$BaseExe,
  [string]$FgExe = "",
  [string]$Title = 'RA Motion Zoo',
  [switch]$Loop
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..\..')
if (-not $FgExe) { $FgExe = Join-Path $root 'build-release\phyriad_fg.exe' }
if (-not (Test-Path $BaseExe)) { throw "no base exe at $BaseExe" }
$src = Join-Path $Run ("source_k{0}" -f $K)
if (-not (Test-Path (Join-Path $src 'manifest.txt'))) { throw "no $src\manifest.txt - render the corpus with --barcode --manifest-k $K" }
$fps = [double]((Get-Content (Join-Path $src 'manifest.txt') | Where-Object { $_ -like 'fps *' }) -split '\s+')[1]
$obs = Join-Path $Run ("observer_k{0}" -f $K)
New-Item -ItemType Directory -Force $obs | Out-Null
$q = { param($s) '"' + $s + '"' }
$common = @('--window', (& $q $Title), '--exit-after', $Seconds, '--fg-factor', $K, '--wsub', '--warp-timing')
$arms = @(
  @{ name='base';   exe=$BaseExe; extra=@() },
  @{ name='off';    exe=$FgExe;   extra=@() },
  @{ name='nocopy'; exe=$FgExe;   extra=@('--gdump', (& $q (Join-Path $obs 'gd_nocopy')), '--gdump-ring', 0) },
  @{ name='gdump';  exe=$FgExe;   extra=@('--gdump', (& $q (Join-Path $obs 'gd_gdump'))) }
)
Write-Host ("[observer] corpus {0} at {1} fps -> x{2}; {3} arms x {4} repeats x {5} s; logs -> {6}" -f $src, $fps, $K, $arms.Count, $Repeats, $Seconds, $obs)
for ($r = 1; $r -le $Repeats; $r++) {
  foreach ($arm in $arms) {
    foreach ($d in @('gd_nocopy', 'gd_gdump')) { $p = Join-Path $obs $d; if (Test-Path $p) { Remove-Item -Recurse -Force $p } }
    $pargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
      (& $q (Join-Path $root 'tools\motion_truth\play_frames.ps1')), '-Dir', (& $q $src), '-Fps', $fps,
      '-Title', (& $q $Title), '-MaxFrames', 0)
    if (-not $Loop) { $pargs += '-NoLoop' }
    $player = Start-Process powershell -PassThru -ArgumentList $pargs
    Start-Sleep -Seconds 3
    $log = Join-Path $obs ("{0}_r{1}.log" -f $arm.name, $r)
    $err = Join-Path $obs ("{0}_r{1}.err" -f $arm.name, $r)
    $proc = Start-Process $arm.exe -PassThru -NoNewWindow -RedirectStandardOutput $log -RedirectStandardError $err -ArgumentList ($common + $arm.extra)
    # A bounded wait, never -Wait: the 2026-09-09 r2 run wedged at exit (a pending timeline signal under
    # vkDestroySemaphore, CR3) and -Wait would have held the screen forever. 30 s of grace past --exit-after.
    $exited = $proc.WaitForExit(($Seconds + 30) * 1000)
    if (-not $exited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue; Write-Host ("[observer] {0} r{1}: FG did NOT exit {2} s after --exit-after -- KILLED (a CR3 failure; this run's exit is not clean)" -f $arm.name, $r, 30) }
    if (-not $player.HasExited) { Stop-Process -Id $player.Id -Force -ErrorAction SilentlyContinue }
    $last = Get-Content $log -Tail 1
    $code = if ($exited) { $proc.ExitCode } else { 'KILLED' }
    Write-Host ("[observer] {0} r{1}: exit {2} | {3}" -f $arm.name, $r, $code, $last)
    if ($arm.name -eq 'gdump') {
      $sum = Join-Path (Join-Path $obs 'gd_gdump') 'summary.txt'
      if (Test-Path $sum) { Copy-Item $sum (Join-Path $obs ("gdump_r{0}_summary.txt" -f $r)) -Force; Get-Content $sum | Select-Object -First 4 | ForEach-Object { Write-Host ('    ' + $_) } }
    }
    Start-Sleep -Seconds 2
  }
}
& python (Join-Path $here 'gdump_observer.py') --dir $obs --repeats $Repeats
