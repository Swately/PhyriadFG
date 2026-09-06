# r5_pressure.ps1 — G-R5's missing run: PhyriadFG under REAL GPU saturation, so the CONTROL-plane branches the ball
# zoo alone never reaches are exercised — the pressure-tier ladder and, with it, the `HOLON` (tier < 4, !holon_skip)
# and `BIDIR_OK` (tier < 5) arms the FLOW rows carry since R5 step 3b, plus the bwd-skip hysteresis.
#
# The load source is the SIBLING project's stability tool, not tools/gpu_load.exe: measured 2026-09-06,
# gpu_load.exe lifts the 4090 to 32-35 % utilisation (records/R4_GATE.md s4.6.1) while
# projects/gpu_oc/escalera_arbiter.exe --profile heavy holds 96-100 % at 355-361 W and ~10.7 GB of VRAM. The
# arbiter is an OCCT-class tool (six detectors: compute self-compare, an a-priori integer oracle, a graphics-pipeline
# self-compare, a 9 GB VRAM pattern oracle, a cross-thread race detector, a power-virus phase); here it is used ONLY
# as the load, but its own verdict is captured too - if it reports anything other than STABLE the run is not a valid
# FG measurement, it is a GPU fault, and the record must say so.
#
# What the gate reads from the output: (1) the run exits clean; (2) `gov-floor ENGAGE tier:N` appears, i.e. the ladder
# actually engaged; (3) both two-oracle instruments still report 0 mismatches - the rows and the transport agree with
# the former hand conditions WITH the shedding branches live; (4) the arbiter's verdict.
# Made with my soul - Swately <3
param([int]$Seconds = 45, [string]$Profile = 'heavy', [int]$ZooFps = 60)
# ZooFps raises the SOURCE rate, which shrinks the F thread's per-pair budget (pair_budget_ms = src_interval_us/1000)
# and is the lever that actually moves the pressure tier: the ladder compares t_pair_ema against that budget
# (flow.cpp), so GPU saturation alone does not raise it - it raises the GPU legs F waits on, which is only part of t_pair.
$root = 'F:\Phyriad\projects\PhyriadFG'
$arb  = 'F:\Phyriad\projects\gpu_oc\escalera_arbiter.exe'
$out  = Join-Path $PSScriptRoot 'r5'
New-Item -ItemType Directory -Force $out | Out-Null
$exe = Join-Path $root 'build-release\phyriad_fg.exe'
$zooScript = Join-Path $root 'tools\ball_zoo.ps1'
if (-not (Test-Path $arb)) { "MISSING: $arb (build it with projects\gpu_oc\build_arbiter.bat)"; exit 1 }

$tag = "pressure_${Profile}_${ZooFps}fps"
$zoo = Start-Process powershell -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',$zooScript,'-Fps',"$ZooFps",'-W','1920','-H','1080','-X','0','-Y','0' -PassThru
Start-Sleep -Seconds 4
$arbLog = Join-Path $out ($tag + '_arbiter.log')
$arbProc = Start-Process $arb -ArgumentList '--secs',"$($Seconds + 20)",'--profile',$Profile,'--gpu','0' -PassThru -RedirectStandardOutput $arbLog -WindowStyle Minimized
Start-Sleep -Seconds 6            # let the load ramp and the 9 GB allocation settle before the FG starts measuring
$csv = Join-Path $out ($tag + '.csv'); $log = Join-Path $out ($tag + '.log')
& $exe '--window' 'RA Ball Zoo' '--exit-after' "$Seconds" '--csv' $csv '--warp-timing' > $log 2>&1
$rc = $LASTEXITCODE
Stop-Process -Id $zoo.Id -Force -ErrorAction SilentlyContinue
$arbProc.WaitForExit(40000) | Out-Null
if (-not $arbProc.HasExited) { Stop-Process -Id $arbProc.Id -Force -ErrorAction SilentlyContinue }

"=== $tag : rc=$rc ==="
Select-String -Path $log -Pattern 'bounded-run clean exit' | Select-Object -Last 1 | ForEach-Object { $_.Line }
Select-String -Path $log -Pattern 'gov-floor ENGAGE' | Group-Object { $_.Line -replace '.*(tier:\d+).*','$1' } | ForEach-Object { "  governor: $($_.Name) x$($_.Count)" }
Select-String -Path $log -Pattern 'flow rows resolved|flow rows vs the hand|transport rows vs the hand' | ForEach-Object { "  " + $_.Line }
Select-String -Path $log -Pattern 'fps \(present\)' | Select-Object -Last 1 | ForEach-Object { "  " + $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
Select-String -Path $arbLog -Pattern 'RESULT verdict' | ForEach-Object { "  arbiter: " + $_.Line.Substring(0, [Math]::Min(150, $_.Line.Length)) }
"=== done ==="
