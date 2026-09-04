# play_frames.ps1 — present a marker_zoo sequence on screen at a fixed cadence (MOTION_TRUTH phase T3).
#
# WHY IT EXISTS. `marker_zoo.py` writes ground truth to disk; the FG captures a WINDOW. Until the
# frames are on screen the zoo is a file writer with a passing self-test, not an instrument. This is
# the piece that closes that gap: it puts the sequence in front of the capture path exactly the way a
# game would, so every stage downstream — capture, dedup, flow, the warp, `--qdump+` — sees the zoo as
# ordinary content and nothing has to be special-cased for measurement.
#
# IT MUST NOT HIDE FROM CAPTURE. No WDA_EXCLUDEFROMCAPTURE, borderless, ordinary window — the June
# TB-C1 note. A window the FG cannot capture measures nothing.
#
# THE PACING IS BORROWED, NOT REINVENTED. Windows.Forms.Timer has ~15.6 ms granularity (a ~64 fps
# ceiling). This reuses `ball_zoo.ps1`'s proven loop — Stopwatch deadlines + winmm timeBeginPeriod(1)
# + BufferedGraphics, sleep(1) far from the deadline and spin the tail, resync rather than spiral when
# a tick is missed — because that loop is already measured to hold 60 and to reach far past it.
#
# THE FRAMES ARE PRELOADED, and that is a real bound: 1280x720 x 4 B = 3,686,400 B per frame, so 120
# frames is 442 MB of managed heap. -MaxFrames caps it, and the achieved-rate print is what tells you
# whether the cap or the machine is the limit — not an assumption.
#
#   powershell -File tools/motion_truth/play_frames.ps1 -Dir DIR [-Fps 60] [-MaxFrames 120]
#
# Esc closes the window. Made with my soul - Swately <3
param(
  [Parameter(Mandatory=$true)][string]$Dir,
  [double]$Fps = 60,
  [int]$MaxFrames = 0,                    # 0 = the whole sequence
  [string]$Title = 'RA Motion Zoo',
  [int]$X = -1, [int]$Y = -1,
  [switch]$NoLoop
)
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
Add-Type -Name WinMM -Namespace MZ -MemberDefinition @'
[DllImport("winmm.dll")] public static extern uint timeBeginPeriod(uint p);
[DllImport("winmm.dll")] public static extern uint timeEndPeriod(uint p);
'@

# The RGBA -> BGRA swap and the Bitmap fill happen in compiled code. Per-pixel PowerShell over
# 921,600 pixels x N frames would cost minutes at load and is the difference between a tool that runs
# and one that does not. The file is R,G,B,A per pixel; GDI+ Format32bppRgb holds B,G,R,X in memory.
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
public static class MzLoad {
  public static Bitmap Rgba(string path, int w, int h) {
    byte[] src = File.ReadAllBytes(path);
    if (src.Length != w * h * 4) throw new Exception(path + ": " + src.Length + " B, expected " + (w*h*4));
    for (int i = 0; i < src.Length; i += 4) { byte r = src[i]; src[i] = src[i+2]; src[i+2] = r; }
    Bitmap bmp = new Bitmap(w, h, PixelFormat.Format32bppRgb);
    BitmapData bd = bmp.LockBits(new Rectangle(0,0,w,h), ImageLockMode.WriteOnly, PixelFormat.Format32bppRgb);
    if (bd.Stride == w * 4) {
      Marshal.Copy(src, 0, bd.Scan0, src.Length);
    } else {
      for (int y = 0; y < h; y++)
        Marshal.Copy(src, y * w * 4, new IntPtr(bd.Scan0.ToInt64() + (long)y * bd.Stride), w * 4);
    }
    bmp.UnlockBits(bd);
    return bmp;
  }
}
'@ -ReferencedAssemblies System.Drawing

# --- the manifest is the contract: the sequence describes itself -------------------------------
$manPath = Join-Path $Dir 'manifest.txt'
if(-not (Test-Path $manPath)){ Write-Host "[zoo-play] no manifest.txt in $Dir"; exit 1 }
$W = 0; $H = 0; $T = 0; $prefix = 'f_'; $srcFps = 0.0
foreach($line in Get-Content $manPath){
  $p = $line.Trim() -split '\s+'
  if($p[0] -eq 'size'){ $W = [int]$p[1]; $H = [int]$p[2] }
  elseif($p[0] -eq 'sequence'){ $prefix = $p[1]; $T = [int]$p[2] }
  elseif($p[0] -eq 'fps'){ $srcFps = [double]$p[1] }
}
if($W -le 0 -or $T -le 0){ Write-Host '[zoo-play] manifest lacks size / sequence'; exit 1 }
$n = if($MaxFrames -gt 0 -and $MaxFrames -lt $T){ $MaxFrames } else { $T }
if($srcFps -gt 0 -and [Math]::Abs($srcFps - $Fps) -gt 0.01){
  # Not an error: replaying a 60 fps sequence at 120 halves the per-frame step, which is a legitimate
  # experiment. But the ANALYTIC p(t) is parameterised in seconds, so the extractor must be told, and
  # silence here would corrupt every position it computes.
  Write-Host "[zoo-play] NOTE: the sequence was generated at $srcFps fps and is being played at $Fps."
  Write-Host "[zoo-play]       p(t) is in SECONDS -- the extractor must use the PLAYBACK rate, not the manifest's."
}

