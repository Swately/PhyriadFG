# marker_live.ps1 - K4: the live runner for marker corpora (MOTION_TRUTH T3-T5), twin of scene_live.ps1.
#
#   tools\motion_truth\marker_live.ps1 -Zoo <zoodir> [-Run <dir>] [-Tag r1] [-Seconds 20] [-Triples 48]
#                                       [-K 4] [-FgExe build-release\phyriad_fg.exe] [-Title 'RA Motion Zoo']
#                                       [-FgFlags '--no-mv-candsel'] [-Gdump] [-NoLoop] [-NoExtract]
#                                       [-Jobs 8] [-DryRun]
#
# WHY IT EXISTS. The M1 chain (marker_zoo.py -> play_frames.ps1 -> --qdump -> marker_extract.py ->
# motion_report.py, M1_SRC_RATE.md) was run by hand, one shell command at a time, for every arm. This
# makes one arm -- one capture + one extraction -- a single invocation, the way scene_live.ps1 already
# does for the scene_truth corpora; it is deliberately that script's twin, not a rewrite. -Tag names the
# arm (DI-3 wants two runs per side, e.g. -Tag r1 and -Tag r2) so two arms of one zoo sit side by side
# instead of one overwriting the other's dump, log and detections.
#
# -FgFlags is the arm's extra FG flags, split on whitespace and appended verbatim to the FG's argument
# list ('--no-mv-candsel', '--no-asw --no-mv-guided'). -DryRun prints every command line exactly as it
# would be passed and starts nothing -- no window, no FG process; safe to run with no screen.
#
# -Gdump swaps the capture leg for the every-tick tap (GDUMP_PLAN.md S8): the FG runs with --gdump
# <dir> instead of --qdump <dir> <Triples>, then gdump_adapter.py reshapes that directory into the same
# qdump layout so marker_extract.py below runs UNCHANGED. $Triples is unused in this mode.
#
# Two traps inherited from scene_live.ps1, paid for there and not re-paid here: Start-Process joins its
# argument list with spaces and does NOT quote, so a title like 'RA Motion Zoo' reaches the child as
# three tokens unless quoted here; and PowerShell variable names are case-insensitive, so $fg and $Fg
# are the same variable.
# Made with my soul - Swately <3
param(
  [Parameter(Mandatory=$true)][string]$Zoo,
  [string]$Run = "",       # default: the zoo's parent directory
  [string]$Tag = 'r1',     # the arm's name; the dump dir is <Run>\qdump_<Tag> (or gdump_<Tag> -> adapted)
  [int]$Seconds = 20,
  [int]$Triples = 48,
  [int]$K = 4,
  [string]$FgExe = "",
  [string]$Title = 'RA Motion Zoo',
  [string]$FgFlags = '',    # extra FG flags for this arm, whitespace-split, appended verbatim
  [switch]$Gdump,          # capture with --gdump (every recorded tick, async path) instead of --qdump
                           # (a sampler, sync path); adapted into the same qdump_<Tag> layout via
                           # gdump_adapter.py. $Triples is unused in this mode.
  [switch]$NoLoop,         # passed to play_frames.ps1; without it the player loops (its own default)
  [switch]$NoExtract,      # capture only; skip marker_extract.py and the motion_report.py suggestion
  [int]$Jobs = 0,          # marker_extract.py --jobs (0 = omit the flag, the extractor's own default)
  [switch]$DryRun          # print the command lines it would run, start nothing
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..\..')
if (-not $FgExe) { $FgExe = Join-Path $root 'build-release\phyriad_fg.exe' }
if (-not (Test-Path (Join-Path $Zoo 'trajectories.json'))) { throw "no $Zoo\trajectories.json - render the zoo with marker_zoo.py --out $Zoo" }
if (-not $Run) { $Run = Split-Path -Parent (Resolve-Path $Zoo) }
$fps = [double]((Get-Content (Join-Path $Zoo 'trajectories.json') -Raw | ConvertFrom-Json).fps)
$qd = Join-Path $Run ("qdump_{0}" -f $Tag)
$gd = Join-Path $Run ("gdump_{0}" -f $Tag)
$q = { param($s) '"' + $s + '"' }

$pargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
  (& $q (Join-Path $here 'play_frames.ps1')), '-Dir', (& $q $Zoo), '-Fps', $fps,
  '-Title', (& $q $Title), '-MaxFrames', 0)
if ($NoLoop) { $pargs += '-NoLoop' }

