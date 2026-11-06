param([string]$Executable = (Join-Path $PSScriptRoot '..\build\range_metadata_app.exe'))
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class RangeUi {
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr parent, IntPtr after, string cls, string title);
  public static IntPtr FindClass(string cls) { return FindWindow(cls, null); }
  public static IntPtr Description(IntPtr parent) { return FindWindowEx(parent, IntPtr.Zero, "EDIT", null); }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr parent, int id);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd, int command);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, EntryPoint="SendMessageW")] public static extern IntPtr SendText(IntPtr hwnd, uint msg, IntPtr w, StringBuilder text);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
  [DllImport("powrprof.dll")] static extern uint PowerGetActiveScheme(IntPtr root, out IntPtr scheme);
  [DllImport("powrprof.dll")] static extern uint PowerReadACValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("powrprof.dll")] static extern uint PowerReadDCValueIndex(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, out uint value);
  [DllImport("powrprof.dll")] static extern uint PowerReadFriendlyName(IntPtr root, ref Guid scheme, ref Guid group, ref Guid setting, IntPtr buffer, ref uint size);
  [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr p);
  static Guid group = new Guid("7516b95f-f776-4464-8c53-06167f40cc99"), setting = new Guid("3c0bc021-c8a8-4e07-a973-6b14cbcb2b7e");
  public static Guid Active() { IntPtr p; if(PowerGetActiveScheme(IntPtr.Zero,out p)!=0 || p==IntPtr.Zero) throw new Exception("Active scheme unavailable"); try { return (Guid)Marshal.PtrToStructure(p,typeof(Guid)); } finally { LocalFree(p); } }
  public static uint Value(Guid scheme, bool ac) { uint value; uint e=ac ? PowerReadACValueIndex(IntPtr.Zero,ref scheme,ref group,ref setting,out value) : PowerReadDCValueIndex(IntPtr.Zero,ref scheme,ref group,ref setting,out value); if(e!=0) throw new Exception("Value unavailable: " + e); return value; }
  public static string Name(Guid scheme) { uint size=0; uint e=PowerReadFriendlyName(IntPtr.Zero,ref scheme,ref group,ref setting,IntPtr.Zero,ref size); if((e!=0 && e!=234)||size==0) throw new Exception("Name unavailable"); IntPtr p=Marshal.AllocHGlobal((int)size); try { if(PowerReadFriendlyName(IntPtr.Zero,ref scheme,ref group,ref setting,p,ref size)!=0) throw new Exception("Name read failed"); return Marshal.PtrToStringUni(p); } finally { Marshal.FreeHGlobal(p); } }
  public static string Text(IntPtr hwnd) { var s=new StringBuilder(16384); SendText(hwnd,13,(IntPtr)s.Capacity,s); return s.ToString(); }
}
'@
if ([RangeUi]::FindClass('PWSwitchMain') -ne [IntPtr]::Zero) { throw 'Close PWSwitch before the range metadata UI test.' }
if (-not (Test-Path -LiteralPath $Executable)) { throw "Build the separate test executable first: $Executable" }
$original = [RangeUi]::Active()
$ac = [RangeUi]::Value($original, $true)
$dc = [RangeUi]::Value($original, $false)
$name = [RangeUi]::Name($original)
foreach ($mode in @('--test-denied-step', '--test-zero-step')) {
  $app = Start-Process -FilePath $Executable -ArgumentList @('--settings', $mode) -WindowStyle Hidden -PassThru
  try {
    $editor = [IntPtr]::Zero
    for ($i=0; $i -lt 150 -and $editor -eq [IntPtr]::Zero; $i++) {
      Start-Sleep -Milliseconds 100
      if ($app.HasExited) { throw 'Test application exited before opening settings.' }
      $editor = [RangeUi]::FindClass('PWSwitchEditor')
    }
    if ($editor -eq [IntPtr]::Zero) { throw 'Settings did not open.' }
    [void][RangeUi]::ShowWindow($editor, 5)
    # Include hidden settings so the test also works when Windows hides this setting.
    [void][RangeUi]::SendMessage([RangeUi]::GetDlgItem($editor, 102), 0xF5, [IntPtr]::Zero, [IntPtr]::Zero)
    $list = [RangeUi]::GetDlgItem($editor, 101)
    $count = [RangeUi]::SendMessage($list, 0x18B, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    $row = -1
    for ($i=0; $i -lt $count; $i++) {
      $length = [RangeUi]::SendMessage($list, 0x18A, [IntPtr]$i, [IntPtr]::Zero).ToInt32()
      $label = New-Object System.Text.StringBuilder ($length + 1)
      [void][RangeUi]::SendText($list, 0x189, [IntPtr]$i, $label)
      if ($label.ToString().EndsWith($name)) { $row = $i; break }
    }
    if ($row -lt 0) { throw 'Display timeout is absent from the list.' }
    [void][RangeUi]::SendMessage($list, 0x186, [IntPtr]$row, [IntPtr]::Zero)
    [void][RangeUi]::SendMessage($editor, 0x111, [IntPtr](101 + (1 -shl 16)), $list)
    foreach ($id in @(105, 106)) {
      if ([RangeUi]::IsWindowEnabled([RangeUi]::GetDlgItem($editor, $id))) { throw "${mode}: Apply $id is enabled without a reliable increment." }
    }
    $acInput = [RangeUi]::GetDlgItem($editor, 103)
    $dcInput = [RangeUi]::GetDlgItem($editor, 104)
    if (-not [RangeUi]::IsWindowVisible($acInput) -or -not [RangeUi]::IsWindowVisible($dcInput) -or [RangeUi]::Text($acInput) -ne [string]$ac -or [RangeUi]::Text($dcInput) -ne [string]$dc) { throw "${mode}: current AC/DC values are not displayed; AC=$([RangeUi]::Text($acInput)) expected=$ac visible=$([RangeUi]::IsWindowVisible($acInput)); DC=$([RangeUi]::Text($dcInput)) expected=$dc visible=$([RangeUi]::IsWindowVisible($dcInput))." }
    $description = [RangeUi]::Description($editor)
    $text = [RangeUi]::Text($description)
    $explained = $text.Contains('Редактирование недоступно: Windows не предоставила достоверный шаг диапазона.') -or
      $text.Contains('Editing is unavailable: Windows did not provide a valid range step.') -or
      $text.Contains('Edición no disponible: Windows no proporcionó un incremento válido.')
    if (-not $explained -or $text -match '(шаг|step|incremento): 0') { throw "${mode}: missing or misleading unavailable-step explanation: $text" }
    Write-Output "Range metadata UI passed: $mode; values visible, both Apply buttons disabled, reason displayed."
  } finally {
    $main = [RangeUi]::FindClass('PWSwitchMain')
    if ($main -ne [IntPtr]::Zero) { [void][RangeUi]::PostMessage($main, 0x10, [IntPtr]::Zero, [IntPtr]::Zero) }
    if (-not $app.WaitForExit(5000)) { Stop-Process -Id $app.Id -Force }
  }
}
if ([RangeUi]::Active() -ne $original -or [RangeUi]::Value($original, $true) -ne $ac -or [RangeUi]::Value($original, $false) -ne $dc) { throw 'Power state changed during read-only GUI checks.' }
