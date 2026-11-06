$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class PowerOptionsUi {
  [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left, top, right, bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct Point { public int x, y; }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
  public static IntPtr MainWindow() { return FindWindow("PWSwitchMain", null); }
  public static IntPtr NextMenu(IntPtr after) { return FindWindowEx(IntPtr.Zero, after, "#32768", null); }
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
  [DllImport("user32.dll")] public static extern uint GetMenuItemID(IntPtr menu, int pos);
  [DllImport("user32.dll")] public static extern bool GetMenuItemRect(IntPtr hwnd, IntPtr menu, uint item, out Rect rect);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
}
'@

if ([PowerOptionsUi]::MainWindow() -ne [IntPtr]::Zero) { throw 'PWSwitch is already running.' }
$shell = New-Object -ComObject Shell.Application
$russianTitle = -join ([char[]](0x042D,0x043B,0x0435,0x043A,0x0442,0x0440,0x043E,0x043F,0x0438,0x0442,0x0430,0x043D,0x0438,0x0435))
$existing = @{}
foreach ($window in @($shell.Windows())) { $existing[[string]$window.HWND] = $true }
$cursor = New-Object PowerOptionsUi+Point
[void][PowerOptionsUi]::GetCursorPos([ref]$cursor)
$app = $null
$opened = $null
try {
  $app = Start-Process -FilePath (Join-Path $PSScriptRoot '..\build\PWSwitch.exe') -PassThru
  $main = [IntPtr]::Zero
  for ($i = 0; $i -lt 100 -and $main -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 50
    if ($app.HasExited) { throw 'PWSwitch exited during startup.' }
    $main = [PowerOptionsUi]::MainWindow()
  }
  if ($main -eq [IntPtr]::Zero) { throw 'PWSwitch did not start.' }
  [void][PowerOptionsUi]::PostMessage($main, 0x8001, [IntPtr]1, [IntPtr]0x0202)
  $popup = [IntPtr]::Zero
  for ($i = 0; $i -lt 100 -and $popup -eq [IntPtr]::Zero; $i++) {
    $candidate = [IntPtr]::Zero
    while (($candidate = [PowerOptionsUi]::NextMenu($candidate)) -ne [IntPtr]::Zero) {
      [uint32]$owner = 0; [void][PowerOptionsUi]::GetWindowThreadProcessId($candidate, [ref]$owner)
      if ($owner -eq $app.Id) { $popup = $candidate; break }
    }
    if ($popup -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 50 }
  }
  if ($popup -eq [IntPtr]::Zero) { throw 'Tray menu did not open.' }
  $menu = [PowerOptionsUi]::SendMessage($popup, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
  $position = -1
  for ($i = 0; $i -lt [PowerOptionsUi]::GetMenuItemCount($menu); $i++) {
    if ([PowerOptionsUi]::GetMenuItemID($menu, $i) -eq 3001) { $position = $i; break }
  }
  if ($position -lt 0) { throw 'Power Options menu command is missing.' }
  $rect = New-Object PowerOptionsUi+Rect
  if (-not [PowerOptionsUi]::GetMenuItemRect($popup, $menu, [uint32]$position, [ref]$rect)) { throw 'Cannot locate Power Options menu item.' }
  [void][PowerOptionsUi]::SetCursorPos([int](($rect.left + $rect.right) / 2), [int](($rect.top + $rect.bottom) / 2))
  Start-Sleep -Milliseconds 120
  [PowerOptionsUi]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero)
  [PowerOptionsUi]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero)
  for ($i = 0; $i -lt 160 -and $null -eq $opened; $i++) {
    Start-Sleep -Milliseconds 100
    foreach ($window in @($shell.Windows())) {
      if (-not $existing.ContainsKey([string]$window.HWND) -and
          ($window.LocationName -eq $russianTitle -or $window.LocationName -eq 'Power Options' -or $window.LocationURL -match '025A5937-A6BE-4686-A844-36FE4BEC8B6D')) {
        $opened = $window; break
      }
    }
  }
  if ($null -eq $opened) { throw 'No new Power Options window was detected.' }
  Write-Output "Power Options menu opened: $($opened.LocationName)"
} finally {
  if ($null -ne $opened) { $opened.Quit() }
  if ($null -ne $app -and -not $app.HasExited) {
    if ($main -ne [IntPtr]::Zero) { [void][PowerOptionsUi]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
    if (-not $app.WaitForExit(3000)) { $app.Kill(); $app.WaitForExit() }
  }
  [void][PowerOptionsUi]::SetCursorPos($cursor.x, $cursor.y)
}
