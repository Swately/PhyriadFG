# ball_zoo.ps1 — single-ball cadence bench (operator-requested, 2026-07-03).
# ONE ball moving horizontally left->right at a configurable speed, on the SAME background as
# gate_zoo.ps1 (dark navy + fine static grid 24px minor / 96px major + center crosshair): the
# minimal witness for cadence/pacing forensics — one object, one axis, constant velocity.
#
# HIGH-FPS DESIGN (the reason this is not a gate_zoo edit): Windows.Forms.Timer has ~15.6ms
# granularity (ceiling ~64fps). Here a Stopwatch-paced render loop + winmm timeBeginPeriod(1)
# + BufferedGraphics drives the paint directly — no WM_PAINT round-trip — so 240/480fps source
# rates are reachable. The background is pre-rendered ONCE into a bitmap (per-frame cost =
# 1 blit + 1 antialiased ellipse). The ball is drawn ANTIALIASED on purpose: at high fps the
# per-frame step is sub-pixel (e.g. 330px/s @ 480fps = 0.69px) and without AA consecutive
# frames would be byte-identical -> the capture dedup would collapse them and the "source"
# would lie. AA edge coverage changes every frame, so every frame is unique.
#
# Motion contract: FIXED step per rendered frame (SpeedPx/Fps), not wall-time-based — each
# source frame differs by exactly the same displacement (the uniform-step gold standard for
# FG cadence tests). If the loop can't hold the target, fps prints tell you honestly.
#
#   -Fps 60        target source rate (accepts high values; achieved rate printed every 1s)
#   -SpeedPx 330   horizontal speed in px/SECOND (fps-independent on-screen speed)
#   -Size 120      ball diameter (same as gate_zoo's baseline ball A)
#   -Bounce        ping-pong at the edges instead of the default wrap (pure left->right)
#   -Title/-W/-H/-X/-Y   same harness contract as gate_zoo (borderless, 1280x720 @ 120,120;
#                        monitor-sized windows auto-snap to 0,0 — same geometry fix)
# Esc closes the window.
param(
  [double]$Fps = 60,
  [double]$SpeedPx = 330,
  [int]$Size = 120,
  [switch]$Bounce,
  [double]$PanPx = 0,   # -PanPx S: the BACKGROUND lattice scrolls left at S px/s (camera-pan analog) while a
                        # screen-fixed HUD (opaque panel + translucent panel + crosshair) stays put — the
                        # static-overlay-over-moving-world witness (the HSR HUD-ghosting reproduction case).
  [string]$Title = 'RA Ball Zoo',
  [int]$W = 1280, [int]$H = 720, [int]$X = -1, [int]$Y = -1
)
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type -Name WinMM -Namespace BZ -MemberDefinition @'
[DllImport("winmm.dll")] public static extern uint timeBeginPeriod(uint p);
[DllImport("winmm.dll")] public static extern uint timeEndPeriod(uint p);
'@

$f = New-Object Windows.Forms.Form
$f.Text = $Title
$f.FormBorderStyle = 'None'
$f.ClientSize = New-Object System.Drawing.Size($W,$H)
# GEOMETRY FIX inherited from gate_zoo: monitor-sized windows snap to (0,0) so the FG overlay
# (which covers the present monitor at its origin) aligns; the small bench stays at (120,120).
if ($X -lt 0) { if ($W -ge 1920 -or $H -ge 1080) { $X = 0 } else { $X = 120 } }
if ($Y -lt 0) { if ($W -ge 1920 -or $H -ge 1080) { $Y = 0 } else { $Y = 120 } }
$f.StartPosition = 'Manual'; $f.Location = New-Object Drawing.Point($X,$Y)
$f.GetType().GetProperty("DoubleBuffered",[Reflection.BindingFlags]"Instance,NonPublic").SetValue($f,$true,$null)
$f.Add_KeyDown({ param($s,$e) if($e.KeyCode -eq 'Escape'){ $f.Close() } })

# --- The gate_zoo background, pre-rendered once (identical colors/geometry to gate_zoo.ps1) ---
$penThin  = New-Object Drawing.Pen([Drawing.Color]::FromArgb(150,150,160),1)
$penGrid  = New-Object Drawing.Pen([Drawing.Color]::FromArgb(110,110,135),1)   # FINE GRID minor
$penGridM = New-Object Drawing.Pen([Drawing.Color]::FromArgb(165,165,195),1)   # FINE GRID major (every 96px)
# The lattice is 96px-periodic → a W+96-wide cached tile pans at constant cost (one DrawImage at
# -(pan mod 96)). The crosshair joins the HUD (screen-fixed) when panning, the lattice when not.
$bgW = $W + 96
$bg = New-Object Drawing.Bitmap($bgW,$H)
$gb = [Drawing.Graphics]::FromImage($bg)
$gb.Clear([Drawing.Color]::FromArgb(18,18,40))
for($gx = 0; $gx -le $bgW; $gx += 24){ $gb.DrawLine($penGrid, [float]$gx, 0, [float]$gx, [float]$H) }
for($gy = 0; $gy -le $H; $gy += 24){ $gb.DrawLine($penGrid, 0, [float]$gy, [float]$bgW, [float]$gy) }
for($gx = 0; $gx -le $bgW; $gx += 96){ $gb.DrawLine($penGridM, [float]$gx, 0, [float]$gx, [float]$H) }
for($gy = 0; $gy -le $H; $gy += 96){ $gb.DrawLine($penGridM, 0, [float]$gy, [float]$W, [float]$gy) }
if($PanPx -le 0){
  $gb.DrawLine($penThin, [float]($W/2-6), [float]($H/2-6), [float]($W/2+6), [float]($H/2+6))
  $gb.DrawLine($penThin, [float]($W/2+6), [float]($H/2-6), [float]($W/2-6), [float]($H/2+6))
}
$gb.Dispose()
# Screen-fixed HUD resources (pan mode only): the opaque stasis witness + the translucent
# physics-limit witness + fine strokes — gate_zoo's HUD pair, here over a MOVING world.
$fontHud   = New-Object Drawing.Font('Consolas',13,[Drawing.FontStyle]::Bold)
$brWhite   = [Drawing.Brushes]::White
$brHudBg   = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(46,46,52))
$brGlassBg = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(90,80,140,255))
$brGlassTx = New-Object Drawing.SolidBrush([Drawing.Color]::FromArgb(150,255,255,255))

