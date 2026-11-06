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
$video = [guid]'7516b95f-f776-4464-8c53-06167f40cc99'
$display = [guid]'3c0bc021-c8a8-4e07-a973-6b14cbcb2b7e'
$copies = @()
$app = $null
$cursor = New-Object UndoUi+Point
[void][UndoUi]::GetCursorPos([ref]$cursor)
function MenuWindow([int]$processId, [IntPtr]$expected) {
  for ($i = 0; $i -lt 100; $i++) {
    $window = [IntPtr]::Zero
    while (($window = [UndoUi]::NextWindow('#32768', $window)) -ne [IntPtr]::Zero) {
      [uint32]$owner = 0; [void][UndoUi]::GetWindowThreadProcessId($window, [ref]$owner)
      if ($owner -eq $processId) {
        $handle = [UndoUi]::SendMessage($window, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
        if ($expected -eq [IntPtr]::Zero -or $expected -eq $handle) { return $window }
      }
    }
    Start-Sleep -Milliseconds 50
  }
  throw 'Menu did not open.'
}
function Open-Menu {
  [void][UndoUi]::PostMessage($main, 0x8001, [IntPtr]1, [IntPtr]0x0202)
  return (MenuWindow $app.Id ([IntPtr]::Zero))
}
function Click-MenuItem([IntPtr]$window, [IntPtr]$menu, [int]$position) {
  $rect = New-Object UndoUi+Rect
  if (-not [UndoUi]::GetMenuItemRect($window, $menu, [uint32]$position, [ref]$rect)) { throw 'Cannot locate menu item.' }
  [void][UndoUi]::SetCursorPos([int](($rect.left + $rect.right) / 2), [int](($rect.top + $rect.bottom) / 2))
  Start-Sleep -Milliseconds 120
  [UndoUi]::mouse_event(2,0,0,0,[UIntPtr]::Zero); [UndoUi]::mouse_event(4,0,0,0,[UIntPtr]::Zero)
  Start-Sleep -Milliseconds 250
}
function Select-Command([int]$command) {
  $window = Open-Menu
  $menu = [UndoUi]::SendMessage($window, 0x01E1, [IntPtr]::Zero, [IntPtr]::Zero)
  for ($i=0; $i -lt [UndoUi]::GetMenuItemCount($menu); $i++) {
    if ([UndoUi]::GetMenuItemID($menu, $i) -eq $command) { Click-MenuItem $window $menu $i; return }
    $sub = [UndoUi]::GetSubMenu($menu, $i)
    if ($sub -ne [IntPtr]::Zero) {
      for ($j=0; $j -lt [UndoUi]::GetMenuItemCount($sub); $j++) {
        if ([UndoUi]::GetMenuItemID($sub, $j) -eq $command) {
          Click-MenuItem $window $menu $i
          $subWindow = MenuWindow $app.Id $sub
          Click-MenuItem $subWindow $sub $j
          return
        }
      }
    }
  }
  throw "Menu command $command not found."
}
function Dismiss-Warning {
  for ($i=0; $i -lt 50; $i++) {
    $dialog=[IntPtr]::Zero
    while (($dialog=[UndoUi]::NextWindow('#32770',$dialog)) -ne [IntPtr]::Zero) {
      [uint32]$owner=0; [void][UndoUi]::GetWindowThreadProcessId($dialog,[ref]$owner)
      if ($owner -eq $app.Id) {
        $button = if ([UndoUi]::GetDlgItem($dialog,1) -ne [IntPtr]::Zero) { 1 } else { 2 }
        [void][UndoUi]::PostMessage($dialog,0x0111,[IntPtr]$button,[IntPtr]::Zero); Start-Sleep -Milliseconds 200; return
      }
    }
    Start-Sleep -Milliseconds 50
  }
  throw 'Expected conflict warning did not appear.'
}
function Set-Active([guid]$scheme) {
  $code=[UndoUi]::PowerSetActiveScheme([IntPtr]::Zero,[ref]$scheme)
  if ($code -ne 0 -or [UndoUi]::Active() -ne $scheme) { throw "Cannot activate temporary scheme: $code" }
}
function Read-Values([guid]$scheme) {
  [uint32]$ac=0; [uint32]$dc=0
  if ([UndoUi]::PowerReadACValueIndex([IntPtr]::Zero,[ref]$scheme,[ref]$video,[ref]$display,[ref]$ac) -ne 0 -or [UndoUi]::PowerReadDCValueIndex([IntPtr]::Zero,[ref]$scheme,[ref]$video,[ref]$display,[ref]$dc) -ne 0) { throw 'Cannot read temporary values.' }
  return "$ac,$dc"
}
try {
  for ($i=0; $i -lt 3; $i++) {
    $ptr=[IntPtr]::Zero; $code=[UndoUi]::PowerDuplicateScheme([IntPtr]::Zero,[ref]$original,[ref]$ptr)
    if ($code -ne 0 -or $ptr -eq [IntPtr]::Zero) { throw "Cannot duplicate scheme: $code" }
    $copies += [Runtime.InteropServices.Marshal]::PtrToStructure($ptr,[type][guid]); [void][UndoUi]::LocalFree($ptr)
  }
  [guid]$a=$copies[0]; [guid]$b=$copies[1]; [guid]$c=$copies[2]
  if ([UndoUi]::PowerWriteACValueIndex([IntPtr]::Zero,[ref]$a,[ref]$video,[ref]$display,420) -ne 0 -or [UndoUi]::PowerWriteDCValueIndex([IntPtr]::Zero,[ref]$a,[ref]$video,[ref]$display,780) -ne 0) { throw 'Cannot set test values.' }
  $bIndex=-1
  for ([uint32]$i=0; $i -lt 900; $i++) {
    $id=[guid]::Empty; [uint32]$size=16
    if ([UndoUi]::PowerEnumerate([IntPtr]::Zero,[IntPtr]::Zero,[IntPtr]::Zero,16,$i,[ref]$id,[ref]$size) -ne 0) { break }
    if ($id -eq $b) { $bIndex=$i; break }
  }
  if ($bIndex -lt 0) { throw 'Temporary target scheme was not enumerated.' }
  Set-Active $a
  $app=Start-Process -FilePath (Join-Path $PSScriptRoot '..\build\PWSwitch.exe') -PassThru -WindowStyle Hidden
  $main=[IntPtr]::Zero
  for ($i=0; $i -lt 150 -and $main -eq [IntPtr]::Zero; $i++) { Start-Sleep -Milliseconds 100; if ($app.HasExited) { throw 'Application exited.' }; $main=[UndoUi]::MainWindow() }
  if ($main -eq [IntPtr]::Zero) { throw 'Main window unavailable.' }
  Select-Command (1000 + $bIndex)
  if ([UndoUi]::Active() -ne $b) { throw 'Switch through menu failed.' }
  Select-Command 3003
  if ([UndoUi]::Active() -ne $a) { throw 'Undo failed to restore prior scheme.' }
  Select-Command (1000 + $bIndex)
  if ([UndoUi]::Active() -ne $b) { throw 'Second switch through menu failed.' }
  Set-Active $c
  Select-Command 3003; Dismiss-Warning
  if ([UndoUi]::Active() -ne $c) { throw 'Undo overwrote an external scheme change.' }
  Set-Active $a
  Select-Command 2002
  if ((Read-Values $a) -ne '300,300') { throw 'Quick timeout change failed.' }
  Select-Command 3003
  if ((Read-Values $a) -ne '420,780') { throw 'Undo did not restore independent AC/DC values.' }
  Select-Command 2002
  if ((Read-Values $a) -ne '300,300') { throw 'Second quick timeout change failed.' }
  if ([UndoUi]::PowerWriteDCValueIndex([IntPtr]::Zero,[ref]$a,[ref]$video,[ref]$display,1200) -ne 0) { throw 'Cannot simulate external DC change.' }
  Set-Active $a
  Select-Command 3003; Dismiss-Warning
  if ((Read-Values $a) -ne '300,1200') { throw 'Undo overwrote an external timeout change.' }

  Select-Command 3000
  $editor=[IntPtr]::Zero
  for ($i=0; $i -lt 150 -and $editor -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 100
    $candidate=[UndoUi]::EditorWindow()
    if ($candidate -ne [IntPtr]::Zero) {
      [uint32]$owner=0; [void][UndoUi]::GetWindowThreadProcessId($candidate,[ref]$owner)
      if ($owner -eq $app.Id) { $editor=$candidate }
    }
  }
  if ($editor -eq [IntPtr]::Zero) { throw 'Advanced editor did not open.' }
  $settingName=[UndoUi]::SettingName($a,$video,$display)
  $list=[UndoUi]::GetDlgItem($editor,101)
  $count=[UndoUi]::SendMessage($list,0x018B,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32()
  $row=-1
  for ($i=0; $i -lt $count; $i++) {
    $length=[UndoUi]::SendMessage($list,0x018A,[IntPtr]$i,[IntPtr]::Zero).ToInt32()
    $text=New-Object System.Text.StringBuilder ($length + 1)
    [void][UndoUi]::SendText($list,0x0189,[IntPtr]$i,$text)
    if ($text.ToString().EndsWith($settingName)) { $row=$i; break }
  }
  if ($row -lt 0) { throw 'Display timeout not found in advanced editor.' }
  [void][UndoUi]::SendMessage($list,0x0186,[IntPtr]$row,[IntPtr]::Zero)
  [void][UndoUi]::SendMessage($editor,0x0111,[IntPtr](101 + (1 -shl 16)),$list)
  $acInput=[UndoUi]::GetDlgItem($editor,103)
  $newAc=New-Object System.Text.StringBuilder '5432'
  [void][UndoUi]::SendText($acInput,0x000C,[IntPtr]::Zero,$newAc)
  [void][UndoUi]::PostMessage($editor,0x0111,[IntPtr]105,[UndoUi]::GetDlgItem($editor,105))
  for ($attempt=0; $attempt -lt 100; $attempt++) {
    if ((Read-Values $a) -eq '5432,1200') { break }
    Start-Sleep -Milliseconds 50
  }
  if ((Read-Values $a) -ne '5432,1200') { throw 'Advanced editor did not apply AC independently.' }
  if ([UndoUi]::PowerWriteDCValueIndex([IntPtr]::Zero,[ref]$a,[ref]$video,[ref]$display,1500) -ne 0) { throw 'Cannot simulate external DC change after editor apply.' }
  Select-Command 3003
  if ((Read-Values $a) -ne '300,1500') { throw 'Undo of editor AC change did not restore AC while preserving external DC change.' }
  Write-Output 'Undo smoke passed: scheme, both-source timeout, and editor AC undo; external changes preserved.'
} finally {
  if ($app -and -not $app.HasExited) {
    if ($main -ne [IntPtr]::Zero) { [void][UndoUi]::PostMessage($main,0x0010,[IntPtr]::Zero,[IntPtr]::Zero) }
    if (-not $app.WaitForExit(5000)) { Stop-Process -Id $app.Id -Force }
  }
  $current=[UndoUi]::Active()
  if ($copies -contains $current) {
    $restore=[UndoUi]::PowerSetActiveScheme([IntPtr]::Zero,[ref]$original)
    if ($restore -ne 0 -or [UndoUi]::Active() -ne $original) { throw "Cannot restore original scheme: $restore" }
  } elseif ($current -ne $original) { Write-Warning "Active scheme changed externally to $current; it was not overwritten." }
  foreach ($item in $copies) { [guid]$copy=$item; $code=[UndoUi]::PowerDeleteScheme([IntPtr]::Zero,[ref]$copy); if ($code -ne 0) { Write-Error "Cannot remove temporary scheme ${copy}: $code" } }
  [void][UndoUi]::SetCursorPos($cursor.x,$cursor.y)
}
