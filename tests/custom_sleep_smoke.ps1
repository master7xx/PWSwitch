$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class UndoUi {
  [StructLayout(LayoutKind.Sequential)] public struct Rect { public int left, top, right, bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct Point { public int x, y; }
  [DllImport("powrprof.dll")] static extern uint PowerGetActiveScheme(IntPtr root, out IntPtr id);
  [DllImport("powrprof.dll")] public static extern uint PowerSetActiveScheme(IntPtr root, ref Guid id);
  [DllImport("powrprof.dll")] public static extern uint PowerDuplicateScheme(IntPtr root, ref Guid source, out IntPtr id);
  [DllImport("powrprof.dll")] public static extern uint PowerDeleteScheme(IntPtr root, ref Guid id);
  [DllImport("powrprof.dll")] public static extern uint PowerEnumerate(IntPtr root, IntPtr scheme, IntPtr group, int access, uint index, out Guid id, ref uint size);
  [DllImport("powrprof.dll")] static extern uint PowerReadFriendlyName(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, IntPtr buffer, ref uint size);
  [DllImport("powrprof.dll")] public static extern uint PowerReadACValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("powrprof.dll")] public static extern uint PowerReadDCValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("powrprof.dll")] public static extern uint PowerWriteACValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, uint value);
  [DllImport("powrprof.dll")] public static extern uint PowerWriteDCValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, uint value);
  [DllImport("kernel32.dll")] public static extern IntPtr LocalFree(IntPtr p);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  public static IntPtr MainWindow() { return FindWindow("PWSwitchMain", null); }
  public static IntPtr EditorWindow() { return FindWindow("PWSwitchEditor", null); }
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr hwnd, uint command);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int max);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendText(IntPtr hwnd, uint msg, IntPtr wp, StringBuilder lp);
  [DllImport("user32.dll")] public static extern int GetMenuItemCount(IntPtr menu);
  [DllImport("user32.dll")] public static extern uint GetMenuItemID(IntPtr menu, int pos);
  [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr menu, int pos);
  [DllImport("user32.dll")] public static extern bool GetMenuItemRect(IntPtr hwnd, IntPtr menu, uint item, out Rect rect);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out Point p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  public static Guid Active() { IntPtr p; uint e=PowerGetActiveScheme(IntPtr.Zero,out p); if(e!=0 || p==IntPtr.Zero) throw new Exception("Active scheme error " + e); try { return (Guid)Marshal.PtrToStructure(p,typeof(Guid)); } finally { LocalFree(p); } }
  public static string SettingName(Guid scheme, Guid group, Guid setting) {
    uint size=0; uint status=PowerReadFriendlyName(IntPtr.Zero,ref scheme,ref group,ref setting,IntPtr.Zero,ref size);
    if ((status!=0 && status!=234) || size==0) throw new Exception("Cannot read setting name size: " + status);
    IntPtr buffer=Marshal.AllocHGlobal((int)size);
    try { status=PowerReadFriendlyName(IntPtr.Zero,ref scheme,ref group,ref setting,buffer,ref size); if(status!=0) throw new Exception("Cannot read setting name: " + status); return Marshal.PtrToStringUni(buffer); }
    finally { Marshal.FreeHGlobal(buffer); }
  }
  public static IntPtr NextWindow(string cls, IntPtr after) { return FindWindowEx(IntPtr.Zero, after, cls, null); }
}
'@