$brGold = [Drawing.Brushes]::Gold
$step = $SpeedPx / $Fps          # px per rendered frame (uniform-step contract)
$bx = [double](-$Size)           # wrap mode: enter from the left edge
if($Bounce){ $bx = 0.0 }
$vdir = 1.0
$by = [double](($H - $Size) / 2.0)

$f.Show(); $f.Activate()

# --- Stopwatch-paced render loop (sleep(1) far out, spin the tail; catch-up resync on falls) ---
[void][BZ.WinMM]::timeBeginPeriod(1)
$ctx = [Drawing.BufferedGraphicsManager]::Current
$ctx.MaximumBuffer = New-Object Drawing.Size(($W+1),($H+1))
$gScreen = $f.CreateGraphics()
$buf = $ctx.Allocate($gScreen, (New-Object Drawing.Rectangle(0,0,$W,$H)))
$g = $buf.Graphics
$g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias

$sw = [Diagnostics.Stopwatch]::StartNew()
$freq = [double][Diagnostics.Stopwatch]::Frequency
$periodTicks = [long]($freq / $Fps)
$next = $sw.ElapsedTicks + $periodTicks
$frames = 0; $statT0 = $sw.ElapsedTicks
try {
  $panAccum = 0.0
  $panStep = if($Fps -gt 0){ $PanPx / $Fps } else { 0.0 }   # px per rendered frame (uniform-step contract)
  while(-not $f.IsDisposed){
    if($PanPx -gt 0){
      $panOff = [int]([Math]::Floor($panAccum)) % 96
      $g.DrawImageUnscaled($bg, -$panOff, 0)
      $panAccum += $panStep
    } else {
      $g.DrawImageUnscaled($bg, 0, 0)
    }
    if($Size -gt 0){ $g.FillEllipse($brGold, [float]$bx, [float]$by, [float]$Size, [float]$Size) }
    if($PanPx -gt 0){
      # Screen-fixed HUD over the moving world: opaque panel + strokes, translucent panel, crosshair.
      $g.FillRectangle($brHudBg, 10, 10, 290, 64)
      $g.DrawString("HP 100   AMMO 42", $fontHud, $brWhite, 20, 16)
      $g.DrawString("SCORE 003417",     $fontHud, $brWhite, 20, 42)
      $g.FillRectangle($brGlassBg, 430, 600, 420, 90)
      $g.DrawString("TRANSLUCENT HUD  12:34", $fontHud, $brGlassTx, 470, 630)
      $g.DrawLine($penThin, [float]($W/2-6), [float]($H/2-6), [float]($W/2+6), [float]($H/2+6))
      $g.DrawLine($penThin, [float]($W/2+6), [float]($H/2-6), [float]($W/2-6), [float]($H/2+6))
    }
    $buf.Render()
    $frames++

    if($Bounce){
      $bx += $vdir * $step
      if($bx -ge ($W - $Size)){ $bx = $W - $Size; $vdir = -1.0 }
      elseif($bx -le 0){ $bx = 0; $vdir = 1.0 }
    } else {
      $bx += $step
      if($bx -gt $W){ $bx = -$Size }   # wrap: re-enter from the left (pure left->right)
    }

    [Windows.Forms.Application]::DoEvents()
    if($f.IsDisposed){ break }

    $nowT = $sw.ElapsedTicks
    if($nowT -gt $next + 4*$periodTicks){ $next = $nowT }   # fell behind: resync, don't spiral
    while($sw.ElapsedTicks -lt $next){
      $remMs = ($next - $sw.ElapsedTicks) * 1000.0 / $freq
      if($remMs -gt 2.0){ [Threading.Thread]::Sleep(1) } else { [Threading.Thread]::SpinWait(60) }
    }
    $next += $periodTicks

    if(($sw.ElapsedTicks - $statT0) -ge $freq){
      $ach = $frames * $freq / [double]($sw.ElapsedTicks - $statT0)
      Write-Host ("[ball-zoo] fps={0:N1} target={1:N0} step={2:N2}px" -f $ach, $Fps, $step)
      $frames = 0; $statT0 = $sw.ElapsedTicks
    }
  }
} finally {
  [void][BZ.WinMM]::timeEndPeriod(1)
  $buf.Dispose(); $gScreen.Dispose(); $bg.Dispose()
  $penThin.Dispose(); $penGrid.Dispose(); $penGridM.Dispose()
  $fontHud.Dispose(); $brHudBg.Dispose(); $brGlassBg.Dispose(); $brGlassTx.Dispose()
}
