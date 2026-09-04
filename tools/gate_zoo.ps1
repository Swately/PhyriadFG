# gate_zoo.ps1 — the artifact ZOO bench (operator-designed, 2026-06-12).
# Extends gate_motion.ps1's single ball into every confuser class named:
#   A  baseline yellow ball + a fine-stroke MOVING nameplate (letters that travel)
#   B  shaded ball — a specular highlight that MOVES inside the object (reflection/shading change)
#   FL flag — internal stripes SCROLL against the object's own motion (two motions per pixel)
#   ST sticker ball — hard internal color edge that travels COHERENT with the object (membership confuser)
#   SC scaling+pulsing ball — apparent size (perspective) + brightness (shadow) change per tick
#   static thin strokes (lines + crosshair) the movers cross
#   OPAQUE HUD (solid panel + text)  — the stasis-layer witness
#   TRANSLUCENT HUD (alpha panel + alpha text) — the known physics-limit witness (sad_zero>0)
# Same harness contract as gate_motion: borderless 1280x720 @ (120,120), title for --window,
# 33ms timer (~30fps source) by DEFAULT, DoubleBuffered.
# STAGE-82b (operator's updates question): -IntervalMs N sets the source rate — 16 ≈ 60fps source
# (each pair/ghost-transition lives a third as long; the real-game condition), 33 = the classic.
# Object velocities scale with the interval so on-screen speed stays comparable across rates.
# STAGE-84 (HSR repro): -Title / -W / -H parameterize the window so a SEPARATE load-test
# instance can coexist with the operator's anchored 'RA Gate Zoo'. ALL defaults are the
# original values (title, 1280x720) so the anchored zoo is byte-unaffected when launched bare.
param([int]$IntervalMs = 33, [string]$Title = 'RA Gate Zoo', [int]$W = 1280, [int]$H = 720, [int]$X = -1, [int]$Y = -1)
$vscale = [double]$IntervalMs / 33.0
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$f = New-Object Windows.Forms.Form
$f.Text = $Title
$f.FormBorderStyle = 'None'
$f.ClientSize = New-Object System.Drawing.Size($W,$H)
# GEOMETRY FIX (operator-found, 2026-06-13): the render_assistant present overlay covers the FULL
# present monitor at its ORIGIN (0,0) (main.cpp:4429 psd.width/height=0). A monitor-SIZED test window
# (e.g. 1920x1080) parked at the old default (120,120) sits off-origin + clips off-screen → the overlay
# paints the captured content at (0,0) while the window is at (120,120) → "desplazado" + "en partes".
# A real fullscreen game is at (0,0) and aligns perfectly. So: a monitor-sized zoo AUTO-SNAPS to (0,0)
# (the small artifact zoo keeps 120,120); -X/-Y override either way. The anchored 1280x720 zoo is
# byte-unaffected (X/Y default -1 → 120,120 because it is not monitor-sized).
if ($X -lt 0) { if ($W -ge 1920 -or $H -ge 1080) { $X = 0 } else { $X = 120 } }
if ($Y -lt 0) { if ($W -ge 1920 -or $H -ge 1080) { $Y = 0 } else { $Y = 120 } }
$f.StartPosition = 'Manual'; $f.Location = New-Object Drawing.Point($X,$Y)
$f.GetType().GetProperty("DoubleBuffered",[Reflection.BindingFlags]"Instance,NonPublic").SetValue($f,$true,$null)

$script:t  = 0
$script:a  = @{x=50.0;  y=50.0;  vx=11.0; vy=7.0;  r=120}    # baseline ball + nameplate
$script:b  = @{x=900.0; y=120.0; vx=-9.0; vy=6.0;  r=110}    # shaded/specular ball
$script:fl = @{x=300.0; y=430.0; vx=8.0;  vy=-5.0; w=150; h=90}  # scrolling-stripe flag
$script:st = @{x=700.0; y=500.0; vx=-7.0; vy=-9.0; r=100}    # sticker ball
$script:sc = @{x=200.0; y=250.0; vx=6.0;  vy=9.0;  r=75.0}   # scaling+pulsing ball
$script:mo = @{x=950.0; y=450.0; vx=10.0; vy=-8.0; r=120.0}  # MO — the EXTREME ball (all confusers in one)
$script:tq = @{x=450.0; y=150.0; vx=9.0; vy=-10.0; s=150}     # TQ — the TORTURE SQUARE v2 (operator-clarified):
                                                              # the TUNNEL — concentric square rings BORN at the
                                                              # center, expanding outward (2.5 px/tick). The
                                                              # diagonals split the square into 4 triangular
                                                              # faces: left/right = vertical segments sweeping
                                                              # horizontally outward; top/bottom = horizontal
                                                              # segments sweeping vertically — radial OUTWARD
                                                              # flow in all 4 directions inside ONE translating
                                                              # object: any face whose flow opposes the object
                                                              # velocity transits the cancellation state, and
                                                              # the center is a singularity (rings born at
                                                              # sub-block frequency).
