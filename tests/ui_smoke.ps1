param([switch]$Capture, [switch]$KeyboardOnly)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeUi {
  [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left, top, right, bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct Point { public int x, y; }
  [StructLayout(LayoutKind.Sequential)] public struct GuiThreadInfo { public uint cbSize, flags; public IntPtr active, focus, capture, menuOwner, moveSize, caret; public Rect caretRect; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hwnd, ref Point point);
  [DllImport("user32.dll")] public static extern bool GetGUIThreadInfo(uint thread, ref GuiThreadInfo info);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint first, uint second, bool attach);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsChild(IntPtr parent, IntPtr child);
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct MenuItem {
    public uint cbSize, fMask, fType, fState, wID; public IntPtr hSubMenu, hbmpChecked, hbmpUnchecked, dwItemData, dwTypeData;
    public uint cch; public IntPtr hbmpItem;
  }
  [StructLayout(LayoutKind.Sequential)] public struct ScrollInfo { public uint cbSize, fMask; public int nMin, nMax; public uint nPage; public int nPos, nTrackPos; }
  [DllImport("user32.dll")] public static extern bool GetScrollInfo(IntPtr hwnd, int bar, ref ScrollInfo info);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct NotifyData {
    public uint cbSize; public IntPtr hWnd; public uint uID, uFlags, uCallbackMessage; public IntPtr hIcon;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=128)] public string szTip;
    public uint dwState, dwStateMask;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=256)] public string szInfo;
    public uint uTimeout;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst=64)] public string szInfoTitle;
    public uint dwInfoFlags; public Guid guidItem; public IntPtr hBalloonIcon;
  }
  [DllImport("shell32.dll", CharSet=CharSet.Unicode)] public static extern bool Shell_NotifyIcon(uint msg, ref NotifyData data);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
  public static IntPtr FindNextMenu(IntPtr after) { return FindWindowEx(IntPtr.Zero, after, "#32768", null); }
  [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
  [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr menu, int position);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool GetMenuItemInfo(IntPtr menu, uint item, bool byPosition, ref MenuItem info);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetMenuString(IntPtr menu, uint item, System.Text.StringBuilder text, int max, uint flags);
  public delegate bool EnumProc(IntPtr hwnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, System.Text.StringBuilder name, int length);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, System.Text.StringBuilder name, int length);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  public static IntPtr FindClass(string cls) { return FindWindow(cls, null); }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendText(IntPtr hwnd, uint msg, IntPtr w, System.Text.StringBuilder l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint GetGuiResources(IntPtr process, uint flags);
}
'@
$exe = Join-Path $PSScriptRoot '..\build\PWSwitch.exe'
if ([NativeUi]::FindClass('PWSwitchMain') -ne [IntPtr]::Zero) { throw 'PWSwitch is already running; close it before the UI test.' }
$p = Start-Process -FilePath $exe -ArgumentList '--settings' -PassThru -WindowStyle Hidden
try {
  $editor = [IntPtr]::Zero
  for ($i = 0; $i -lt 300 -and $editor -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 100
    if ($p.HasExited) { throw 'Application exited before opening settings.' }
    $editor = [NativeUi]::FindClass('PWSwitchEditor')
  }
  if ($editor -eq [IntPtr]::Zero) {
    Get-CimInstance Win32_Process -Filter "ProcessId = $($p.Id)" | Select-Object CommandLine | Format-List | Out-Host
    $callback = [NativeUi+EnumProc]{ param($h, $l)
      [uint32]$windowPid = 0
      [void][NativeUi]::GetWindowThreadProcessId($h, [ref]$windowPid)
      if ($windowPid -eq $p.Id) {
        $class = New-Object System.Text.StringBuilder 256
        $title = New-Object System.Text.StringBuilder 256
        [void][NativeUi]::GetClassName($h, $class, 256)
        [void][NativeUi]::GetWindowText($h, $title, 256)
        Write-Host "Window: $class / $title"
      }
      return $true
    }
    [void][NativeUi]::EnumWindows($callback, [IntPtr]::Zero)
    throw 'Settings window did not open.'
  }
  $list = [NativeUi]::GetDlgItem($editor, 101)
  $hidden = [NativeUi]::GetDlgItem($editor, 102)
  if ($list -eq [IntPtr]::Zero -or $hidden -eq [IntPtr]::Zero) { throw 'Missing settings controls.' }
  $visibleCount = [NativeUi]::SendMessage($list, 0x018B, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
  if ($visibleCount -le 0) { throw 'No visible power settings.' }
  [void][NativeUi]::SendMessage($hidden, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)
  $allCount = [NativeUi]::SendMessage($list, 0x018B, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
  if ($allCount -lt $visibleCount) { throw 'Enabling hidden settings lost entries.' }
  if ($allCount -gt $visibleCount) {
    $foundMarker = $false
    for ($row = 0; $row -lt $allCount; $row++) {
      $n = [NativeUi]::SendMessage($list, 0x018A, [IntPtr]$row, [IntPtr]::Zero).ToInt32()
      $label = New-Object System.Text.StringBuilder ($n + 1)
      [void][NativeUi]::SendText($list, 0x0189, [IntPtr]$row, $label)
      if ($label.ToString().StartsWith([string][char]0x25C6)) { $foundMarker = $true; break }
    }
    if (-not $foundMarker) { throw 'Hidden settings have no visible marker.' }
  }
  [void][NativeUi]::SetWindowPos($editor, [IntPtr]::Zero, 0, 0, 450, 300, 0x0016)
  $scroll = New-Object NativeUi+ScrollInfo
  $scroll.cbSize = [Runtime.InteropServices.Marshal]::SizeOf([type][NativeUi+ScrollInfo]); $scroll.fMask = 3
  if (-not [NativeUi]::GetScrollInfo($editor, 1, [ref]$scroll) -or $scroll.nMax -lt $scroll.nPage) { throw 'Small settings window has no usable vertical scrolling.' }
  # Send real Tab key messages through the editor's dialog-message loop, then inspect
  # focus and coordinates in the live application; no power values are written.
  $description = [NativeUi]::FindWindowEx($editor, [IntPtr]::Zero, 'Edit', $null)
  $dc = [NativeUi]::GetDlgItem($editor, 104)
  if ($description -eq [IntPtr]::Zero -or $dc -eq [IntPtr]::Zero) { throw 'Missing keyboard test controls.' }
  [uint32]$windowPid = 0
  $uiThread = [NativeUi]::GetWindowThreadProcessId($editor, [ref]$windowPid)
  $testThread = [NativeUi]::GetCurrentThreadId()
  if (-not [NativeUi]::AttachThreadInput($testThread, $uiThread, $true)) { throw 'Could not attach input for the keyboard test.' }
  try {
    [void][NativeUi]::SetForegroundWindow($editor)
    [void][NativeUi]::SetFocus($list)
  } finally { [void][NativeUi]::AttachThreadInput($testThread, $uiThread, $false) }
  [void][NativeUi]::PostMessage($list, 0x0100, [IntPtr]9, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 250
  $gui = New-Object NativeUi+GuiThreadInfo
  $gui.cbSize = [Runtime.InteropServices.Marshal]::SizeOf([type][NativeUi+GuiThreadInfo])
  if (-not [NativeUi]::GetGUIThreadInfo($uiThread, [ref]$gui)) { throw 'Could not inspect keyboard focus.' }
  $keyboardFailures = @()
  if ($gui.focus -ne $description) { $keyboardFailures += 'Tab from settings list skipped the description.' }
  else {
    $original = New-Object System.Text.StringBuilder 8192
    [void][NativeUi]::GetWindowText($description, $original, $original.Capacity)
    $longDescription = ((1..40 | ForEach-Object { "Description line $_" }) -join "`r`n")
    [void][NativeUi]::SendText($description, 0x000C, [IntPtr]::Zero, [System.Text.StringBuilder]::new($longDescription))
    [void][NativeUi]::PostMessage($description, 0x0100, [IntPtr]0x22, [IntPtr]::Zero) # Page Down
    Start-Sleep -Milliseconds 150
    if ([NativeUi]::SendMessage($description, 0x00CE, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -le 0) {
      $lineCount = [NativeUi]::SendMessage($description, 0x00BA, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
      [void][NativeUi]::GetGUIThreadInfo($uiThread, [ref]$gui)
      $actual = New-Object System.Text.StringBuilder 8192
      $length = [NativeUi]::GetWindowText($description, $actual, $actual.Capacity)
      $keyboardFailures += "Focused description could not be scrolled by keyboard (lines=$lineCount, chars=$length, input=$($longDescription.Length), focus=$($gui.focus), description=$description)."
    }
    [void][NativeUi]::SendText($description, 0x000C, [IntPtr]::Zero, $original)
  }

  # Tab from the AC Apply button to the DC input, which starts below the viewport.
  $acApply = [NativeUi]::GetDlgItem($editor, 105)
  if (-not [NativeUi]::AttachThreadInput($testThread, $uiThread, $true)) { throw 'Could not attach input for the scrolling test.' }
  try { [void][NativeUi]::SetFocus($acApply) } finally { [void][NativeUi]::AttachThreadInput($testThread, $uiThread, $false) }
  [void][NativeUi]::PostMessage($acApply, 0x0100, [IntPtr]9, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 250
  if (-not [NativeUi]::GetGUIThreadInfo($uiThread, [ref]$gui)) { throw 'Could not inspect keyboard focus.' }
  if ($gui.focus -ne $dc -and -not [NativeUi]::IsChild($dc, $gui.focus)) { throw 'Tab did not reach the DC input; scrolling test is inconclusive.' }
  $client = New-Object NativeUi+Rect; $origin = New-Object NativeUi+Point; $bounds = New-Object NativeUi+Rect
  if (-not [NativeUi]::GetClientRect($editor, [ref]$client) -or -not [NativeUi]::ClientToScreen($editor, [ref]$origin) -or -not [NativeUi]::GetWindowRect($dc, [ref]$bounds)) { throw 'Could not inspect focused control bounds.' }
  if ($bounds.top -lt $origin.y -or $bounds.bottom -gt ($origin.y + $client.bottom)) { $keyboardFailures += 'Focused DC input is outside the visible client area.' }
  [void][NativeUi]::PostMessage($gui.focus, 0x0100, [IntPtr]9, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 250
  if (-not [NativeUi]::GetGUIThreadInfo($uiThread, [ref]$gui)) { throw 'Could not inspect keyboard focus.' }
  $dcApply = [NativeUi]::GetDlgItem($editor, 106)
  if ($gui.focus -ne $dcApply) { throw 'Tab did not reach the DC Apply button; horizontal scrolling test is inconclusive.' }
  if (-not [NativeUi]::GetWindowRect($dcApply, [ref]$bounds)) { throw 'Could not inspect focused button bounds.' }
  if ($bounds.left -lt $origin.x -or $bounds.right -gt ($origin.x + $client.right) -or $bounds.top -lt $origin.y -or $bounds.bottom -gt ($origin.y + $client.bottom)) { $keyboardFailures += 'Focused Apply button is outside the visible client area.' }
  if ($keyboardFailures.Count) { throw ($keyboardFailures -join ' ') }
  if ($KeyboardOnly) { Write-Output 'Keyboard accessibility passed.'; return }
  $main = [NativeUi]::FindClass('PWSwitchMain')
  $tray = New-Object NativeUi+NotifyData
  $tray.cbSize = [Runtime.InteropServices.Marshal]::SizeOf([type][NativeUi+NotifyData])
  $tray.hWnd = $main; $tray.uID = 1
  if (-not [NativeUi]::Shell_NotifyIcon(2, [ref]$tray)) { throw 'Could not remove tray icon for restart simulation.' }
  $taskbarCreated = [NativeUi]::RegisterWindowMessage('TaskbarCreated')
  [void][NativeUi]::SendMessage($main, $taskbarCreated, [IntPtr]::Zero, [IntPtr]::Zero)
  $tray.uFlags = 4; $tray.szTip = 'PWSwitch test'
  if (-not [NativeUi]::Shell_NotifyIcon(1, [ref]$tray)) { throw 'Tray icon did not recover after taskbar recreation.' }
  [void][NativeUi]::PostMessage($main, 0x8001, [IntPtr]1, [IntPtr]0x0202)
  $menuWindow = [IntPtr]::Zero
  for ($i = 0; $i -lt 50 -and $menuWindow -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 50
    $candidate = [IntPtr]::Zero
    while (($candidate = [NativeUi]::FindNextMenu($candidate)) -ne [IntPtr]::Zero) {
      [uint32]$menuPid = 0
      [void][NativeUi]::GetWindowThreadProcessId($candidate, [ref]$menuPid)
      if ($menuPid -eq $p.Id) { $menuWindow = $candidate; break }
    }
  }
  if ($menuWindow -eq [IntPtr]::Zero) { throw 'Tray click did not open a menu.' }
  # Repeated tray clicks must not create nested menus or change a setting.
  for ($repeat = 0; $repeat -lt 3; $repeat++) {
    [void][NativeUi]::PostMessage($main, 0x8001, [IntPtr]1, [IntPtr]0x0202)
  }
  Start-Sleep -Milliseconds 150
  $popupCount = 0; $candidate = [IntPtr]::Zero
  while (($candidate = [NativeUi]::FindNextMenu($candidate)) -ne [IntPtr]::Zero) {
    [uint32]$menuPid = 0
    [void][NativeUi]::GetWindowThreadProcessId($candidate, [ref]$menuPid)
    if ($menuPid -eq $p.Id) { $popupCount++ }
  }
  if ($popupCount -ne 1) { throw "Repeated tray clicks opened $popupCount menu windows." }
  $menuHandle = [NativeUi]::SendMessage($menuWindow, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
  $menuCount = [NativeUi]::GetMenuItemCount($menuHandle)
  if ($menuCount -lt 7) { throw "Tray menu has too few entries: $menuCount" }
  $timeoutMenus = 0
  $checkedPlans = 0
  $startupItems = 0
  for ($position = 0; $position -lt $menuCount; $position++) {
    $info = New-Object NativeUi+MenuItem
    $info.cbSize = [Runtime.InteropServices.Marshal]::SizeOf([type][NativeUi+MenuItem]); $info.fMask = 0x0100 -bor 0x0001 -bor 0x0002 -bor 0x0004 -bor 0x0080
    if (-not [NativeUi]::GetMenuItemInfo($menuHandle, [uint32]$position, $true, [ref]$info)) { throw 'Could not inspect tray menu item.' }
    $isPlan = $info.wID -ge 1000 -and $info.wID -lt 1900
    $isCommand = $info.wID -ge 3000 -and $info.wID -le 3005
    if ($info.wID -eq 3004) { $startupItems++ }
    if (($isPlan -or $isCommand -or $info.hSubMenu -ne [IntPtr]::Zero) -and $info.hbmpItem -eq [IntPtr]::Zero) { throw "Menu item $position has no icon." }
    if ($isPlan -and ($info.fState -band 8) -ne 0) {
      $checkedPlans++
      $caption = New-Object System.Text.StringBuilder 512
      [void][NativeUi]::GetMenuString($menuHandle, [uint32]$position, $caption, 512, 0x0400)
      if (-not $caption.ToString().EndsWith([string][char]0x2713)) { throw 'The active scheme lacks a visible selection marker.' }
    }
    $submenu = [NativeUi]::GetSubMenu($menuHandle, $position)
    if ($submenu -ne [IntPtr]::Zero -and [NativeUi]::GetMenuItemCount($submenu) -eq 11) {
      $timeoutMenus++
      $header = New-Object NativeUi+MenuItem
      $header.cbSize = [Runtime.InteropServices.Marshal]::SizeOf([type][NativeUi+MenuItem]); $header.fMask = 0x0001
      if (-not [NativeUi]::GetMenuItemInfo($submenu, 0, $true, [ref]$header) -or ($header.fState -band 3) -eq 0) { throw 'Missing information that quick settings affect both power sources.' }
      for ($subpos = 1; $subpos -lt 11; $subpos++) {
        $subinfo = New-Object NativeUi+MenuItem
        $subinfo.cbSize = [Runtime.InteropServices.Marshal]::SizeOf([type][NativeUi+MenuItem]); $subinfo.fMask = 0x0001 -bor 0x0080 -bor 0x0100
        if (-not [NativeUi]::GetMenuItemInfo($submenu, [uint32]$subpos, $true, [ref]$subinfo)) { throw 'Could not inspect timeout option.' }
        if (($subinfo.fType -band 0x0800) -eq 0 -and $subinfo.hbmpItem -eq [IntPtr]::Zero) { throw "Timeout option $subpos has no icon." }
        if (($subinfo.fState -band 8) -ne 0) {
          $caption = New-Object System.Text.StringBuilder 512
          [void][NativeUi]::GetMenuString($submenu, [uint32]$subpos, $caption, 512, 0x0400)
          if (-not $caption.ToString().EndsWith([string][char]0x2713)) { throw 'The selected timeout lacks a visible selection marker.' }
        }
      }
    }
  }
  if ($checkedPlans -ne 1) { throw "Expected one active plan; found $checkedPlans." }
  if ($startupItems -ne 1) { throw "Expected one autostart command; found $startupItems." }
  if ($timeoutMenus -ne 2) { throw "Expected display and sleep menus; found $timeoutMenus" }
  if ($Capture) {
    Add-Type -AssemblyName System.Drawing
    $rect = New-Object NativeUi+Rect
    if (-not [NativeUi]::GetWindowRect($menuWindow, [ref]$rect)) { throw 'Could not get menu bounds.' }
    $width = $rect.right - $rect.left; $height = $rect.bottom - $rect.top
    if ($width -le 0 -or $height -le 0 -or $width -gt 2000 -or $height -gt 2000) { throw 'Unexpected menu bounds.' }
    $image = [System.Drawing.Bitmap]::new($width, $height)
    $graphics = [System.Drawing.Graphics]::FromImage($image)
    try {
      $graphics.CopyFromScreen($rect.left, $rect.top, 0, 0, [System.Drawing.Size]::new($width, $height))
      $image.Save((Join-Path $PSScriptRoot '..\build\menu-preview.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    } finally { $graphics.Dispose(); $image.Dispose() }
  }
  [void][NativeUi]::PostMessage($main, 0x001F, [IntPtr]::Zero, [IntPtr]::Zero)
  Start-Sleep -Milliseconds 150
  $gdiBefore = [NativeUi]::GetGuiResources($p.Handle, 0)
  for ($iteration = 0; $iteration -lt 6; $iteration++) {
    $event = @(0x0202, 0x0205, 0x007B)[$iteration % 3]
    [void][NativeUi]::PostMessage($main, 0x8001, [IntPtr]1, [IntPtr]$event)
    $found = $false
    for ($attempt = 0; $attempt -lt 40 -and -not $found; $attempt++) {
      Start-Sleep -Milliseconds 25
      $candidate = [IntPtr]::Zero
      while (($candidate = [NativeUi]::FindNextMenu($candidate)) -ne [IntPtr]::Zero) {
        [uint32]$menuPid = 0
        [void][NativeUi]::GetWindowThreadProcessId($candidate, [ref]$menuPid)
        if ($menuPid -eq $p.Id) { $found = $true; break }
      }
    }
    if (-not $found) { throw 'Tray menu could not be reopened.' }
    [void][NativeUi]::PostMessage($main, 0x001F, [IntPtr]::Zero, [IntPtr]::Zero)
    Start-Sleep -Milliseconds 100
  }
  $gdiAfter = [NativeUi]::GetGuiResources($p.Handle, 0)
  if ($gdiAfter -gt $gdiBefore + 2) { throw "Repeated menu use leaked GDI objects: before=$gdiBefore after=$gdiAfter" }
  Write-Output "UI smoke passed: visible=$visibleCount, including hidden=$allCount"
} finally {
  $main = [NativeUi]::FindClass('PWSwitchMain')
  if ($main -ne [IntPtr]::Zero) { [void][NativeUi]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
  if (-not $p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
}