if ([UndoUi]::MainWindow() -ne [IntPtr]::Zero) { throw 'PWSwitch is already running.' }
$original = [UndoUi]::Active()
$sleep = [guid]'238c9fa8-0aad-41ed-83f4-97be242c8f20'
$standby = [guid]'29f6c1db-86da-48c5-9fdb-f2b67b1f44da'
$copy = [guid]::Empty
$app = $null
$main = [IntPtr]::Zero
$cursor = New-Object UndoUi+Point
$haveCursor = [UndoUi]::GetCursorPos([ref]$cursor)
function Assert-Active([guid]$expected) {
  $current = [UndoUi]::Active()
  if ($current -ne $expected) { throw "Active scheme changed externally to $current; stopping test." }
}
function Read-Sleep([guid]$scheme) {
  [uint32]$ac = 0; [uint32]$dc = 0
  if ([UndoUi]::PowerReadACValueIndex([IntPtr]::Zero,[ref]$scheme,[ref]$sleep,[ref]$standby,[ref]$ac) -ne 0 -or
      [UndoUi]::PowerReadDCValueIndex([IntPtr]::Zero,[ref]$scheme,[ref]$sleep,[ref]$standby,[ref]$dc) -ne 0) { throw 'Cannot read sleep timeout.' }
  return "$ac,$dc"
}
function Find-Owned([string]$class) {
  $window = [IntPtr]::Zero
  while (($window = [UndoUi]::NextWindow($class, $window)) -ne [IntPtr]::Zero) {
    [uint32]$owner = 0; [void][UndoUi]::GetWindowThreadProcessId($window, [ref]$owner)
    if ($owner -eq $app.Id) { return $window }
  }
  return [IntPtr]::Zero
}
function Wait-Owned([string]$class) {
  for ($i=0; $i -lt 100; $i++) {
    if ($app.HasExited) { throw 'PWSwitch exited during the test.' }
    $window = Find-Owned $class
    if ($window -ne [IntPtr]::Zero) { return $window }
    Start-Sleep -Milliseconds 50
  }
  throw "Expected window $class did not open."
}
function Click-MenuItem([IntPtr]$window, [IntPtr]$menu, [int]$position) {
  $rect = New-Object UndoUi+Rect
  if (-not [UndoUi]::GetMenuItemRect($window,$menu,[uint32]$position,[ref]$rect)) { throw 'Cannot locate menu item.' }
  [void][UndoUi]::SetCursorPos([int](($rect.left+$rect.right)/2),[int](($rect.top+$rect.bottom)/2))
  Start-Sleep -Milliseconds 120
  [UndoUi]::mouse_event(2,0,0,0,[UIntPtr]::Zero); [UndoUi]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
}
function Open-CustomSleep {
  Assert-Active $copy
  [void][UndoUi]::PostMessage($main,0x8001,[IntPtr]1,[IntPtr]0x0202)
  $menuWindow = Wait-Owned '#32768'
  $menu = [UndoUi]::SendMessage($menuWindow,0x01E1,[IntPtr]::Zero,[IntPtr]::Zero)
  for ($i=0; $i -lt [UndoUi]::GetMenuItemCount($menu); $i++) {
    $sub = [UndoUi]::GetSubMenu($menu,$i)
    if ($sub -eq [IntPtr]::Zero) { continue }
    for ($j=0; $j -lt [UndoUi]::GetMenuItemCount($sub); $j++) {
      if ([UndoUi]::GetMenuItemID($sub,$j) -ne 2199) { continue }
      Click-MenuItem $menuWindow $menu $i
      $subWindow = [IntPtr]::Zero
      for ($attempt=0; $attempt -lt 100 -and $subWindow -eq [IntPtr]::Zero; $attempt++) {
        $candidate=[IntPtr]::Zero
        while (($candidate=[UndoUi]::NextWindow('#32768',$candidate)) -ne [IntPtr]::Zero) {
          [uint32]$owner=0; [void][UndoUi]::GetWindowThreadProcessId($candidate,[ref]$owner)
          if ($owner -eq $app.Id -and [UndoUi]::SendMessage($candidate,0x01E1,[IntPtr]::Zero,[IntPtr]::Zero) -eq $sub) { $subWindow=$candidate; break }
        }
        if ($subWindow -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 50 }
      }
      if ($subWindow -eq [IntPtr]::Zero) { throw 'Sleep submenu did not open.' }
      Assert-Active $copy
      Click-MenuItem $subWindow $sub $j
      return (Wait-Owned 'PWSwitchPrompt')
    }
  }
  throw 'Custom sleep command unavailable.'
}
function Set-Minutes([IntPtr]$prompt, [string]$minutes) {
  $edit=[UndoUi]::GetDlgItem($prompt,11)
  if ($edit -eq [IntPtr]::Zero) { throw 'Custom-time input missing.' }
  $text=New-Object System.Text.StringBuilder $minutes
  [void][UndoUi]::SendText($edit,0x000C,[IntPtr]::Zero,$text)
}
function Respond([IntPtr]$window, [int]$button) {
  if (-not [UndoUi]::PostMessage($window,0x0111,[IntPtr]$button,[IntPtr]::Zero)) { throw 'Could not answer dialog.' }
}
function Wait-Closed([IntPtr]$window, [string]$stage) {
  for ($attempt=0; $attempt -lt 100 -and [UndoUi]::IsWindow($window); $attempt++) {
    Assert-Active $copy
    Start-Sleep -Milliseconds 50
  }
  if ([UndoUi]::IsWindow($window)) {
    $class=New-Object System.Text.StringBuilder 256
    $title=New-Object System.Text.StringBuilder 256
    [void][UndoUi]::GetClassName($window,$class,256)
    [void][UndoUi]::GetWindowText($window,$title,256)
    throw "Dialog did not close at ${stage}: class=$class title=$title enabled=$([UndoUi]::IsWindowEnabled($window)) hwnd=$window."
  }
}
function Wait-Values([string]$expected) {
  for ($attempt=0; $attempt -lt 100; $attempt++) {
    Assert-Active $copy
    $actual=Read-Sleep $copy
    if ($actual -eq $expected) { return }
    Start-Sleep -Milliseconds 50
  }
  throw "Expected sleep timeouts $expected, got $actual."
}
try {
  $originalValues = Read-Sleep $original
  $ptr=[IntPtr]::Zero
  $code=[UndoUi]::PowerDuplicateScheme([IntPtr]::Zero,[ref]$original,[ref]$ptr)
  if ($code -ne 0 -or $ptr -eq [IntPtr]::Zero) { throw "Cannot duplicate scheme: $code" }
  try { $copy=[Runtime.InteropServices.Marshal]::PtrToStructure($ptr,[type][guid]) } finally { [void][UndoUi]::LocalFree($ptr) }
  Assert-Active $original
  if ([UndoUi]::PowerWriteACValueIndex([IntPtr]::Zero,[ref]$copy,[ref]$sleep,[ref]$standby,14400) -ne 0) { throw 'Cannot set temporary AC value.' }
  Assert-Active $original
  if ([UndoUi]::PowerWriteDCValueIndex([IntPtr]::Zero,[ref]$copy,[ref]$sleep,[ref]$standby,18000) -ne 0) { throw 'Cannot set temporary DC value.' }
  Assert-Active $original
  $code=[UndoUi]::PowerSetActiveScheme([IntPtr]::Zero,[ref]$copy)
  if ($code -ne 0) { throw "Cannot activate temporary scheme: $code" }
  Assert-Active $copy
  $app=Start-Process -FilePath (Join-Path $PSScriptRoot '..\build\PWSwitch.exe') -PassThru -WindowStyle Hidden
  $main=Wait-Owned 'PWSwitchMain'
  Write-Output 'Checking cancellation.'
  $prompt=Open-CustomSleep
  Set-Minutes $prompt '123'
  Respond $prompt 2
  Wait-Closed $prompt 'cancel custom time'
  Wait-Values '14400,18000'
  Write-Output 'Checking zero minutes.'
  $prompt=Open-CustomSleep
  Set-Minutes $prompt '0'
  Assert-Active $copy
  Respond $prompt 1
  Wait-Values '0,0'
  Write-Output 'Checking minute conversion.'
  $prompt=Open-CustomSleep
  Set-Minutes $prompt '240'
  Assert-Active $copy
  Respond $prompt 1
  Wait-Values '14400,14400'
  Write-Output 'Checking overlarge input.'
  $prompt=Open-CustomSleep
  Set-Minutes $prompt '71582789'
  Assert-Active $copy
  Respond $prompt 1
  $warning=[IntPtr]::Zero; $okButton=[IntPtr]::Zero
  for ($attempt=0; $attempt -lt 100 -and $warning -eq [IntPtr]::Zero; $attempt++) {
    Assert-Active $copy
    $candidate=[IntPtr]::Zero
    while (($candidate=[UndoUi]::NextWindow('#32770',$candidate)) -ne [IntPtr]::Zero) {
      [uint32]$owner=0; [void][UndoUi]::GetWindowThreadProcessId($candidate,[ref]$owner)
      if ($owner -ne $app.Id -or [UndoUi]::GetWindow($candidate,4) -ne $prompt -or -not [UndoUi]::IsWindowVisible($candidate)) { continue }
      $button=[UndoUi]::GetDlgItem($candidate,1)
      if ($button -eq [IntPtr]::Zero) { $button=[UndoUi]::GetDlgItem($candidate,2) }
      if ($button -ne [IntPtr]::Zero) { $warning=$candidate; $okButton=$button; break }
    }
    if ($warning -eq [IntPtr]::Zero) { Start-Sleep -Milliseconds 50 }
  }
  if ($warning -eq [IntPtr]::Zero) { throw 'Expected custom-time validation warning and button did not appear.' }
  if ((Read-Sleep $copy) -ne '14400,14400') { throw 'Out-of-range value changed the sleep timeouts.' }
  if (-not [UndoUi]::PostMessage($okButton,0x00F5,[IntPtr]::Zero,[IntPtr]::Zero)) { throw 'Could not click the validation warning OK button.' }
  Wait-Closed $warning 'dismiss validation warning'
  Respond $prompt 2
  Wait-Closed $prompt 'cancel after validation warning'
  Wait-Values '14400,14400'
  if ((Read-Sleep $original) -ne $originalValues) { throw 'Original scheme sleep settings changed.' }
  Write-Output 'Custom sleep passed: cancel, zero, minute conversion, and out-of-range validation.'
} finally {
  if ($app -and -not $app.HasExited) {
    if ($main -ne [IntPtr]::Zero) { [void][UndoUi]::PostMessage($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) }
    if (-not $app.WaitForExit(5000)) { Stop-Process -Id $app.Id -Force }
  }
  if ($copy -ne [guid]::Empty) {
    $current=$null
    try { $current=[UndoUi]::Active() } catch { Write-Warning "Cannot read active scheme; leaving temporary scheme $copy." }
    if ($null -ne $current -and $current -eq $copy) {
      $restore=[UndoUi]::PowerSetActiveScheme([IntPtr]::Zero,[ref]$original)
      if ($restore -ne 0) { Write-Warning "Cannot restore original scheme ($restore); leaving temporary scheme $copy." }
    } elseif ($null -ne $current -and $current -ne $original) { Write-Warning "Active scheme changed externally to $current; preserving that choice." }
    $current=$null
    try { $current=[UndoUi]::Active() } catch { Write-Warning "Cannot verify active scheme; leaving temporary scheme $copy." }
    if ($null -ne $current -and $current -ne $copy) {
      $cleanup=[UndoUi]::PowerDeleteScheme([IntPtr]::Zero,[ref]$copy)
      if ($cleanup -ne 0) { Write-Warning "Cannot remove temporary scheme ${copy}: $cleanup" }
    } else { Write-Warning "Temporary scheme $copy was not removed because it is active or its status is unknown." }
  }
  if ($haveCursor) { [void][UndoUi]::SetCursorPos($cursor.x,$cursor.y) }
}
