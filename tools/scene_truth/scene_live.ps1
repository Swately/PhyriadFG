# scene_live.ps1 - B1: show the corpus to the LIVE FG, capture what it generates, align it, score it.
#
#   tools\scene_truth\scene_live.ps1 -Run <corpus> -K 4 [-Seconds 20] [-Fg build-release\phyriad_fg.exe]
#                                   [-Tag nostasis -FgFlags '--no-stasis'] [-Jobs 14] [-DryRun]
#
# AN A/B ARM IS A TAG (2026-09-10, REGIME_TEST_MATRIX.md family 0). -Tag <t> writes the capture to
# qdump_k<K>_<t> (or gdump_k<K>_<t>), the FG log to fg_k<K>_<t>.log, the aligned arm to arms/fg_k<K>_<t>/ and
# the scores to fg_k<K>_<t>.{md,json} -- so the DEFAULT capture (no tag) is never overwritten by an arm, and
# two arms of one corpus sit side by side. -FgFlags is the arm's extra FG flags, split on whitespace and
# appended verbatim to the FG's argument list ('--no-stasis', '--mv-sim 0.30', '--no-asw --no-mv-guided').
# The record states the arm by the flags the FG printed, not by the tag (P-027: two help texts lie about
# their default). -DryRun prints every command line exactly as it would be passed and starts nothing.
#
# What happens, in order: play_frames.ps1 presents source_k<K>/ at base_fps/K in its own window
# ("RA Motion Zoo"); the FG captures that window and, with --qdump, writes every sampled generated frame
# next to the two real frames it came from; scene_align.py decodes the barcodes and files each generated
# frame under arms/fg/ at its base index; scene_report.py scores arms/fg against the same truth the
# synthetic arms were scored against. The operator's screen is taken for ~Seconds; nothing else.
#
# Known limits, stated: --qdump is a SAMPLER (8 phase bins, >= 8 ticks between dumps) and forces the
# SYNC present path, so this measures the kernel on a subset of frames, not the shipping async path
# on all of them. The file backend (B2) is what removes both limits.
#
# -Gdump swaps the capture leg for the every-tick tap (GDUMP_PLAN.md S8): the FG runs with --gdump
# <dir> instead of --qdump, capturing EVERY recorded warp on the SHIPPING ASYNC path, then
# gdump_adapter.py reshapes that directory into the same --qdump layout so scene_align.py below runs
# UNCHANGED. -Gdump does NOT force the synchronous path (GDUMP_PLAN.md S2/DR2) -- the tap has no
# --qdump-style sampler, so the async counters (rdrop/fresh) stay live during the capture.
#
# Two traps this file already paid for: Start-Process joins its argument list with spaces and does NOT
# quote, so a title like 'RA Motion Zoo' reaches the child as three tokens unless quoted here; and
# PowerShell variable names are case-insensitive, so $fg and $Fg are the same variable.
# Made with my soul - Swately <3
param(
  [Parameter(Mandatory=$true)][string]$Run,
  [int]$K = 4,
  [int]$Seconds = 20,
  [int]$Triples = 400,
  [string]$FgExe = "",
  [string]$Title = 'RA Motion Zoo',
  [switch]$NoScore,        # capture + align only; score later (lets several captures run back to back)
  [string]$Tag = '',       # the A/B arm's name; '' = the default capture (qdump_k<K>, arms/fg_k<K>)
  [string]$FgFlags = '',   # extra FG flags for this arm, whitespace-split, appended verbatim. NOT -FgArgs:
                           # PowerShell names are case-insensitive, so $FgArgs IS this script's $fgArgs array;
                           # the header's trap bit a second time here (2026-09-10: a duplicated FG line).
  [int]$Jobs = 0,          # scene_report.py --jobs (0 = the scorer's own default, cores - 2)
  [switch]$DryRun,         # print the command lines it would run, start nothing (and touch nothing)
  [switch]$Overwrite,      # replace an existing capture dir of a run marked KEEP (refused otherwise)
  [switch]$Loop,           # loop the sequence. Without it the player CLOSES after the last frame, so the
                           # corpus must outlast 3 s + Seconds or the FG captures nothing (seen: 0 triples
                           # on a 1 s corpus). With it, the FG sees a CUT at every seam; the scorer counts
                           # and excludes those pairs (align.json pair_ok), so a loop is the like-for-like
                           # protocol for short corpora.
  [switch]$Gdump           # capture with --gdump (every recorded tick, async path) instead of --qdump
                           # (a sampler, sync path); the capture is then adapted into the same
                           # qdump_k<K> layout via gdump_adapter.py (GDUMP_PLAN.md S8). $Triples is
                           # unused in this mode (the tap has no per-run triple budget).
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..\..')
if (-not $FgExe) { $FgExe = Join-Path $root 'build-release\phyriad_fg.exe' }
$src = Join-Path $Run ("source_k{0}" -f $K)
if (-not (Test-Path (Join-Path $src 'manifest.txt'))) { throw "no $src\manifest.txt - render the corpus with --barcode --manifest-k $K" }
$fps = [double]((Get-Content (Join-Path $src 'manifest.txt') | Where-Object { $_ -like 'fps *' }) -split '\s+')[1]
$sfx = if ($Tag) { "_" + $Tag } else { "" }
$qd = Join-Path $Run ("qdump_k{0}{1}" -f $K, $sfx)
$gd = Join-Path $Run ("gdump_k{0}{1}" -f $K, $sfx)
$q = { param($s) '"' + $s + '"' }

if ($Gdump) {
  Write-Host ("[scene-live] source {0} at {1} fps -> FG x{2}, gdump -> {3} -> adapted -> {4}" -f $src, $fps, $K, $gd, $qd)
} else {
  Write-Host ("[scene-live] source {0} at {1} fps -> FG x{2}, qdump -> {3}" -f $src, $fps, $K, $qd)
}
$pargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
  (& $q (Join-Path $root 'tools\motion_truth\play_frames.ps1')), '-Dir', (& $q $src), '-Fps', $fps,
  '-Title', (& $q $Title), '-MaxFrames', 0)
