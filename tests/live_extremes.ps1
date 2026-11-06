param(
    [Parameter(Mandatory=$true)][guid]$Maximum,
    [Parameter(Mandatory=$true)][guid]$Savings
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
public static class ExtremeCheck {
  [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left, top, right, bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct NotifyId { public uint cbSize; public IntPtr hwnd; public uint uid; public Guid guid; }
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct MenuItem {
    public uint cbSize, fMask, fType, fState, wID; public IntPtr submenu, checkedBmp, uncheckedBmp, data, typeData;
    public uint cch; public IntPtr bitmap;
  }
  [DllImport("powrprof.dll")] static extern uint PowerGetActiveScheme(IntPtr root, out IntPtr scheme);
  [DllImport("powrprof.dll")] public static extern uint PowerSetActiveScheme(IntPtr root, ref Guid scheme);
  [DllImport("powrprof.dll")] static extern uint PowerReadFriendlyName(IntPtr root, ref Guid scheme, IntPtr subgroup, IntPtr setting, IntPtr buffer, ref uint size);
  [DllImport("powrprof.dll")] public static extern uint PowerReadACValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("powrprof.dll")] public static extern uint PowerReadDCValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls, string title);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool GetMenuItemInfo(IntPtr menu, uint pos, bool byPosition, ref MenuItem info);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuString(IntPtr menu, uint pos, StringBuilder s, int len, uint flags);
  [DllImport("shell32.dll")] static extern int Shell_NotifyIconGetRect(ref NotifyId id, out Rect rect);
  public static IntPtr MainWindow() { return FindWindow("PWSwitchMain", null); }
  public static IntPtr NextMenu(IntPtr after) { return FindWindowEx(IntPtr.Zero, after, "#32768", null); }
  public static Guid Active() { IntPtr p; uint e=PowerGetActiveScheme(IntPtr.Zero,out p); if(e!=0) throw new Exception("active error " + e); try { return (Guid)Marshal.PtrToStructure(p,typeof(Guid)); } finally { LocalFree(p); } }
  public static string Name(Guid id) { uint n=0; uint e=PowerReadFriendlyName(IntPtr.Zero,ref id,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,ref n); if ((e!=0 && e!=234)||n==0) throw new Exception("name length error " + e); IntPtr p=Marshal.AllocHGlobal((int)n); try { e=PowerReadFriendlyName(IntPtr.Zero,ref id,IntPtr.Zero,IntPtr.Zero,p,ref n); if(e!=0) throw new Exception("name error " + e); return Marshal.PtrToStringUni(p); } finally { Marshal.FreeHGlobal(p); } }
  public static string Capture(IntPtr hwnd, string path) {
    NotifyId id = new NotifyId(); id.cbSize=(uint)Marshal.SizeOf(typeof(NotifyId)); id.hwnd=hwnd; id.uid=1;
    Rect r; int status=Shell_NotifyIconGetRect(ref id,out r); int w=r.right-r.left, h=r.bottom-r.top;
    if(status!=0 || w<=0 || h<=0 || w>96 || h>96) throw new Exception("tray icon rectangle unavailable: " + status);
    int reds=0, greens=0;
    using(var bmp=new Bitmap(w,h)) using(var g=Graphics.FromImage(bmp)) {
      g.CopyFromScreen(r.left,r.top,0,0,new Size(w,h));
      for(int y=0;y<h;y++) for(int x=0;x<w;x++) { Color c=bmp.GetPixel(x,y); if(c.R>165 && c.G<115 && c.B<115) reds++; if(c.G>150 && c.R>80 && c.R<210 && c.B<110 && c.G>c.R*1.05) greens++; }
      bmp.Save(path, System.Drawing.Imaging.ImageFormat.Png);
    }
    return reds + "," + greens;
  }
}
'@ -ReferencedAssemblies System.Drawing