$script:tx = @{x=180.0; y=480.0; vx=-8.0; vy=-7.0; s=150}     # TX — the SADDLE tunnel (operator-requested,
                                                              # LSFG round): VERTICAL faces flow INWARD (top
                                                              # face top→down, bottom face bottom→up = EXIT
                                                              # tunnel) while HORIZONTAL faces flow OUTWARD
                                                              # like TQ (entry tunnel) — OPPOSED radial flows
                                                              # in ONE object; tests the operator's "motion
                                                              # cancels the hallucination" observation: object
                                                              # velocity pushes different face-combinations
                                                              # into/out of the cancellation state. White/
                                                              # purple to distinguish from TQ's white/crimson.
$script:f2 = @{x=1000.0; y=80.0; vx=-6.0; vy=4.0; w=150; h=90}  # FLAG-2 — the DIRECTIONAL witness (operator's
                                                              # theory, 2026-06-12): VERTICAL stripes scrolling
                                                              # HORIZONTALLY at 6 px/tick, vx=±6 → the motion-
                                                              # cancellation degeneracy fires on HORIZONTAL
                                                              # bounces only (FL fires on vertical: scroll 5
                                                              # down, vy=±5 → vy=-5 ⇒ stripes screen-static).

# Static GDI resources (created once — per-frame allocations are disposed in Paint).
$fontHud   = New-Object Drawing.Font('Consolas',13,[Drawing.FontStyle]::Bold)
$fontName  = New-Object Drawing.Font('Consolas',10,[Drawing.FontStyle]::Regular)
$penThin   = New-Object Drawing.Pen([Drawing.Color]::FromArgb(150,150,160),1)
$penGrid   = New-Object Drawing.Pen([Drawing.Color]::FromArgb(110,110,135),1)   # FINE GRID minor (operator 2026-06-15): the static reference lattice the movers cross
$penGridM  = New-Object Drawing.Pen([Drawing.Color]::FromArgb(165,165,195),1)   # FINE GRID major (every 96px) — a brighter line for spatial reference
$brWhite   = [Drawing.Brushes]::White
$brGold    = [Drawing.Brushes]::Gold
$brCyan    = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(40,200,220))
$brMagenta = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(230,30,180))
$brRed     = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(200,30,30))
$brStripe  = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(240,240,240))
$brHudBg   = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(46,46,52))
$brGlassBg = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(90,80,140,255))   # translucent panel
$brGlassTx = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(150,255,255,255)) # translucent text
$brMoStripe= New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(120,255,255,255)) # MO: semi-transparent scrolling stripes
$brMoStick = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(60,230,60))       # MO: opaque sticker
$brF2Base  = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(30,140,60))       # FLAG-2 base (green)
$brTqB     = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(200,25,55))       # TQ dark sectors (crimson)
$brTxB     = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(140,60,220))      # TX dark bands (purple)