Write-Host "[zoo-play] $Dir : ${W}x${H}, $n of $T frames, target $Fps fps"
$mb = [Math]::Round($n * $W * $H * 4 / 1MB, 1)
Write-Host "[zoo-play] preloading $mb MB..."
$imgs = New-Object 'System.Collections.Generic.List[System.Drawing.Bitmap]'
for($i = 0; $i -lt $n; $i++){
  $p = Join-Path $Dir ("{0}{1:d6}.rgba" -f $prefix, $i)
  if(-not (Test-Path $p)){ Write-Host "[zoo-play] missing $p"; exit 1 }
  $imgs.Add([MzLoad]::Rgba($p, $W, $H))
}
Write-Host "[zoo-play] preloaded $($imgs.Count) frames"

$f = New-Object Windows.Forms.Form
$f.Text = $Title
$f.FormBorderStyle = 'None'
$f.StartPosition = 'Manual'
$f.ClientSize = New-Object Drawing.Size($W,$H)
$sc = [Windows.Forms.Screen]::PrimaryScreen.Bounds
# a monitor-sized window snaps to the origin, the same geometry rule ball_zoo/gate_zoo use
if($X -lt 0 -or $Y -lt 0){
  if($W -ge $sc.Width -and $H -ge $sc.Height){ $f.Location = New-Object Drawing.Point(0,0) }
  else { $f.Location = New-Object Drawing.Point(120,120) }
} else { $f.Location = New-Object Drawing.Point($X,$Y) }
$f.BackColor = [Drawing.Color]::Black
$f.TopMost = $false
$f.Add_KeyDown({ param($s,$e) if($e.KeyCode -eq 'Escape'){ $f.Close() } })
$f.Show(); $f.Activate()

[void][MZ.WinMM]::timeBeginPeriod(1)
$ctx = [Drawing.BufferedGraphicsManager]::Current
$ctx.MaximumBuffer = New-Object Drawing.Size(($W+1),($H+1))
$gScreen = $f.CreateGraphics()
$buf = $ctx.Allocate($gScreen, (New-Object Drawing.Rectangle(0,0,$W,$H)))
$g = $buf.Graphics
$g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
$g.PixelOffsetMode   = [Drawing.Drawing2D.PixelOffsetMode]::Half

$sw = [Diagnostics.Stopwatch]::StartNew()
$freq = [double][Diagnostics.Stopwatch]::Frequency
$periodTicks = [long]($freq / $Fps)
$next = $sw.ElapsedTicks + $periodTicks
$idx = 0; $frames = 0; $missed = 0; $loops = 0; $statT0 = $sw.ElapsedTicks
try {
  while(-not $f.IsDisposed){
    $g.DrawImageUnscaled($imgs[$idx], 0, 0)
    $buf.Render()
    $frames++
    $idx++
    if($idx -ge $n){
      if($NoLoop){ break }
      $idx = 0; $loops++
    }

    [Windows.Forms.Application]::DoEvents()
    if($f.IsDisposed){ break }

    $nowT = $sw.ElapsedTicks
    # A missed tick is COUNTED, not smoothed away. The whole point of a fixed-cadence source is that
    # its rate is known; an unreported drop turns a position table into a lie about when a frame was
    # shown, and the barcode would then be the only thing that catches it.
    if($nowT -gt $next + $periodTicks){ $missed += [int](($nowT - $next) / $periodTicks) }
    if($nowT -gt $next + 4*$periodTicks){ $next = $nowT }   # fell far behind: resync, don't spiral
    while($sw.ElapsedTicks -lt $next){
      $remMs = ($next - $sw.ElapsedTicks) * 1000.0 / $freq
      if($remMs -gt 2.0){ [Threading.Thread]::Sleep(1) } else { [Threading.Thread]::SpinWait(60) }
    }
    $next += $periodTicks

    $dt = ($sw.ElapsedTicks - $statT0) / $freq
    if($dt -ge 1.0){
      $ach = $frames / $dt
      Write-Host ("[zoo-play] fps={0:N1} target={1:N0} frame={2}/{3} loops={4} missed={5}" -f $ach,$Fps,$idx,$n,$loops,$missed)
      $frames = 0; $statT0 = $sw.ElapsedTicks
    }
  }
} finally {
  [void][MZ.WinMM]::timeEndPeriod(1)
  if($buf){ $buf.Dispose() }
  if($gScreen){ $gScreen.Dispose() }
  foreach($b in $imgs){ $b.Dispose() }
  Write-Host "[zoo-play] stopped after $loops loop(s); missed ticks total: $missed"
}
