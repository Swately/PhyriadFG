# gdump_g4.ps1 - gate G4 of docs/planning/GDUMP_PLAN.md: BOTH taps in one run, then the byte-for-byte cross-check.
#
#   tools\scene_truth\gdump_g4.ps1 -Run <corpus> -K 4 [-Seconds 20] [-Triples 16] [-FgExe build-release\phyriad_fg.exe] [-Loop]
#
# --qdump forces the SYNCHRONOUS present path (cli.cpp resolve_config), so this run measures nothing about pacing;
# it exists only to prove that for every tick the sampler dumped, the tap's streamed frame is the SAME bytes
# (P7-6: the join key is qdump_xref.tsv, written by P when both fire on one tick). Takes the screen ~3 s + Seconds.
# Made with my soul - Swately <3
param(
  [Parameter(Mandatory=$true)][string]$Run,
  [int]$K = 4,
  [int]$Seconds = 20,
  [int]$Triples = 16,
  [string]$FgExe = "",
  [string]$Title = 'RA Motion Zoo',
  [switch]$Loop
)
$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Resolve-Path (Join-Path $here '..\..')
if (-not $FgExe) { $FgExe = Join-Path $root 'build-release\phyriad_fg.exe' }
$src = Join-Path $Run ("source_k{0}" -f $K)
if (-not (Test-Path (Join-Path $src 'manifest.txt'))) { throw "no $src\manifest.txt" }
$fps = [double]((Get-Content (Join-Path $src 'manifest.txt') | Where-Object { $_ -like 'fps *' }) -split '\s+')[1]
$xq = Join-Path $Run ("g4_qdump_k{0}" -f $K); $xg = Join-Path $Run ("g4_gdump_k{0}" -f $K)
foreach ($d in @($xq, $xg)) { if (Test-Path $d) { Remove-Item -Recurse -Force $d } }
New-Item -ItemType Directory -Force $xq | Out-Null
$q = { param($s) '"' + $s + '"' }
$pargs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (& $q (Join-Path $root 'tools\motion_truth\play_frames.ps1')),
  '-Dir', (& $q $src), '-Fps', $fps, '-Title', (& $q $Title), '-MaxFrames', 0)
if (-not $Loop) { $pargs += '-NoLoop' }
$player = Start-Process powershell -PassThru -ArgumentList $pargs
Start-Sleep -Seconds 3
$log = Join-Path $Run ("g4_k{0}.log" -f $K)
$proc = Start-Process $FgExe -PassThru -Wait -NoNewWindow -RedirectStandardOutput $log -RedirectStandardError (Join-Path $Run ("g4_k{0}.err" -f $K)) `
  -ArgumentList @('--window', (& $q $Title), '--qdump', (& $q $xq), $Triples, '--gdump', (& $q $xg), '--exit-after', $Seconds, '--fg-factor', $K)
if (-not $player.HasExited) { Stop-Process -Id $player.Id -Force -ErrorAction SilentlyContinue }
Get-Content $log | Select-String -Pattern 'gdump|qdump: wrote|done \(' | ForEach-Object { Write-Host ('[fg] ' + $_.Line) }
Write-Host ("[g4] FG exited {0}" -f $proc.ExitCode)
& python (Join-Path $here 'gdump_xcheck.py') --gdump $xg --qdump $xq
Write-Host ("[g4] xcheck exit {0} (0 = every joined pair byte-identical)" -f $LASTEXITCODE)
