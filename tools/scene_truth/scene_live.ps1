# scene_live.ps1 - B1: show the corpus to the LIVE FG, capture what it generates, align it, score it.
#
#   tools\scene_truth\scene_live.ps1 -Run <corpus> -K 4 [-Seconds 20] [-Fg build-release\phyriad_fg.exe]
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
  [switch]$Loop            # loop the sequence. Without it the player CLOSES after the last frame, so the
                           # corpus must outlast 3 s + Seconds or the FG captures nothing (seen: 0 triples
                           # on a 1 s corpus). With it, the FG sees a CUT at every seam; the scorer counts
                           # and excludes those pairs (align.json pair_ok), so a loop is the like-for-like
                           # protocol for short corpora.
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..\..')
if (-not $FgExe) { $FgExe = Join-Path $root 'build-release\phyriad_fg.exe' }
$src = Join-Path $Run ("source_k{0}" -f $K)
if (-not (Test-Path (Join-Path $src 'manifest.txt'))) { throw "no $src\manifest.txt - render the corpus with --barcode --manifest-k $K" }
$fps = [double]((Get-Content (Join-Path $src 'manifest.txt') | Where-Object { $_ -like 'fps *' }) -split '\s+')[1]
$qd = Join-Path $Run ("qdump_k{0}" -f $K)
if (Test-Path $qd) { Remove-Item -Recurse -Force $qd }
New-Item -ItemType Directory -Force $qd | Out-Null
$q = { param($s) '"' + $s + '"' }

Write-Host ("[scene-live] source {0} at {1} fps -> FG x{2}, qdump -> {3}" -f $src, $fps, $K, $qd)
$pargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File',
  (& $q (Join-Path $root 'tools\motion_truth\play_frames.ps1')), '-Dir', (& $q $src), '-Fps', $fps,
  '-Title', (& $q $Title), '-MaxFrames', 0)
if (-not $Loop) { $pargs += '-NoLoop' }
$player = Start-Process powershell -PassThru -ArgumentList $pargs
Start-Sleep -Seconds 3
$proc = Start-Process $FgExe -PassThru -Wait -NoNewWindow -ArgumentList @('--window', (& $q $Title),
  '--qdump', (& $q $qd), $Triples, '--exit-after', $Seconds, '--fg-factor', $K)
if (-not $player.HasExited) { Stop-Process -Id $player.Id -Force -ErrorAction SilentlyContinue }
$nlive = (Get-ChildItem $qd -Filter '*_live.rgba' -ErrorAction SilentlyContinue | Measure-Object).Count
Write-Host ("[scene-live] FG exited {0}; triples on disk: {1}" -f $proc.ExitCode, $nlive)
if ($nlive -eq 0) { throw "the FG wrote no triples - check the capture target and --qdump" }

& python (Join-Path $here 'scene_align.py') --qdump $qd --run $Run --k $K --arm fg
& python (Join-Path $here 'scene_report.py') --run $Run --k $K --arm truth --arm nearest --arm oracle2 --arm fg `
    --md (Join-Path $Run ("fg_k{0}.md" -f $K)) --json (Join-Path $Run ("fg_k{0}.json" -f $K))
