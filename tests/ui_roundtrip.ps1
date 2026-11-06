param([switch]$SkipDeletedScheme)
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class RoundTrip {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern IntPtr FindWindow(string cls, string title);
  public static IntPtr FindClass(string cls) { return FindWindow(cls, null); }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendText(IntPtr hwnd, uint msg, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr hwnd, string text);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int max);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
  public delegate bool EnumProc(IntPtr hwnd, IntPtr state);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr state);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr parent, EnumProc callback, IntPtr state);
  [DllImport("user32.dll")] static extern int GetDlgCtrlID(IntPtr hwnd);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, StringBuilder name, int length);
  public static IntPtr[] Dialogs(uint pid) {
    var dialogs = new List<IntPtr>();
    EnumWindows((hwnd, state) => {
      uint owner; GetWindowThreadProcessId(hwnd, out owner);
      var name = new StringBuilder(256); GetClassName(hwnd, name, name.Capacity);
      if (owner == pid && name.ToString() == "#32770") dialogs.Add(hwnd);
      return true;
    }, IntPtr.Zero);
    return dialogs.ToArray();
  }
  public static IntPtr[] PopupMenus(uint pid) {
    var menus = new List<IntPtr>();
    EnumWindows((hwnd, state) => {
      uint owner; GetWindowThreadProcessId(hwnd, out owner);
      var name = new StringBuilder(256); GetClassName(hwnd, name, name.Capacity);
      if (owner == pid && name.ToString() == "#32768" && IsWindowVisible(hwnd)) menus.Add(hwnd);
      return true;
    }, IntPtr.Zero);
    return menus.ToArray();
  }
  public static string Children(IntPtr hwnd) {
    var items = new List<string>();
    EnumChildWindows(hwnd, (child, state) => { var n = new StringBuilder(256); var t = new StringBuilder(512); GetClassName(child,n,n.Capacity); GetWindowText(child,t,t.Capacity); items.Add(n + ":" + GetDlgCtrlID(child) + ":" + t); return true; }, IntPtr.Zero);
    return string.Join(",", items.ToArray());
  }
  [DllImport("powrprof.dll")] public static extern uint PowerGetActiveScheme(IntPtr root, out IntPtr scheme);
  [DllImport("powrprof.dll")] public static extern uint PowerDuplicateScheme(IntPtr root, ref Guid source, out IntPtr scheme);
  [DllImport("powrprof.dll")] public static extern uint PowerDeleteScheme(IntPtr root, ref Guid scheme);
  [DllImport("powrprof.dll")] public static extern uint PowerEnumerate(IntPtr root, IntPtr scheme, IntPtr group, int access, uint index, out Guid result, ref uint size);
  [DllImport("powrprof.dll")] public static extern uint PowerReadFriendlyName(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, IntPtr buf, ref uint size);
  [DllImport("powrprof.dll")] public static extern uint PowerReadACValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("powrprof.dll")] public static extern uint PowerReadDCValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("powrprof.dll")] public static extern uint PowerWriteACValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, uint value);
  [DllImport("powrprof.dll", EntryPoint="PowerEnumerate")] static extern uint EnumeratePtr(IntPtr root, IntPtr scheme, IntPtr group, int access, uint index, out Guid result, ref uint size);
  [DllImport("powrprof.dll", EntryPoint="PowerReadFriendlyName")] static extern uint FriendlyNamePtr(IntPtr root, IntPtr scheme, IntPtr group, IntPtr setting, IntPtr buf, ref uint size);
  [DllImport("powrprof.dll")] static extern uint PowerReadSettingAttributes(ref Guid group, ref Guid setting);
  [DllImport("kernel32.dll")] public static extern IntPtr LocalFree(IntPtr h);
  public static Guid Active() { IntPtr p; if (PowerGetActiveScheme(IntPtr.Zero, out p) != 0) throw new Exception("active read failed"); try { return (Guid)Marshal.PtrToStructure(p, typeof(Guid)); } finally { LocalFree(p); } }
  public static string Name(Guid scheme, Guid group, Guid setting) { uint n=0; uint r=PowerReadFriendlyName(IntPtr.Zero,ref scheme,ref group,ref setting,IntPtr.Zero,ref n); if ((r!=0 && r!=234)||n==0) throw new Exception("name size failed"); IntPtr p=Marshal.AllocHGlobal((int)n); try { if (PowerReadFriendlyName(IntPtr.Zero,ref scheme,ref group,ref setting,p,ref n)!=0) throw new Exception("name failed"); return Marshal.PtrToStringUni(p); } finally { Marshal.FreeHGlobal(p); } }
  public sealed class EditorRow { public Guid Group, Setting; public string Label; }
  static string FriendlyName(IntPtr scheme, IntPtr group, IntPtr setting, Guid fallback) {
    uint size=0; uint status=FriendlyNamePtr(IntPtr.Zero,scheme,group,setting,IntPtr.Zero,ref size);
    if ((status!=0 && status!=234) || size==0) return fallback.ToString("B").ToUpperInvariant();
    IntPtr buffer=Marshal.AllocHGlobal((int)size);
    try { if (FriendlyNamePtr(IntPtr.Zero,scheme,group,setting,buffer,ref size)!=0) return fallback.ToString("B").ToUpperInvariant(); string name=Marshal.PtrToStringUni(buffer); return string.IsNullOrEmpty(name) ? fallback.ToString("B").ToUpperInvariant() : name; }
    finally { Marshal.FreeHGlobal(buffer); }
  }
  public static EditorRow[] VisibleRows(Guid scheme) {
    var rows=new List<EditorRow>();
    IntPtr schemePtr=Marshal.AllocHGlobal(16), groupPtr=Marshal.AllocHGlobal(16), settingPtr=Marshal.AllocHGlobal(16);
    try {
      Marshal.StructureToPtr(scheme,schemePtr,false);
      for (uint g=0;;g++) {
        Guid group; uint size=16; uint status=EnumeratePtr(IntPtr.Zero,schemePtr,IntPtr.Zero,17,g,out group,ref size);
        if (status==259) break;
        if (status!=0) throw new Exception("Group enumeration failed: " + status);
        Marshal.StructureToPtr(group,groupPtr,false);
        string groupName=FriendlyName(schemePtr,groupPtr,IntPtr.Zero,group);
        for (uint s=0;;s++) {
          Guid setting; size=16; status=EnumeratePtr(IntPtr.Zero,schemePtr,groupPtr,18,s,out setting,ref size);
          if (status==259) break;
          if (status!=0) throw new Exception("Setting enumeration failed: " + status);
          if ((PowerReadSettingAttributes(ref group,ref setting) & 1)!=0) continue;
          Marshal.StructureToPtr(setting,settingPtr,false);
          rows.Add(new EditorRow { Group=group, Setting=setting, Label=groupName + "  " + (char)0x203A + "  " + FriendlyName(schemePtr,groupPtr,settingPtr,setting) });
        }
      }
      return rows.ToArray();
    } finally { Marshal.FreeHGlobal(schemePtr); Marshal.FreeHGlobal(groupPtr); Marshal.FreeHGlobal(settingPtr); }
  }
}
'@
function Reply-ToDialog($processId, $buttonId, $scenario, $optional = $false) {
  $dialogs = @()
  for ($attempt = 0; $attempt -lt 100 -and $dialogs.Count -eq 0; $attempt++) {
    Start-Sleep -Milliseconds 50
    $dialogs = @([RoundTrip]::Dialogs([uint32]$processId))
  }
  if ($dialogs.Count -eq 0 -and $optional) { return }
  if ($dialogs.Count -ne 1) { throw "${scenario}: expected one confirmation or error dialog; found $($dialogs.Count)." }
  $beforeChildren = [RoundTrip]::Children($dialogs[0])
  $actualButton = $buttonId
  if ($buttonId -eq 1 -and [RoundTrip]::GetDlgItem($dialogs[0], 1) -eq [IntPtr]::Zero -and [RoundTrip]::GetDlgItem($dialogs[0], 2) -ne [IntPtr]::Zero) { $actualButton = 2 }
  [void][RoundTrip]::PostMessage($dialogs[0], 0x0111, [IntPtr]$actualButton, [IntPtr]::Zero)
  for ($attempt = 0; $attempt -lt 100 -and [RoundTrip]::IsWindow($dialogs[0]); $attempt++) { Start-Sleep -Milliseconds 50 }
  if ([RoundTrip]::IsWindow($dialogs[0])) {
    $title = New-Object System.Text.StringBuilder 256
    [void][RoundTrip]::GetWindowText($dialogs[0], $title, 256)
    $ok = [RoundTrip]::GetDlgItem($dialogs[0], 1); $no = [RoundTrip]::GetDlgItem($dialogs[0], 7)
    $visible = [RoundTrip]::IsWindowVisible($dialogs[0])
    $children = [RoundTrip]::Children($dialogs[0])
    throw "${scenario}: dialog did not close after response (button=$buttonId, title=$title, ok=$ok, no=$no, visible=$visible, before=$beforeChildren, children=$children)."
  }
}
function Select-TentativeSchemeWithKey($processId, $schemes, $initialSelection) {
  $schemeCount = [RoundTrip]::SendMessage($schemes, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() # CB_GETCOUNT
  if ($schemeCount -lt 2) { throw 'Need at least two schemes for keyboard selection.' }
  $key = if ($initialSelection -lt $schemeCount - 1) { 0x28 } else { 0x26 } # VK_DOWN or VK_UP at the end
  [void][RoundTrip]::SendMessage($schemes, 0x014F, [IntPtr]1, [IntPtr]::Zero) # CB_SHOWDROPDOWN
  if ([RoundTrip]::SendMessage($schemes, 0x0157, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -eq 0) { throw 'The scheme drop-down did not open.' }
  if (-not [RoundTrip]::PostMessage($schemes, 0x0100, [IntPtr]$key, [IntPtr]::Zero)) { throw 'Could not post the scheme navigation key.' }
  $changed = $false
  for ($attempt = 0; $attempt -lt 100; $attempt++) {
    Start-Sleep -Milliseconds 20
    if (@([RoundTrip]::Dialogs([uint32]$processId)).Count -gt 0) {
      Reply-ToDialog $processId 7 'Navigating an open scheme drop-down'
      throw 'Navigating an open scheme drop-down prompted to discard a draft.'
    }
    if ([RoundTrip]::SendMessage($schemes, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $initialSelection) { $changed = $true; break }
  }
  if (-not $changed) { throw 'The posted arrow key did not change the actual combo-box selection; keyboard test is inconclusive.' }
  if ([RoundTrip]::SendMessage($schemes, 0x0157, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -eq 0) { throw 'Navigating closed the scheme drop-down before confirmation.' }
}
if ([RoundTrip]::FindClass('PWSwitchMain') -ne [IntPtr]::Zero) { throw 'PWSwitch is already running.' }
$active = [RoundTrip]::Active()
$video = [guid]'7516b95f-f776-4464-8c53-06167f40cc99'
$display = [guid]'3c0bc021-c8a8-4e07-a973-6b14cbcb2b7e'
$copyPtr = [IntPtr]::Zero
$code = [RoundTrip]::PowerDuplicateScheme([IntPtr]::Zero, [ref]$active, [ref]$copyPtr)
if ($code -ne 0 -or $copyPtr -eq [IntPtr]::Zero) { throw "Cannot duplicate scheme: $code" }
$copy = [Runtime.InteropServices.Marshal]::PtrToStructure($copyPtr, [type][guid])
[void][RoundTrip]::LocalFree($copyPtr)
$p = $null
$copyDeleted = $false
$removedCopy = [guid]::Empty
$removedCopyDeleted = $false
try {
  $removedPtr = [IntPtr]::Zero
  $code = [RoundTrip]::PowerDuplicateScheme([IntPtr]::Zero, [ref]$active, [ref]$removedPtr)
  if ($code -ne 0 -or $removedPtr -eq [IntPtr]::Zero) { throw "Cannot duplicate second scheme: $code" }
  $removedCopy = [Runtime.InteropServices.Marshal]::PtrToStructure($removedPtr, [type][guid])
  [void][RoundTrip]::LocalFree($removedPtr)
  [uint32]$dcBefore = 0
  if ([RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$dcBefore) -ne 0) { throw 'Cannot read copied DC value.' }
  $schemeIndex = -1; $removedIndex = -1; $otherIndex = -1
  for ([uint32]$i = 0; $i -lt 1000; $i++) {
    $id = [guid]::Empty; [uint32]$size = 16
    $status = [RoundTrip]::PowerEnumerate([IntPtr]::Zero, [IntPtr]::Zero, [IntPtr]::Zero, 16, $i, [ref]$id, [ref]$size)
    if ($status -ne 0) { break }
    if ($id -eq $copy) { $schemeIndex = $i }
    elseif ($id -eq $removedCopy) { $removedIndex = $i }
    else { $otherIndex = $i }
  }
  if ($schemeIndex -lt 0 -or $removedIndex -lt 0 -or $otherIndex -lt 0) { throw 'Required schemes not enumerated.' }
  $name = [RoundTrip]::Name($copy, $video, $display)
  $p = Start-Process -FilePath (Join-Path $PSScriptRoot '..\build\PWSwitch.exe') -ArgumentList '--settings' -PassThru -WindowStyle Hidden
  $editor = [IntPtr]::Zero
  for ($i = 0; $i -lt 300 -and $editor -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 100
    if ($p.HasExited) { throw 'App exited during startup.' }
    $editor = [RoundTrip]::FindClass('PWSwitchEditor')
  }
  if ($editor -eq [IntPtr]::Zero) { throw 'Editor unavailable.' }
  $schemes = [RoundTrip]::GetDlgItem($editor, 100)
  [void][RoundTrip]::SendMessage($schemes, 0x014E, [IntPtr]$schemeIndex, [IntPtr]::Zero)
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr](100 + (1 -shl 16)), $schemes)
  $list = [RoundTrip]::GetDlgItem($editor, 101)
  $count = [RoundTrip]::SendMessage($list, 0x018B, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
  # Match the editor's visible rows to Windows GUIDs, then exercise a real
  # enumerated value on the inactive temporary copy.
  $visibleRows = @([RoundTrip]::VisibleRows($copy))
  if ($visibleRows.Count -ne $count) { throw "Windows/editor visible setting counts differ: Windows=$($visibleRows.Count) editor=$count" }
  if ([RoundTrip]::Active() -ne $active -or $copy -eq $active) { throw 'Temporary scheme is not safely inactive.' }
  $enumRow = -1; $enumGroup = [guid]::Empty; $enumSetting = [guid]::Empty
  [uint32]$enumBefore = 0; [uint32]$enumDcBefore = 0; [uint32]$enumOriginalBefore = 0; [uint32]$enumOriginalDcBefore = 0
  for ($i = 0; $i -lt $count; $i++) {
    $length = [RoundTrip]::SendMessage($list, 0x018A, [IntPtr]$i, [IntPtr]::Zero).ToInt32()
    $rowText = New-Object System.Text.StringBuilder ($length + 1)
    [void][RoundTrip]::SendText($list, 0x0189, [IntPtr]$i, $rowText)
    if ($rowText.ToString() -ne $visibleRows[$i].Label) { throw "Cannot safely map editor row $i to its Windows GUID." }
    [void][RoundTrip]::SendMessage($list, 0x0186, [IntPtr]$i, [IntPtr]::Zero)
    [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr](101 + (1 -shl 16)), $list)
    $candidateInput = [RoundTrip]::GetDlgItem($editor, 103)
    $optionCount = [RoundTrip]::SendMessage($candidateInput, 0x0146, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    if ($optionCount -lt 2 -or -not [RoundTrip]::IsWindowEnabled([RoundTrip]::GetDlgItem($editor, 105))) { continue }
    $group = [guid]$visibleRows[$i].Group; $setting = [guid]$visibleRows[$i].Setting
    if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$group, [ref]$setting, [ref]$enumBefore) -ne 0 -or
        [RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$group, [ref]$setting, [ref]$enumDcBefore) -ne 0 -or
        [RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$active, [ref]$group, [ref]$setting, [ref]$enumOriginalBefore) -ne 0 -or
        [RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$active, [ref]$group, [ref]$setting, [ref]$enumOriginalDcBefore) -ne 0) { continue }
    $selectedOption = [RoundTrip]::SendMessage($candidateInput, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    if ($selectedOption -ne $enumBefore) { continue }
    $enumRow = $i; $enumGroup = $group; $enumSetting = $setting; break
  }
  if ($enumRow -lt 0) { throw 'No readable enumerated parameter with at least two valid options was found.' }
  $enumTarget = if ($enumBefore -eq 0) { 1 } else { 0 }
  if ([RoundTrip]::SendMessage($candidateInput, 0x014E, [IntPtr]$enumTarget, [IntPtr]::Zero).ToInt32() -ne $enumTarget) { throw 'Could not select the alternative enumerated option.' }
  if (-not [RoundTrip]::PostMessage($editor, 0x0111, [IntPtr]105, [RoundTrip]::GetDlgItem($editor, 105))) { throw 'Could not request application of the enumerated option.' }
  [uint32]$enumAfter = 0
  for ($attempt = 0; $attempt -lt 100; $attempt++) {
    Start-Sleep -Milliseconds 50
    if (@([RoundTrip]::Dialogs([uint32]$p.Id)).Count -gt 0) { throw 'Applying the enumerated option produced an unexpected dialog.' }
    if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$enumGroup, [ref]$enumSetting, [ref]$enumAfter) -ne 0) { throw 'Could not read the enumerated option from the temporary scheme.' }
    if ($enumAfter -eq $enumTarget) { break }
  }
  if ($enumAfter -ne $enumTarget) { throw "Enumerated option was not saved to the temporary scheme: expected=$enumTarget actual=$enumAfter" }
  [uint32]$enumDcAfter = 0; [uint32]$enumOriginalAfter = 0; [uint32]$enumOriginalDcAfter = 0
  if ([RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$enumGroup, [ref]$enumSetting, [ref]$enumDcAfter) -ne 0 -or $enumDcAfter -ne $enumDcBefore) { throw 'Applying the enumerated AC option changed the copied DC value.' }
  if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$active, [ref]$enumGroup, [ref]$enumSetting, [ref]$enumOriginalAfter) -ne 0 -or $enumOriginalAfter -ne $enumOriginalBefore -or
      [RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$active, [ref]$enumGroup, [ref]$enumSetting, [ref]$enumOriginalDcAfter) -ne 0 -or $enumOriginalDcAfter -ne $enumOriginalDcBefore -or [RoundTrip]::Active() -ne $active) { throw 'The original or active scheme changed during the enumerated option test.' }
  $row = -1
  for ($i = 0; $i -lt $count; $i++) {
    $length = [RoundTrip]::SendMessage($list, 0x018A, [IntPtr]$i, [IntPtr]::Zero).ToInt32()
    $text = New-Object System.Text.StringBuilder ($length + 1)
    [void][RoundTrip]::SendText($list, 0x0189, [IntPtr]$i, $text)
    if ($text.ToString().EndsWith($name)) { $row = $i; break }
  }
  if ($row -lt 0) { throw 'Display timeout not shown in editor.' }
  [void][RoundTrip]::SendMessage($list, 0x0186, [IntPtr]$row, [IntPtr]::Zero)
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr](101 + (1 -shl 16)), $list)
  $input = [RoundTrip]::GetDlgItem($editor, 103)
  $schemeSelection = [RoundTrip]::SendMessage($schemes, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
  $rowSelection = [RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
  $beforeText = New-Object System.Text.StringBuilder 256
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $beforeText)
  $replacement = New-Object System.Text.StringBuilder '1234'
  [void][RoundTrip]::SendText($input, 0x000C, [IntPtr]::Zero, $replacement)
  $dcInput = [RoundTrip]::GetDlgItem($editor, 104)
  $dcDraft = if ($dcBefore -eq 2345) { 2346 } else { 2345 }
  $dcReplacement = New-Object System.Text.StringBuilder ([string]$dcDraft)
  [void][RoundTrip]::SendText($dcInput, 0x000C, [IntPtr]::Zero, $dcReplacement)
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr]105, [RoundTrip]::GetDlgItem($editor, 105))
  [uint32]$acAfter = 0; [uint32]$dcAfter = 0
  if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$acAfter) -ne 0 -or $acAfter -ne 1234) {
    $diagnostic = New-Object System.Text.StringBuilder 256
    [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $diagnostic)
    $enabled = [RoundTrip]::IsWindowEnabled([RoundTrip]::GetDlgItem($editor, 105))
    throw "Editor failed to write AC value on temporary scheme; actual=$acAfter input=$diagnostic before=$beforeText enabled=$enabled row=$row selectedRow=$rowSelection schemeIndex=$schemeIndex selectedScheme=$schemeSelection"
  }
  if ([RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$dcAfter) -ne 0 -or $dcAfter -ne $dcBefore) { throw 'Editor unexpectedly changed DC value.' }
  $dcText = New-Object System.Text.StringBuilder 256
  [void][RoundTrip]::SendText($dcInput, 0x000D, [IntPtr]256, $dcText)
  if ($dcText.ToString() -ne [string]$dcDraft) { throw "Applying AC discarded the DC draft: expected=$dcDraft actual=$dcText" }
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr]106, [RoundTrip]::GetDlgItem($editor, 106))
  if ([RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$dcAfter) -ne 0 -or $dcAfter -ne $dcDraft) { throw "Editor failed to write DC draft: expected=$dcDraft actual=$dcAfter" }
  if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$acAfter) -ne 0 -or $acAfter -ne 1234) { throw 'Applying DC unexpectedly changed AC value.' }

  $acConflictDraft = New-Object System.Text.StringBuilder '5678'
  $dcConflictDraft = New-Object System.Text.StringBuilder '6789'
  [void][RoundTrip]::SendText($input, 0x000C, [IntPtr]::Zero, $acConflictDraft)
  [void][RoundTrip]::SendText($dcInput, 0x000C, [IntPtr]::Zero, $dcConflictDraft)
  $writeStatus = [RoundTrip]::PowerWriteACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, 4567)
  if ($writeStatus -ne 0) { throw "Cannot simulate external AC change: $writeStatus" }
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr]105, [RoundTrip]::GetDlgItem($editor, 105))
  Reply-ToDialog $p.Id 1 'Applying externally changed AC'
  if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$acAfter) -ne 0 -or $acAfter -ne 4567) { throw "Conflict overwrote external AC value: actual=$acAfter" }
  if ([RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$dcAfter) -ne 0 -or $dcAfter -ne $dcDraft) { throw 'Conflict unexpectedly changed stored DC value.' }
  $acAfterConflict = New-Object System.Text.StringBuilder 256
  $dcAfterConflict = New-Object System.Text.StringBuilder 256
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $acAfterConflict)
  [void][RoundTrip]::SendText($dcInput, 0x000D, [IntPtr]256, $dcAfterConflict)
  if ($acAfterConflict.ToString() -ne '5678' -or $dcAfterConflict.ToString() -ne '6789') { throw "Conflict discarded drafts: AC=$acAfterConflict DC=$dcAfterConflict" }
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr]105, [RoundTrip]::GetDlgItem($editor, 105))
  for ($attempt = 0; $attempt -lt 100; $attempt++) {
    if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$acAfter) -ne 0) { throw 'Cannot read AC after retry.' }
    if ($acAfter -eq 5678) { break }
    Start-Sleep -Milliseconds 50
  }
  if ($acAfter -ne 5678) { throw "Explicit retry did not apply AC draft: actual=$acAfter" }
  $dcAfterConflict.Clear() | Out-Null
  [void][RoundTrip]::SendText($dcInput, 0x000D, [IntPtr]256, $dcAfterConflict)
  if ($dcAfterConflict.ToString() -ne '6789') { throw 'Retrying AC discarded the DC draft.' }
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr]106, [RoundTrip]::GetDlgItem($editor, 106))
  for ($attempt = 0; $attempt -lt 100; $attempt++) {
    if ([RoundTrip]::PowerReadDCValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$dcAfter) -ne 0) { throw 'Cannot read DC after retry.' }
    if ($dcAfter -eq 6789) { break }
    Start-Sleep -Milliseconds 50
  }
  if ($dcAfter -ne 6789) { throw "Preserved DC draft could not be applied: actual=$dcAfter" }

  $draft = New-Object System.Text.StringBuilder '3456'
  [void][RoundTrip]::SendText($input, 0x000C, [IntPtr]::Zero, $draft)
  # Exercise the native combo box, not only manually posted notifications.
  Select-TentativeSchemeWithKey $p.Id $schemes $schemeIndex
  [void][RoundTrip]::PostMessage($schemes, 0x0100, [IntPtr]0x1B, [IntPtr]::Zero) # VK_ESCAPE
  for ($attempt = 0; $attempt -lt 100; $attempt++) {
    Start-Sleep -Milliseconds 20
    if (@([RoundTrip]::Dialogs([uint32]$p.Id)).Count -gt 0) {
      Reply-ToDialog $p.Id 7 'Cancelling scheme selection with Escape'
      throw 'Escape from the scheme drop-down prompted to discard a draft.'
    }
    if ([RoundTrip]::SendMessage($schemes, 0x0157, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -eq 0) { break }
  }
  if ([RoundTrip]::SendMessage($schemes, 0x0157, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne 0) { throw 'Escape did not close the scheme drop-down; keyboard test is inconclusive.' }
  $afterKeyCancel = New-Object System.Text.StringBuilder 256
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $afterKeyCancel)
  if ([RoundTrip]::SendMessage($schemes, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $schemeIndex -or [RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $row -or $afterKeyCancel.ToString() -ne '3456') { throw 'Escape from the scheme drop-down did not preserve context and draft.' }
  Select-TentativeSchemeWithKey $p.Id $schemes $schemeIndex
  [void][RoundTrip]::PostMessage($schemes, 0x0100, [IntPtr]0x0D, [IntPtr]::Zero) # VK_RETURN
  Reply-ToDialog $p.Id 7 'Confirming scheme selection with Enter'
  $afterKeyCancel.Clear() | Out-Null
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $afterKeyCancel)
  if ([RoundTrip]::SendMessage($schemes, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $schemeIndex -or [RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $row -or $afterKeyCancel.ToString() -ne '3456') { throw 'Declining discard after Enter did not preserve context and draft.' }
  # A highlighted scheme in an open drop-down is tentative until selection ends.
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr](100 + (7 -shl 16)), $schemes) # CBN_DROPDOWN
  [void][RoundTrip]::SendMessage($schemes, 0x014E, [IntPtr]$otherIndex, [IntPtr]::Zero)
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](100 + (1 -shl 16)), $schemes) # CBN_SELCHANGE
  for ($attempt = 0; $attempt -lt 50; $attempt++) {
    Start-Sleep -Milliseconds 20
    if (@([RoundTrip]::Dialogs([uint32]$p.Id)).Count -gt 0) {
      Reply-ToDialog $p.Id 7 'Tentative scheme selection'
      throw 'Tentative scheme selection prompted to discard a draft before confirmation.'
    }
  }
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](100 + (10 -shl 16)), $schemes) # CBN_SELENDCANCEL
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](100 + (8 -shl 16)), $schemes) # CBN_CLOSEUP
  for ($attempt = 0; $attempt -lt 50; $attempt++) {
    Start-Sleep -Milliseconds 20
    if (@([RoundTrip]::Dialogs([uint32]$p.Id)).Count -gt 0) {
      Reply-ToDialog $p.Id 7 'Cancelling tentative scheme selection'
      throw 'Cancelling tentative scheme selection prompted to discard a draft.'
    }
  }
  $afterDropCancel = New-Object System.Text.StringBuilder 256
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $afterDropCancel)
  if ([RoundTrip]::SendMessage($schemes, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $schemeIndex -or [RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $row -or $afterDropCancel.ToString() -ne '3456') { throw 'Cancelling tentative scheme selection did not preserve context and draft.' }
  if ($count -lt 2) { throw 'Need at least two settings to test cancellation.' }
  $otherRow = if ($row -eq 0) { 1 } else { 0 }
  [void][RoundTrip]::SendMessage($list, 0x0186, [IntPtr]$otherRow, [IntPtr]::Zero)
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](101 + (1 -shl 16)), $list)
  $dialogs = @()
  for ($attempt = 0; $attempt -lt 100 -and $dialogs.Count -eq 0; $attempt++) {
    Start-Sleep -Milliseconds 50
    $dialogs = @([RoundTrip]::Dialogs([uint32]$p.Id))
  }
  if ($dialogs.Count -ne 1 -or [RoundTrip]::GetDlgItem($dialogs[0], 7) -eq [IntPtr]::Zero) { throw 'Changing setting did not show exactly one discard confirmation.' }
  $confirmation = $dialogs[0]
  # A queued combo-box close may arrive during the modal message loop.
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](100 + (7 -shl 16)), $schemes)
  [void][RoundTrip]::SendMessage($schemes, 0x014E, [IntPtr]$otherIndex, [IntPtr]::Zero)
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](100 + (8 -shl 16)), $schemes)
  $repeat = Start-Process -FilePath (Join-Path $PSScriptRoot '..\build\PWSwitch.exe') -ArgumentList '--settings' -PassThru -WindowStyle Hidden
  if (-not $repeat.WaitForExit(5000) -or $repeat.ExitCode -ne 0) {
    if (-not $repeat.HasExited) { Stop-Process -Id $repeat.Id -Force }
    throw 'Repeated launch during confirmation did not exit successfully.'
  }
  $main = [RoundTrip]::FindClass('PWSwitchMain')
  if ($main -eq [IntPtr]::Zero -or -not [RoundTrip]::PostMessage($main, 0x8001, [IntPtr]1, [IntPtr]0x0205)) { throw 'Could not send the repeated tray click.' }
  for ($attempt = 0; $attempt -lt 50; $attempt++) {
    Start-Sleep -Milliseconds 20
    if (-not [RoundTrip]::IsWindow($editor) -or -not [RoundTrip]::IsWindow($main) -or -not [RoundTrip]::IsWindow($confirmation)) { throw 'Repeated tray click closed a window during discard confirmation.' }
    if (@([RoundTrip]::PopupMenus([uint32]$p.Id)).Count -ne 0) { throw 'Repeated tray click opened a menu during discard confirmation.' }
    if (@([RoundTrip]::Dialogs([uint32]$p.Id)).Count -ne 1) { throw 'Repeated tray click opened another dialog during discard confirmation.' }
  }
  Reply-ToDialog $p.Id 7 'Changing setting'
  $afterCancel = New-Object System.Text.StringBuilder 256
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $afterCancel)
  if ([RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $row -or $afterCancel.ToString() -ne '3456') { throw 'Declining discard after changing setting did not preserve selection and draft.' }
  [void][RoundTrip]::SendMessage($schemes, 0x014E, [IntPtr]$otherIndex, [IntPtr]::Zero)
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](100 + (1 -shl 16)), $schemes)
  Reply-ToDialog $p.Id 7 'Changing scheme'
  $afterCancel.Clear() | Out-Null
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $afterCancel)
  if ([RoundTrip]::SendMessage($schemes, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $schemeIndex -or [RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $row -or $afterCancel.ToString() -ne '3456') { throw 'Declining discard after changing scheme did not preserve context and draft.' }
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr]105, [RoundTrip]::GetDlgItem($editor, 105))
  if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$acAfter) -ne 0 -or $acAfter -ne 3456) { throw 'Draft could not be applied to the original context after cancellation.' }

  if (-not $SkipDeletedScheme) {
  $deleteStatus = [RoundTrip]::PowerDeleteScheme([IntPtr]::Zero, [ref]$removedCopy)
  if ($deleteStatus -ne 0) { throw "Cannot remove second temporary scheme before selection: $deleteStatus" }
  $removedCopyDeleted = $true
  [void][RoundTrip]::SendMessage($schemes, 0x014E, [IntPtr]$removedIndex, [IntPtr]::Zero)
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](100 + (1 -shl 16)), $schemes)
  Reply-ToDialog $p.Id 1 'Selecting deleted scheme' $true
  if ([RoundTrip]::SendMessage($schemes, 0x0147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $schemeIndex -or [RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $row) { throw 'Failed selection of deleted scheme did not restore prior context.' }
  $replacementAfterDelete = New-Object System.Text.StringBuilder '4567'
  [void][RoundTrip]::SendText($input, 0x000C, [IntPtr]::Zero, $replacementAfterDelete)
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr]105, [RoundTrip]::GetDlgItem($editor, 105))
  if ([RoundTrip]::PowerReadACValueIndex([IntPtr]::Zero, [ref]$copy, [ref]$video, [ref]$display, [ref]$acAfter) -ne 0 -or $acAfter -ne 4567) { throw 'Prior context was not writable after selection of deleted scheme.' }
  }
  if ([RoundTrip]::Active() -ne $active) { throw 'Editor unexpectedly changed active scheme.' }
  $deleteCopyStatus = [RoundTrip]::PowerDeleteScheme([IntPtr]::Zero, [ref]$copy)
  if ($deleteCopyStatus -ne 0) { throw "Cannot remove temporary scheme for unreadable-draft test: $deleteCopyStatus" }
  $copyDeleted = $true
  [void][RoundTrip]::SendMessage($list, 0x0186, [IntPtr]$otherRow, [IntPtr]::Zero)
  [void][RoundTrip]::SendMessage($editor, 0x0111, [IntPtr](101 + (1 -shl 16)), $list)
  if ([RoundTrip]::IsWindowEnabled([RoundTrip]::GetDlgItem($editor, 105))) { throw 'Apply remained enabled after the selected scheme was removed.' }
  $unreadableDraft = New-Object System.Text.StringBuilder '9876'
  [void][RoundTrip]::SendText($input, 0x000C, [IntPtr]::Zero, $unreadableDraft)
  [void][RoundTrip]::SendMessage($list, 0x0186, [IntPtr]$row, [IntPtr]::Zero)
  [void][RoundTrip]::PostMessage($editor, 0x0111, [IntPtr](101 + (1 -shl 16)), $list)
  Reply-ToDialog $p.Id 7 'Changing setting after read failure'
  $unreadableText = New-Object System.Text.StringBuilder 256
  [void][RoundTrip]::SendText($input, 0x000D, [IntPtr]256, $unreadableText)
  if ([RoundTrip]::SendMessage($list, 0x0188, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $otherRow -or $unreadableText.ToString() -ne '9876') { throw 'Draft after read failure was not protected.' }
  # Discard the intentional draft so cleanup can close the editor without interaction.
  [void][RoundTrip]::SendText($input, 0x000C, [IntPtr]::Zero, (New-Object System.Text.StringBuilder))
  if ($SkipDeletedScheme) { Write-Output 'UI round trip passed: independent drafts and cancelled context changes.' }
  else { Write-Output 'UI round trip passed: independent drafts, cancelled context changes, and deleted scheme recovery.' }
} finally {
  if ($p) {
    $main = [RoundTrip]::FindClass('PWSwitchMain')
    if ($main -ne [IntPtr]::Zero) { [void][RoundTrip]::PostMessage($main, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
    if (-not $p.WaitForExit(5000)) { Stop-Process -Id $p.Id -Force }
  }
  if (-not $copyDeleted) {
    $cleanup = [RoundTrip]::PowerDeleteScheme([IntPtr]::Zero, [ref]$copy)
    if ($cleanup -ne 0) { Write-Error "Temporary scheme $copy could not be removed: $cleanup" }
  }
  if ($removedCopy -ne [guid]::Empty -and -not $removedCopyDeleted) {
    $cleanupRemoved = [RoundTrip]::PowerDeleteScheme([IntPtr]::Zero, [ref]$removedCopy)
    if ($cleanupRemoved -ne 0) { Write-Error "Temporary scheme $removedCopy could not be removed: $cleanupRemoved" }
  }
}