$f.Add_Paint({ param($s,$e)
  $g = $e.Graphics
  $g.Clear([System.Drawing.Color]::FromArgb(18,18,40))

  # static FINE GRID (operator 2026-06-15) — many thin static vertical+horizontal lines, the fixed
  # reference lattice the movers cross. Gravity (the disocclusion attraction) is OBVIOUS here: a grid
  # line near/behind a moving object inherits the object's MV (block-match bleed) and BENDS toward it,
  # standing out against the regular lattice. The grid is STATIC (MV=0 → the gme background model), so
  # any displacement of a line IS the artifact — no motion confound. Minor every 24px (1px, subtle);
  # a brighter major every 96px for spatial reference; a small center crosshair the movers cross.
  $cw = $f.ClientSize.Width; $ch = $f.ClientSize.Height
  for($gx = 0; $gx -le $cw; $gx += 24){ $g.DrawLine($penGrid, [float]$gx, 0, [float]$gx, [float]$ch) }
  for($gy = 0; $gy -le $ch; $gy += 24){ $g.DrawLine($penGrid, 0, [float]$gy, [float]$cw, [float]$gy) }
  for($gx = 0; $gx -le $cw; $gx += 96){ $g.DrawLine($penGridM, [float]$gx, 0, [float]$gx, [float]$ch) }
  for($gy = 0; $gy -le $ch; $gy += 96){ $g.DrawLine($penGridM, 0, [float]$gy, [float]$cw, [float]$gy) }
  $g.DrawLine($penThin, [float]($cw/2-6), [float]($ch/2-6), [float]($cw/2+6), [float]($ch/2+6))
  $g.DrawLine($penThin, [float]($cw/2+6), [float]($ch/2-6), [float]($cw/2-6), [float]($ch/2+6))

  # A — baseline ball + moving nameplate (fine strokes that travel with the object)
  $g.FillEllipse($brGold, [float]$script:a.x, [float]$script:a.y, $script:a.r, $script:a.r)
  $g.DrawString("PLAYER_01", $fontName, $brWhite, [float]($script:a.x+18), [float]($script:a.y-20))

  # B — shaded ball: PathGradient highlight whose center ORBITS inside the object per tick
  $gp = New-Object Drawing.Drawing2D.GraphicsPath
  $gp.AddEllipse([float]$script:b.x, [float]$script:b.y, $script:b.r, $script:b.r)
  $pgb = New-Object Drawing.Drawing2D.PathGradientBrush($gp)
  $cx = $script:b.x + $script:b.r/2 + 28*[Math]::Cos($script:t*0.11)
  $cy = $script:b.y + $script:b.r/2 + 28*[Math]::Sin($script:t*0.11)
  $pgb.CenterPoint = New-Object Drawing.PointF([float]$cx,[float]$cy)
  $pgb.CenterColor = [Drawing.Color]::FromArgb(255,250,230)
  $pgb.SurroundColors = @([Drawing.Color]::FromArgb(190,90,20))
  $g.FillPath($pgb, $gp)
  $pgb.Dispose(); $gp.Dispose()

  # FL — flag: base red rect + white stripes whose PHASE scrolls (internal motion != object motion)
  $g.FillRectangle($brRed, [float]$script:fl.x, [float]$script:fl.y, $script:fl.w, $script:fl.h)
  $period = 24; $band = 10
  $phase = ($script:t * 5) % $period
  for($yy = -$period; $yy -lt $script:fl.h; $yy += $period){
    $sy = $script:fl.y + $yy + $phase
    $top = [Math]::Max($sy, $script:fl.y); $bot = [Math]::Min($sy+$band, $script:fl.y+$script:fl.h)
    if($bot -gt $top){ $g.FillRectangle($brStripe, [float]$script:fl.x, [float]$top, $script:fl.w, [float]($bot-$top)) }
  }

  # TQ — the TORTURE SQUARE v2: the TUNNEL. Concentric square rings (Chebyshev metric — lines
  # stay parallel to the square's edges, so the 4 triangular faces between the diagonals each
  # show segments sweeping outward perpendicular to their face). Rings expand at 2.5 px/tick and
  # are reborn at the center; ring color is tied to its ABSOLUTE index (j + wrap count) so the
  # alternation never pops at the wrap. Painted outside-in (full square = the beyond-outermost
  # ring color, then rings largest→smallest on top); the clip crops overgrown rings.
  $tqs = $script:tq.s
  $tqRect = New-Object Drawing.RectangleF([float]$script:tq.x, [float]$script:tq.y, [float]$tqs, [float]$tqs)
  $g.SetClip($tqRect)
  $tqcx = $script:tq.x + $tqs/2.0; $tqcy = $script:tq.y + $tqs/2.0
  $tqP = 18.0
  $tqTp = $script:t * 2.5
  $tqBase = [int][Math]::Floor($tqTp / $tqP)
  $tqPh = $tqTp % $tqP
  $obr = if((5 + $tqBase) % 2 -eq 0){ $brWhite } else { $brTqB }
  $g.FillRectangle($obr, [float]$script:tq.x, [float]$script:tq.y, [float]$tqs, [float]$tqs)
  for($rj = 4; $rj -ge 0; $rj--){
    $hs = $rj * $tqP + $tqPh
    $rbr = if(($rj + $tqBase) % 2 -eq 0){ $brWhite } else { $brTqB }
    $g.FillRectangle($rbr, [float]($tqcx-$hs), [float]($tqcy-$hs), [float](2*$hs), [float](2*$hs))
  }
  $g.ResetClip()

  # TX — the SADDLE tunnel: 4 triangular faces (the diagonals), each striped independently.
  # Horizontal faces (left/right): bands flow OUTWARD (+1, like TQ). Vertical faces (top/
  # bottom): bands flow INWARD (-1, the exit tunnel). Band color rides its ABSOLUTE index m
  # (parity), so colors travel WITH the bands — no popping at birth/death. Clip = face polygon.
  $txs = $script:tx.s
  $txcx = $script:tx.x + $txs/2.0; $txcy = $script:tx.y + $txs/2.0
  $txP = 18.0; $txTp = $script:t * 2.5; $txH = $txs/2.0
  $ptC  = New-Object Drawing.PointF([float]$txcx,[float]$txcy)
  $ptTL = New-Object Drawing.PointF([float]$script:tx.x,[float]$script:tx.y)
  $ptTR = New-Object Drawing.PointF([float]($script:tx.x+$txs),[float]$script:tx.y)
  $ptBL = New-Object Drawing.PointF([float]$script:tx.x,[float]($script:tx.y+$txs))
  $ptBR = New-Object Drawing.PointF([float]($script:tx.x+$txs),[float]($script:tx.y+$txs))
  $txFaces = @(
    @{dir=-1.0; ax='y-'; poly=[Drawing.PointF[]]@($ptTL,$ptTR,$ptC)},
    @{dir=-1.0; ax='y+'; poly=[Drawing.PointF[]]@($ptBL,$ptBR,$ptC)},
    @{dir= 1.0; ax='x-'; poly=[Drawing.PointF[]]@($ptTL,$ptBL,$ptC)},
    @{dir= 1.0; ax='x+'; poly=[Drawing.PointF[]]@($ptTR,$ptBR,$ptC)} )
  foreach($fc in $txFaces){
    $fp = New-Object Drawing.Drawing2D.GraphicsPath
    $fp.AddPolygon($fc.poly)
    $g.SetClip($fp)
    $s0 = $fc.dir * $txTp
    $mLo = [int][Math]::Floor((0.0 - $s0)/$txP) - 1
    $mHi = [int][Math]::Ceiling(($txH - $s0)/$txP) + 1
    for($m = $mLo; $m -le $mHi; $m++){
      $d0 = $s0 + $m*$txP; $d1 = $d0 + $txP
      $da = [Math]::Max(0.0,$d0); $db = [Math]::Min([double]$txH,$d1)
      if($db -le $da){ continue }
      $fbr = if(((($m % 2)+2) % 2) -eq 0){ $brWhite } else { $brTxB }
      switch($fc.ax){
        'y-' { $g.FillRectangle($fbr, [float]$script:tx.x, [float]($txcy-$db), [float]$txs, [float]($db-$da)) }
        'y+' { $g.FillRectangle($fbr, [float]$script:tx.x, [float]($txcy+$da), [float]$txs, [float]($db-$da)) }
        'x-' { $g.FillRectangle($fbr, [float]($txcx-$db), [float]$script:tx.y, [float]($db-$da), [float]$txs) }
        'x+' { $g.FillRectangle($fbr, [float]($txcx+$da), [float]$script:tx.y, [float]($db-$da), [float]$txs) }
      }
    }
    $g.ResetClip(); $fp.Dispose()
  }

  # FLAG-2 — the DIRECTIONAL witness: VERTICAL stripes, HORIZONTAL scroll (6 px/tick), vx=±6.
  # Prediction under test: hallucinations fire on HORIZONTAL bounces only (the 90°-rotated
  # twin of FL's vertical degeneracy). Confirms/refutes direction-follows-degeneracy-axis.
  $g.FillRectangle($brF2Base, [float]$script:f2.x, [float]$script:f2.y, $script:f2.w, $script:f2.h)
  $p2 = 24; $b2 = 10
  $ph2 = ($script:t * 6) % $p2
  for($xx = -$p2; $xx -lt $script:f2.w; $xx += $p2){
    $sx = $script:f2.x + $xx + $ph2
    $lft = [Math]::Max($sx, $script:f2.x); $rgt = [Math]::Min($sx+$b2, $script:f2.x+$script:f2.w)
    if($rgt -gt $lft){ $g.FillRectangle($brStripe, [float]$lft, [float]$script:f2.y, [float]($rgt-$lft), $script:f2.h) }
  }

  # ST — sticker ball: hard internal color edge traveling coherent with the object
  $g.FillEllipse($brCyan, [float]$script:st.x, [float]$script:st.y, $script:st.r, $script:st.r)
  $g.FillRectangle($brMagenta, [float]($script:st.x+$script:st.r/2+8), [float]($script:st.y+$script:st.r/2-26), 34, 34)

  # SC — scaling (perspective) + brightness-pulsing (shadow) ball
  $rr = 75 + 45*[Math]::Sin($script:t*0.05)
  $script:sc.r = $rr
  $lum = [int](120 + 80*[Math]::Sin($script:t*0.08))
  $bsc = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(60, $lum, 90))
  $g.FillEllipse($bsc, [float]$script:sc.x, [float]$script:sc.y, [float]$rr, [float]$rr)
  $bsc.Dispose()

  # MO — the EXTREME ball (operator-requested): ALL confusers in ONE object. Scaling
  # (perspective) + pulsing surround color (shadow) + ORBITING specular highlight (reflection)
  # + internal SCROLLING semi-transparent stripes clipped to the silhouette (two motions per
  # pixel, on top of the gradient) + an opaque sticker traveling coherent (hard internal edge)
  # + a moving fine-stroke nameplate. The interaction case: every appearance-change mechanism
  # at once on a single silhouette the matte must still hold.
  $mr = 110 + 40*[Math]::Sin($script:t*0.045)
  $script:mo.r = $mr
  $mlum = [int](110 + 70*[Math]::Sin($script:t*0.07))
  $mgp = New-Object Drawing.Drawing2D.GraphicsPath
  $mgp.AddEllipse([float]$script:mo.x, [float]$script:mo.y, [float]$mr, [float]$mr)
  $mpgb = New-Object Drawing.Drawing2D.PathGradientBrush($mgp)
  $mcx = $script:mo.x + $mr/2 + ($mr*0.22)*[Math]::Cos($script:t*0.13)
  $mcy = $script:mo.y + $mr/2 + ($mr*0.22)*[Math]::Sin($script:t*0.13)
  $mpgb.CenterPoint = New-Object Drawing.PointF([float]$mcx,[float]$mcy)
  $mpgb.CenterColor = [Drawing.Color]::FromArgb(255,255,235)
  $mpgb.SurroundColors = @([Drawing.Color]::FromArgb($mlum, 40, 170))
  $g.FillPath($mpgb, $mgp)
  $g.SetClip($mgp)                                  # stripes + sticker stay INSIDE the silhouette
  $mphase = ($script:t * 6) % 28
  for($yy = -28; $yy -lt $mr; $yy += 28){
    $g.FillRectangle($brMoStripe, [float]$script:mo.x, [float]($script:mo.y + $yy + $mphase), [float]$mr, 9)
  }
  $g.FillRectangle($brMoStick, [float]($script:mo.x+$mr*0.62), [float]($script:mo.y+$mr*0.30), 26, 26)
  $g.ResetClip()
  $g.DrawString("BOSS_X", $fontName, $brWhite, [float]($script:mo.x+$mr/2-24), [float]($script:mo.y-20))
  $mpgb.Dispose(); $mgp.Dispose()

  # TRANSLUCENT HUD (drawn over the movers — the alpha physics-limit witness)
  $g.FillRectangle($brGlassBg, 430, 600, 420, 90)
  $g.DrawString("TRANSLUCENT HUD  12:34", $fontHud, $brGlassTx, 470, 630)

  # OPAQUE HUD (solid panel + fine-stroke letters — the stasis witness)
  $g.FillRectangle($brHudBg, 10, 10, 290, 64)
  $g.DrawString("HP 100   AMMO 42", $fontHud, $brWhite, 20, 16)
  $g.DrawString("SCORE 003417",     $fontHud, $brWhite, 20, 42)
})