if (-not $Loop) { $pargs += '-NoLoop' }
# The FG's own stdout is the measurement's provenance (present / capture rates, the real+generated
# tally). It is kept next to the corpus as fg_k<K>.log; the stepper reads it by default.
$fglog = Join-Path $Run ("fg_k{0}{1}.log" -f $K, $sfx)
if ($Gdump) {
  $fgArgs = @('--window', (& $q $Title), '--gdump', (& $q $gd), '--exit-after', $Seconds, '--fg-factor', $K)
} else {
  $fgArgs = @('--window', (& $q $Title), '--qdump', (& $q $qd), $Triples, '--exit-after', $Seconds, '--fg-factor', $K)
}
if ($FgFlags.Trim()) { $fgArgs += ($FgFlags -split '\s+' | Where-Object { $_ }) }
$jobsArg = @(); if ($Jobs -gt 0) { $jobsArg = @('--jobs', $Jobs) }
if ($DryRun) {
  Write-Host ('[dry-run] player: powershell ' + ($pargs -join ' '))
  Write-Host ('[dry-run] fg    : ' + $FgExe + ' ' + ($fgArgs -join ' ') + '  > ' + $fglog)
  if ($Gdump) { Write-Host ('[dry-run] adapt : python gdump_adapter.py --dir ' + $gd + ' --out ' + $qd + ' --link') }
  Write-Host ('[dry-run] align : python scene_align.py --qdump ' + $qd + ' --run ' + $Run + ' --k ' + $K + ' --arm ' + ('fg_k{0}{1}' -f $K, $sfx))
  Write-Host ('[dry-run] score : python scene_report.py --run ' + $Run + ' --k ' + $K + ' --arm truth --arm nearest --arm oracle2 --arm ' + ('fg_k{0}{1}' -f $K, $sfx) + ' ' + ($jobsArg -join ' '))
  exit 0
}
# NOTHING ABOVE THIS LINE TOUCHES THE DISK OR STARTS A PROCESS. On 2026-09-10 a -DryRun of this script deleted the raw qdump_k4 of the KEEP
# run sc_live, because the Remove-Item below used to sit above the dry-run exit: a dry run that mutates is the
# instrument lying about itself (LEARNING_LOG P-028). A run marked KEEP (scene_runs.py keep) refuses to overwrite an
# existing capture directory unless -Overwrite is passed; a fresh tag never collides.
$keep = Test-Path (Join-Path $Run 'KEEP')
foreach ($dir in @($qd) + $(if ($Gdump) { @($gd) } else { @() })) {
  if (Test-Path $dir) {
    if ($keep -and -not $Overwrite) { throw "$dir exists and $Run is marked KEEP - use a new -Tag, or -Overwrite to replace it" }
    Remove-Item -Recurse -Force $dir
  }
}
New-Item -ItemType Directory -Force $qd | Out-Null
# the tap creates its own directory (GDUMP_PLAN.md S2 "created by the tap"); $gd stays absent until the FG makes it
# the player starts only here: after the dry-run exit and after the KEEP guard (2026-09-10: seven "RA Motion Zoo"
# windows were left looping on the operator's screen by dry-runs that had already started it -- P-028)
$player = Start-Process powershell -PassThru -ArgumentList $pargs
Start-Sleep -Seconds 3
$proc = Start-Process $FgExe -PassThru -Wait -NoNewWindow -RedirectStandardOutput $fglog `
  -RedirectStandardError (Join-Path $Run ("fg_k{0}{1}.err" -f $K, $sfx)) -ArgumentList $fgArgs
Get-Content $fglog -Tail 3 | ForEach-Object { Write-Host ('[fg] ' + $_) }
if (-not $player.HasExited) { Stop-Process -Id $player.Id -Force -ErrorAction SilentlyContinue }
if ($Gdump) {
  Write-Host ("[scene-live] FG exited {0}; adapting {1} -> {2}" -f $proc.ExitCode, $gd, $qd)
  & python (Join-Path $here 'gdump_adapter.py') --dir $gd --out $qd --link
  if ($LASTEXITCODE -ne 0) { throw "gdump_adapter.py failed (exit $LASTEXITCODE) - see its output above" }
}
$nlive = (Get-ChildItem $qd -Filter '*_live.rgba' -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Host ("[scene-live] FG exited {0}; triples on disk: {1}" -f $proc.ExitCode, $nlive)
if ($nlive -eq 0) { throw "the FG wrote no triples - check the capture target and --qdump" }

# ONE ARM DIRECTORY PER K. Arms are indexed by BASE frame, and the mids of k=2, 4 and 8 overlap (every
# odd frame is a k=2 mid AND a k=4 mid AND a k=8 mid), so a shared arms/fg/ let a later run overwrite an
# earlier run's frames and its align.json -- seen: the k=8 run replaced k=2's, and a concurrent scorer
# read the mixture. fg_k<K> keeps every run's output intact and the scorer reads only its own.
$arm = "fg_k{0}{1}" -f $K, $sfx
& python (Join-Path $here 'scene_align.py') --qdump $qd --run $Run --k $K --arm $arm
if ($NoScore) { Write-Host "[scene-live] -NoScore: aligned into arms\$arm; score with scene_report.py --arm $arm"; exit 0 }
& python (Join-Path $here 'scene_report.py') --run $Run --k $K --arm truth --arm nearest --arm oracle2 --arm $arm `
    --md (Join-Path $Run ("fg_k{0}{1}.md" -f $K, $sfx)) --json (Join-Path $Run ("fg_k{0}{1}.json" -f $K, $sfx)) @jobsArg
