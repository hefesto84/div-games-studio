# Utilidad de diagnostico del IDE portado (div_ide_port).
#
# Lanza el ejecutable y le inyecta clics y pulsaciones de teclado sinteticas,
# capturando la pantalla en rafaga (DIV_IDE_SHOT_EVERY). Las coordenadas de
# los clics son las del framebuffer virtual (640x480); el script las convierte
# usando la escala real de la ventana.
#
# Ejemplos:
#   tools\drive_ide.ps1 -Clicks "25,472;40,30"
#   tools\drive_ide.ps1 -Keys "ctrl+escape"
#   tools\drive_ide.ps1 -Keys "h;o;l;a" -TailMs 3000
#
# Ver docs/architecture/12-port-progreso.md (hitos E2.2 y E2.3).

param([string]$Base = "C:\Users\Dani\Documents\DIV\build\seq\s.png",
      [string]$Clicks = "",      # "x,y;x,y;..." en coordenadas virtuales
      [string]$Keys = "",        # "a;ctrl+escape;alt+x;..." (ver $vk)
      [int]$StepMs = 1200,
      [int]$Every = 1,
      [int]$TailMs = 2500)

Add-Type -Namespace IDE -Name U -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool SetCursorPos(int X,int Y);
[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
[DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
[DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
[DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
[DllImport("user32.dll")] public static extern void mouse_event(uint f,uint x,uint y,uint d,IntPtr e);
[DllImport("user32.dll")] public static extern void keybd_event(byte vk,byte scan,uint f,IntPtr e);
[DllImport("user32.dll")] public static extern short VkKeyScanA(char c);
public struct RECT{public int l,t,r,b;}
public struct POINT{public int x,y;}
'@ -ErrorAction SilentlyContinue

# Codigos virtuales de las teclas que se nombran por su nombre.
$vk = @{
  'escape'=0x1B; 'esc'=0x1B; 'enter'=0x0D; 'tab'=0x09; 'space'=0x20;
  'backspace'=0x08; 'delete'=0x2E; 'insert'=0x2D; 'home'=0x24; 'end'=0x23;
  'pgup'=0x21; 'pgdn'=0x22; 'up'=0x26; 'down'=0x28; 'left'=0x25; 'right'=0x27;
  'ctrl'=0x11; 'shift'=0x10; 'alt'=0x12;
  'f1'=0x70; 'f2'=0x71; 'f3'=0x72; 'f4'=0x73; 'f5'=0x74; 'f6'=0x75;
  'f7'=0x76; 'f8'=0x77; 'f9'=0x78; 'f10'=0x79; 'f11'=0x7A; 'f12'=0x7B
}
$KEYUP = 0x0002
$EXT   = 0x0001

function Send-Key([string]$spec) {
  $parts = $spec.Split('+')
  $main = $parts[-1].ToLower()
  $mods = @()
  if ($parts.Count -gt 1) {
    foreach ($m in $parts[0..($parts.Count-2)]) {
      if ($m) { $mods += $vk[$m.ToLower()] }
    }
  }

  if ($vk.ContainsKey($main)) {
    $code = $vk[$main]
  } else {
    # Caracter imprimible: VkKeyScan da el codigo virtual segun la
    # distribucion de teclado activa.
    $code = [IDE.U]::VkKeyScanA([char]$main) -band 0xFF
  }

  # El bloque de edicion necesita el flag de tecla extendida o Windows lo
  # confunde con el teclado numerico.
  $flags = 0
  if (@(0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x2D,0x2E) -contains $code) { $flags = $EXT }

  foreach ($m in $mods) { [IDE.U]::keybd_event([byte]$m,0,0,[IntPtr]::Zero); Start-Sleep -Milliseconds 40 }
  [IDE.U]::keybd_event([byte]$code,0,$flags,[IntPtr]::Zero)
  Start-Sleep -Milliseconds 80
  [IDE.U]::keybd_event([byte]$code,0,($flags -bor $KEYUP),[IntPtr]::Zero)
  Start-Sleep -Milliseconds 40
  [array]::Reverse($mods)
  foreach ($m in $mods) { [IDE.U]::keybd_event([byte]$m,0,$KEYUP,[IntPtr]::Zero); Start-Sleep -Milliseconds 40 }
  Write-Host "tecla $spec"
}

$dir = Split-Path $Base
if (Test-Path $dir) { Remove-Item "$dir\*" -Force }
New-Item -ItemType Directory -Force -Path $dir | Out-Null

$env:DIV_IDE_SHOT = $Base
$env:DIV_IDE_SHOT_FRAME = "40"
$env:DIV_IDE_SHOT_EVERY = "$Every"

$p = Start-Process -FilePath "C:\Users\Dani\Documents\DIV\build\Release\div_ide_port.exe" -PassThru
Start-Sleep -Seconds 3
$h = (Get-Process -Id $p.Id).MainWindowHandle
$r = New-Object IDE.U+RECT; [void][IDE.U]::GetClientRect($h, [ref]$r)
$o = New-Object IDE.U+POINT; [void][IDE.U]::ClientToScreen($h, [ref]$o)
$s = $r.r / 640
[void][IDE.U]::SetForegroundWindow($h)
Start-Sleep -Milliseconds 500

foreach ($pt in ($Clicks.Split(';') | Where-Object { $_ })) {
  $c = $pt.Split(','); $vx = [int]$c[0]; $vy = [int]$c[1]
  [void][IDE.U]::SetCursorPos([int]($o.x + $vx*$s), [int]($o.y + $vy*$s))
  Start-Sleep -Milliseconds 250
  [IDE.U]::mouse_event(0x0002,0,0,0,[IntPtr]::Zero)
  Start-Sleep -Milliseconds 120
  [IDE.U]::mouse_event(0x0004,0,0,0,[IntPtr]::Zero)
  Write-Host "click virtual ($vx,$vy)"
  Start-Sleep -Milliseconds $StepMs
}

foreach ($k in ($Keys.Split(';') | Where-Object { $_ })) {
  if ($p.HasExited) { break }
  Send-Key $k
  Start-Sleep -Milliseconds $StepMs
}

Start-Sleep -Milliseconds $TailMs
$exited = $p.HasExited
if (!$exited) { Stop-Process -Id $p.Id }
Write-Host "capturas: $((Get-ChildItem $dir -Filter *.png).Count)  escala=$s  salio_solo=$exited"