# NOTE: the timer variable must NOT be named $t — a top-level .ps1 assignment lands in the
# script scope, so $t here would CLOBBER $script:t (the phase counter all animations read).
# Caught live by the operator (op_Multiply errors on every Paint/Tick at 30Hz).
$timer = New-Object Windows.Forms.Timer
$timer.Interval = $IntervalMs
# STAGE-82b: scale per-tick velocities so on-screen px/second stays constant across source rates
# (a 16ms source ticks 2x as often → halve the per-tick step). The internal texture phases (t-driven)
# scale implicitly with tick count; that changes their px/s — acceptable for the fluidity test, whose
# witnesses are the BALLS and the nameplate.
foreach($o in @($script:a,$script:b,$script:fl,$script:st,$script:sc,$script:mo,$script:tx,$script:f2)){
  $o.vx = $o.vx * $vscale; $o.vy = $o.vy * $vscale
}
$timer.Add_Tick({
  $script:t += 1
  foreach($o in @($script:a, $script:b, $script:st)){
    $o.x += $o.vx; $o.y += $o.vy
    if($o.x -lt 0 -or $o.x -gt ($f.ClientSize.Width  - $o.r)){ $o.vx = -$o.vx }
    if($o.y -lt 0 -or $o.y -gt ($f.ClientSize.Height - $o.r)){ $o.vy = -$o.vy }
  }
  $script:fl.x += $script:fl.vx; $script:fl.y += $script:fl.vy
  if($script:fl.x -lt 0 -or $script:fl.x -gt ($f.ClientSize.Width  - $script:fl.w)){ $script:fl.vx = -$script:fl.vx }
  if($script:fl.y -lt 0 -or $script:fl.y -gt ($f.ClientSize.Height - $script:fl.h)){ $script:fl.vy = -$script:fl.vy }
  $script:sc.x += $script:sc.vx; $script:sc.y += $script:sc.vy
  if($script:sc.x -lt 0 -or $script:sc.x -gt ($f.ClientSize.Width  - $script:sc.r)){ $script:sc.vx = -$script:sc.vx }
  if($script:sc.y -lt 0 -or $script:sc.y -gt ($f.ClientSize.Height - $script:sc.r)){ $script:sc.vy = -$script:sc.vy }
  $script:mo.x += $script:mo.vx; $script:mo.y += $script:mo.vy
  if($script:mo.x -lt 0 -or $script:mo.x -gt ($f.ClientSize.Width  - $script:mo.r)){ $script:mo.vx = -$script:mo.vx }
  if($script:mo.y -lt 0 -or $script:mo.y -gt ($f.ClientSize.Height - $script:mo.r)){ $script:mo.vy = -$script:mo.vy }
  $script:f2.x += $script:f2.vx; $script:f2.y += $script:f2.vy
  if($script:f2.x -lt 0 -or $script:f2.x -gt ($f.ClientSize.Width  - $script:f2.w)){ $script:f2.vx = -$script:f2.vx }
  if($script:f2.y -lt 0 -or $script:f2.y -gt ($f.ClientSize.Height - $script:f2.h)){ $script:f2.vy = -$script:f2.vy }
  $script:tq.x += $script:tq.vx; $script:tq.y += $script:tq.vy
  if($script:tq.x -lt 0 -or $script:tq.x -gt ($f.ClientSize.Width  - $script:tq.s)){ $script:tq.vx = -$script:tq.vx }
  if($script:tq.y -lt 0 -or $script:tq.y -gt ($f.ClientSize.Height - $script:tq.s)){ $script:tq.vy = -$script:tq.vy }
  $script:tx.x += $script:tx.vx; $script:tx.y += $script:tx.vy
  if($script:tx.x -lt 0 -or $script:tx.x -gt ($f.ClientSize.Width  - $script:tx.s)){ $script:tx.vx = -$script:tx.vx }
  if($script:tx.y -lt 0 -or $script:tx.y -gt ($f.ClientSize.Height - $script:tx.s)){ $script:tx.vy = -$script:tx.vy }
  $f.Invalidate()
})
$timer.Start()
[System.Windows.Forms.Application]::Run($f)
# Made with my soul - Swately <3
