$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class RelaunchUi {
  public delegate bool EnumProc(IntPtr hwnd, IntPtr state);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr state);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] static extern int GetClassName(IntPtr hwnd, StringBuilder name, int size);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
  public static IntPtr MainWindow() { return FindWindow("PWSwitchMain", null); }
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr w, IntPtr l);
  public static IntPtr[] Windows(uint pid, string cls) {
    var found = new List<IntPtr>();
    EnumWindows((hwnd, state) => {
      uint owner; GetWindowThreadProcessId(hwnd, out owner);
      var name = new StringBuilder(256); GetClassName(hwnd, name, name.Capacity);
      if (owner == pid && name.ToString() == cls) found.Add(hwnd);
      return true;
    }, IntPtr.Zero);
    return found.ToArray();
  }
}
'@

$exe = Join-Path $PSScriptRoot '..\build\PWSwitch.exe'
if ([RelaunchUi]::MainWindow() -ne [IntPtr]::Zero) { throw 'PWSwitch is already running; close it before the relaunch test.' }
$first = $null
$second = $null
$third = $null
$fourth = $null
try {
  $first = Start-Process -FilePath $exe -PassThru -WindowStyle Hidden
  $main = [IntPtr]::Zero
  for ($i = 0; $i -lt 150 -and $main -eq [IntPtr]::Zero; $i++) {
    Start-Sleep -Milliseconds 100
    if ($first.HasExited) { throw 'First instance exited during startup.' }
    $windows = @([RelaunchUi]::Windows([uint32]$first.Id, 'PWSwitchMain'))
    if ($windows.Count -gt 0) { $main = $windows[0] }
  }
  if ($main -eq [IntPtr]::Zero) { throw 'First instance did not create its main window.' }
  if (@([RelaunchUi]::Windows([uint32]$first.Id, 'PWSwitchEditor')).Count -ne 0) { throw 'First instance unexpectedly opened settings.' }

  $second = Start-Process -FilePath $exe -ArgumentList '--settings' -PassThru -WindowStyle Hidden
  if (-not $second.WaitForExit(15000)) { throw 'Second instance did not exit.' }
  if ($second.ExitCode -ne 0) { throw "Second instance failed: $($second.ExitCode)" }
  $settings = @()
  for ($i = 0; $i -lt 150 -and $settings.Count -eq 0; $i++) {
    Start-Sleep -Milliseconds 100
    if ($first.HasExited) { throw 'First instance exited after relaunch.' }
    $settings = @([RelaunchUi]::Windows([uint32]$first.Id, 'PWSwitchEditor'))
  }
  if ($settings.Count -ne 1) { throw "Expected one settings window in first instance; found $($settings.Count)." }
  $originalEditor = $settings[0]

  $third = Start-Process -FilePath $exe -ArgumentList '--settings' -PassThru -WindowStyle Hidden
  if (-not $third.WaitForExit(15000)) { throw 'Third instance did not exit.' }
  if ($third.ExitCode -ne 0) { throw "Third instance failed: $($third.ExitCode)" }
  for ($i = 0; $i -lt 20; $i++) {
    Start-Sleep -Milliseconds 100
    if ($first.HasExited) { throw 'First instance exited after repeated relaunch.' }
    $settings = @([RelaunchUi]::Windows([uint32]$first.Id, 'PWSwitchEditor'))
    if ($settings.Count -ne 1 -or $settings[0] -ne $originalEditor) { throw 'Repeated --settings did not reuse the existing settings window.' }
  }
  $fourth = Start-Process -FilePath $exe -PassThru -WindowStyle Hidden
  if (-not $fourth.WaitForExit(15000) -or $fourth.ExitCode -ne 0) { throw 'Ordinary repeat launch did not exit successfully.' }
  Start-Sleep -Milliseconds 300
  $settings = @([RelaunchUi]::Windows([uint32]$first.Id, 'PWSwitchEditor'))
  if ($settings.Count -ne 1 -or $settings[0] -ne $originalEditor) { throw 'Ordinary repeat launch did not reuse settings.' }
  Write-Output 'Relaunch smoke passed: existing instance opened and reused one settings window.'
} finally {
  if ($first -and -not $first.HasExited) {
    $ownedMain = @([RelaunchUi]::Windows([uint32]$first.Id, 'PWSwitchMain'))
    if ($ownedMain.Count -gt 0) { [void][RelaunchUi]::PostMessage($ownedMain[0], 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
    if (-not $first.WaitForExit(5000)) { Stop-Process -Id $first.Id -Force }
  }
  foreach ($process in @($second, $third, $fourth)) {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id -Force }
  }
}