if ($Gdump) {
  $fgArgList = @('--window', (& $q $Title), '--gdump', (& $q $gd), '--exit-after', $Seconds, '--fg-factor', $K)
} else {
  $fgArgList = @('--window', (& $q $Title), '--qdump', (& $q $qd), $Triples, '--exit-after', $Seconds, '--fg-factor', $K)
}
if ($FgFlags.Trim()) {
  foreach ($tok in ($FgFlags -split '\s+' | Where-Object { $_ })) { $fgArgList += (& $q $tok) }
}
$fglog = Join-Path $Run ("fg_{0}.log" -f $Tag)
$fgerr = Join-Path $Run ("fg_{0}.err" -f $Tag)
$csv = Join-Path $Run ("detections_{0}.csv" -f $Tag)
$otherTag = if ($Tag -eq 'r1') { 'r2' } elseif ($Tag -eq 'r2') { 'r1' } else { 'r2' }
$csvOther = Join-Path $Run ("detections_{0}.csv" -f $otherTag)
$md = Join-Path $Run ("motion_{0}_{1}.md" -f $Tag, $otherTag)
$adapterPy = Join-Path $root 'tools\scene_truth\gdump_adapter.py'
$extractPy = Join-Path $here 'marker_extract.py'
$reportPy = Join-Path $here 'motion_report.py'
$jobsArg = @(); if ($Jobs -ne 0) { $jobsArg = @('--jobs', $Jobs) }
$extractArgs = @('--dump', $qd, '--zoo', $Zoo, '--out', $csv, '--fps', $fps) + $jobsArg
$reportCmd = 'python "' + $reportPy + '" --zoo "' + $Zoo + '" --run "' + $csv + '" --run "' + $csvOther + '" --md "' + $md + '"'

if ($DryRun) {
  Write-Host ('[dry-run] player : powershell ' + ($pargs -join ' '))
  Write-Host ('[dry-run] fg     : ' + $FgExe + ' ' + ($fgArgList -join ' ') + '  > ' + $fglog)
  if ($Gdump) { Write-Host ('[dry-run] adapt  : python "' + $adapterPy + '" --dir ' + $gd + ' --out ' + $qd + ' --link') }
  if (-not $NoExtract) {
    Write-Host ('[dry-run] extract: python "' + $extractPy + '" ' + ($extractArgs -join ' '))
    Write-Host ('[dry-run] report : ' + $reportCmd)
  }
  exit 0
}

Write-Host ("[marker-live] zoo {0} at {1} fps -> FG x{2}, {3} -> {4}" -f $Zoo, $fps, $K, $(if ($Gdump) { "gdump -> $gd -> adapted" } else { "qdump" }), $qd)
if (Test-Path $qd) { Remove-Item -Recurse -Force $qd }
New-Item -ItemType Directory -Force $qd | Out-Null
if ($Gdump -and (Test-Path $gd)) { Remove-Item -Recurse -Force $gd }

$player = Start-Process powershell -PassThru -ArgumentList $pargs
Start-Sleep -Seconds 3
$proc = Start-Process $FgExe -PassThru -Wait -NoNewWindow -RedirectStandardOutput $fglog `
  -RedirectStandardError $fgerr -ArgumentList $fgArgList
Get-Content $fglog -Tail 3 | ForEach-Object { Write-Host ('[fg] ' + $_) }
if (-not $player.HasExited) { Stop-Process -Id $player.Id -Force -ErrorAction SilentlyContinue }

if ($Gdump) {
  Write-Host ("[marker-live] FG exited {0}; adapting {1} -> {2}" -f $proc.ExitCode, $gd, $qd)
  & python $adapterPy --dir $gd --out $qd --link
  if ($LASTEXITCODE -ne 0) { throw "gdump_adapter.py failed (exit $LASTEXITCODE) - see its output above" }
}

$nlive = (Get-ChildItem $qd -Filter '*_live.rgba' -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Host ("[marker-live] FG exited {0}; triples on disk: {1}" -f $proc.ExitCode, $nlive)
if ($nlive -eq 0) { throw "the FG wrote no triples - check the capture target and --qdump/--gdump" }

if ($NoExtract) { Write-Host "[marker-live] -NoExtract: capture only, dump kept at $qd"; exit 0 }

$extractOut = & python $extractPy @extractArgs 2>&1
$extractOut | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) { throw "marker_extract.py failed (exit $LASTEXITCODE)" }
$realLine = $extractOut | Where-Object { $_ -match 'REAL-PLANE CHECK' } | Select-Object -Last 1
if ($realLine) { Write-Host ('[marker-live] ' + $realLine) }

Write-Host "[marker-live] wrote $csv; for DI-3 run this arm again with -Tag $otherTag, then:"
Write-Host ('  ' + $reportCmd)