if ($Maximum -eq $Savings) { throw 'Maximum and saving schemes must be different.' }
$processes = @(Get-Process -Name PWSwitch -ErrorAction SilentlyContinue)
if ($processes.Count -ne 1) { throw 'Exactly one running PWSwitch instance is required.' }
$app = $processes[0]
$main = [ExtremeCheck]::MainWindow()
if ($main -eq [IntPtr]::Zero) { throw 'PWSwitch window not found.' }
$original = [ExtremeCheck]::Active()
$lastSet = $null
$video = [guid]'7516b95f-f776-4464-8c53-06167f40cc99'
$display = [guid]'3c0bc021-c8a8-4e07-a973-6b14cbcb2b7e'
$sleep = [guid]'238c9fa8-0aad-41ed-83f4-97be242c8f20'
$standby = [guid]'29f6c1db-86da-48c5-9fdb-f2b67b1f44da'
function Read-Timeouts([guid]$scheme) {
  $values = @()
  foreach ($pair in @(@($video,$display), @($sleep,$standby))) {
    [guid]$group=$pair[0]; [guid]$setting=$pair[1]; [uint32]$ac=0; [uint32]$dc=0
    $a=[ExtremeCheck]::PowerReadACValueIndex([IntPtr]::Zero,[ref]$scheme,[ref]$group,[ref]$setting,[ref]$ac)
    $d=[ExtremeCheck]::PowerReadDCValueIndex([IntPtr]::Zero,[ref]$scheme,[ref]$group,[ref]$setting,[ref]$dc)
    if ($a -ne 0 -or $d -ne 0) { throw "Could not read timeouts: AC=$a DC=$d" }
    $values += @($ac,$dc)
  }
  return ($values -join ',')
}
function Test-CheckedMenu([string]$expectedName) {
  [void][ExtremeCheck]::PostMessage($main, 0x8001, [IntPtr]1, [IntPtr]0x0202)
  $menuWindow=[IntPtr]::Zero
  try {
    for ($attempt=0; $attempt -lt 60 -and $menuWindow -eq [IntPtr]::Zero; $attempt++) {
      Start-Sleep -Milliseconds 50
      $candidate=[IntPtr]::Zero
      while (($candidate=[ExtremeCheck]::NextMenu($candidate)) -ne [IntPtr]::Zero) {
        [uint32]$menuPid=0; [void][ExtremeCheck]::GetWindowThreadProcessId($candidate,[ref]$menuPid)
        if ($menuPid -eq $app.Id) { $menuWindow=$candidate; break }
      }
    }
    if ($menuWindow -eq [IntPtr]::Zero) { throw 'Tray menu did not open.' }
    $menu=[ExtremeCheck]::SendMessage($menuWindow,0x01E1,[IntPtr]::Zero,[IntPtr]::Zero)
    $count=[ExtremeCheck]::GetMenuItemCount($menu); $found=$false; $selected=0
    for ($i=0; $i -lt $count; $i++) {
      $item=New-Object ExtremeCheck+MenuItem
      $item.cbSize=[Runtime.InteropServices.Marshal]::SizeOf([type][ExtremeCheck+MenuItem]); $item.fMask=0x1 -bor 0x2 -bor 0x80
      if (-not [ExtremeCheck]::GetMenuItemInfo($menu,[uint32]$i,$true,[ref]$item)) { throw 'Cannot inspect menu.' }
      if ($item.wID -ge 1000 -and $item.wID -lt 1900 -and ($item.fState -band 8) -ne 0) {
        $selected++
        $caption=New-Object System.Text.StringBuilder 512
        [void][ExtremeCheck]::GetMenuString($menu,[uint32]$i,$caption,512,0x400)
        if ($caption.ToString().StartsWith($expectedName) -and $caption.ToString().EndsWith([string][char]0x2713) -and $item.bitmap -ne [IntPtr]::Zero) { $found=$true }
      }
    }
    if (-not $found -or $selected -ne 1) { throw "Menu selection incorrect; selected=$selected expected=$expectedName" }
  } finally { [void][ExtremeCheck]::PostMessage($main,0x001F,[IntPtr]::Zero,[IntPtr]::Zero); Start-Sleep -Milliseconds 120 }
}
$maxBefore=Read-Timeouts $Maximum
$saveBefore=Read-Timeouts $Savings
$outDir=Join-Path $PSScriptRoot '..\build\extreme-checks'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
try {
  foreach ($case in @(@('maximum',$Maximum,'red'), @('savings',$Savings,'green'))) {
    $label=$case[0]; [guid]$target=$case[1]; $color=$case[2]
    $current=[ExtremeCheck]::Active()
    if ($current -ne $original -and ($null -eq $lastSet -or $current -ne $lastSet)) { throw 'Active scheme changed externally; stopping without overwriting the new choice.' }
    $code=[ExtremeCheck]::PowerSetActiveScheme([IntPtr]::Zero,[ref]$target)
    if ($code -ne 0) { throw "Cannot activate ${label}: $code" }
    $lastSet=$target
    if ([ExtremeCheck]::Active() -ne $target) { throw "Active scheme did not become $label." }
    $name=[ExtremeCheck]::Name($target)
    $path=Join-Path $outDir ($label + '.png')
    $matched=$false; $counts=''
    for ($poll=0; $poll -lt 60 -and -not $matched; $poll++) {
      Start-Sleep -Milliseconds 150
      $counts=[ExtremeCheck]::Capture($main,$path); $parts=$counts.Split(',')
      $matched=if ($color -eq 'red') { [int]$parts[0] -ge 5 -and [int]$parts[0] -gt [int]$parts[1] } else { [int]$parts[1] -ge 5 -and [int]$parts[1] -gt [int]$parts[0] }
    }
    if (-not $matched) { throw "Tray icon was not $color for ${label}; red,green=$counts" }
    Test-CheckedMenu $name
    Write-Output "PASS $label : $name | icon $color ($counts) | current menu selection correct | timeouts $(Read-Timeouts $target)"
  }
  if ((Read-Timeouts $Maximum) -ne $maxBefore -or (Read-Timeouts $Savings) -ne $saveBefore) { throw 'One or more timeout settings changed during checks.' }
} finally {
  $current=[ExtremeCheck]::Active()
  if ($null -ne $lastSet -and $current -eq $lastSet -and $current -ne $original) {
    $code=[ExtremeCheck]::PowerSetActiveScheme([IntPtr]::Zero,[ref]$original)
    if ($code -ne 0 -or [ExtremeCheck]::Active() -ne $original) { throw "Could not restore original scheme: $code" }
    Write-Output "RESTORED $original"
  } elseif ($current -eq $original) { Write-Output "UNCHANGED $original" }
  else { Write-Warning "Active scheme was changed externally to $current; it was not overwritten." }
}
