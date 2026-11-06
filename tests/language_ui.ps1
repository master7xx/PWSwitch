param([string]$Executable = (Join-Path $PSScriptRoot '..\build\language-ui-test\language_ui_app.exe'))
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class LanguageUi {
  public delegate bool EnumProc(IntPtr hwnd, IntPtr state);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc callback, IntPtr state);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, ExactSpelling=true)] static extern int GetClassNameW(IntPtr hwnd, StringBuilder text, int count);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, ExactSpelling=true)] public static extern IntPtr FindWindowW(string cls, string title);
  public static IntPtr MainWindow() { return FindWindowW("PWSwitchMain", null); }
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd, int id);
  [DllImport("user32.dll", ExactSpelling=true)] public static extern bool PostMessageW(IntPtr hwnd, uint message, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode, ExactSpelling=true, EntryPoint="SendMessageW")] static extern IntPtr SendText(IntPtr hwnd, uint message, IntPtr w, StringBuilder text);
  public static string Text(IntPtr hwnd) { var text = new StringBuilder(16384); SendText(hwnd, 13, (IntPtr)text.Capacity, text); return text.ToString(); }
  public static bool SetText(IntPtr hwnd, string value) { return SendText(hwnd, 12, IntPtr.Zero, new StringBuilder(value)) != IntPtr.Zero; }
  public static IntPtr[] Windows(uint pid, string cls) {
    var result = new List<IntPtr>();
    EnumWindows((hwnd, state) => {
      uint owner; GetWindowThreadProcessId(hwnd, out owner);
      var name = new StringBuilder(256); GetClassNameW(hwnd, name, name.Capacity);
      if (owner == pid && name.ToString() == cls) result.Add(hwnd);
      return true;
    }, IntPtr.Zero);
    return result.ToArray();
  }
}
'@

if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) { throw "Build the separate test executable first: $Executable" }
if ([LanguageUi]::MainWindow() -ne [IntPtr]::Zero) { throw 'Close PWSwitch before running this test.' }
$token = [guid]::NewGuid().ToString('N')
$relativeRoot = "Software\PWSwitch-language-ui-$token"
$priorToken = [Environment]::GetEnvironmentVariable('PWSWITCH_LANGUAGE_TEST_ID', 'Process')
$app = $null
$forced = $false

function Read-IsolatedValue([string]$subkey, [string]$name) {
  $key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($subkey, $false)
  if (-not $key) { return $null }
  try { return $key.GetValue($name, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames) }
  finally { $key.Dispose() }
}
function Wait-Window([uint32]$pidValue, [string]$className) {
  for ($i = 0; $i -lt 150; $i++) {
    if ($app.HasExited) { throw "Test process exited before $className appeared: $($app.ExitCode)" }
    $windows = @([LanguageUi]::Windows($pidValue, $className))
    if ($windows.Count -eq 1) { return $windows[0] }
    if ($windows.Count -gt 1) { throw "More than one $className belongs to the test process." }
    Start-Sleep -Milliseconds 100
  }
  throw "Timed out waiting for $className."
}
function Wait-Text([IntPtr]$window, [string]$expected) {
  for ($i = 0; $i -lt 100; $i++) {
    if ($app.HasExited) { throw 'Test process exited while waiting for text.' }
    if ([LanguageUi]::Text($window) -eq $expected) { return }
    Start-Sleep -Milliseconds 100
  }
  throw "Expected text '$expected'; got '$([LanguageUi]::Text($window))'."
}
function Wait-Language([string]$expected) {
  for ($i = 0; $i -lt 100; $i++) {
    if ($app.HasExited) { throw 'Test process exited while waiting for the language preference.' }
    if ((Read-IsolatedValue "$relativeRoot\Software\PWSwitch" 'Language') -eq $expected) { return }
    Start-Sleep -Milliseconds 100
  }
  throw "The isolated language preference did not become '$expected'."
}
function Choose-Language([IntPtr]$main) {
  # Only the test executable intercepts TrackPopupMenu: first call chooses
  # Spanish and subsequent calls choose English. No power commands are chosen.
  if (-not [LanguageUi]::PostMessageW($main, 0x8001, [IntPtr]1, [IntPtr]0x0205)) { throw 'Could not request the test tray menu.' }
}
function Reply-Discard([uint32]$pidValue, [int]$buttonId) {
  $dialog = Wait-Window $pidValue '#32770'
  $button = [LanguageUi]::GetDlgItem($dialog, $buttonId)
  if ($button -eq [IntPtr]::Zero) { throw "Discard dialog has no button $buttonId." }
  if (-not [LanguageUi]::PostMessageW($button, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero)) { throw 'Could not answer discard confirmation.' }
  for ($i = 0; $i -lt 100; $i++) {
    if (@([LanguageUi]::Windows($pidValue, '#32770')).Count -eq 0) { return }
    Start-Sleep -Milliseconds 100
  }
  throw 'Discard confirmation did not close.'
}

try {
  $existingRoot = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($relativeRoot, $false)
  if ($existingRoot) { $existingRoot.Dispose(); throw 'Generated test root already exists; refusing to use it.' }
  [Environment]::SetEnvironmentVariable('PWSWITCH_LANGUAGE_TEST_ID', $token, 'Process')
  $app = Start-Process -FilePath $Executable -ArgumentList '--settings' -WindowStyle Hidden -PassThru
  $main = Wait-Window ([uint32]$app.Id) 'PWSwitchMain'
  $editor = Wait-Window ([uint32]$app.Id) 'PWSwitchEditor'
  if ((Read-IsolatedValue $relativeRoot 'TestOwner') -ne $token) { throw 'The isolated test root is missing or has the wrong owner.' }
  $apply = [LanguageUi]::GetDlgItem($editor, 105)
  $input = [LanguageUi]::GetDlgItem($editor, 103)
  if ($apply -eq [IntPtr]::Zero -or $input -eq [IntPtr]::Zero) { throw 'Required editor controls are missing.' }

  Choose-Language $main
  Wait-Language 'es'
  Wait-Text $editor 'PWSwitch — Ajustes avanzados'
  Wait-Text $apply 'Aplicar'
  if ((Read-IsolatedValue "$relativeRoot\Software\PWSwitch" 'Language') -ne 'es') { throw 'Spanish preference was not saved in the isolated root.' }

  $draft = if ([LanguageUi]::Text($input) -eq '123456') { '123457' } else { '123456' }
  if (-not [LanguageUi]::SetText($input, $draft) -or [LanguageUi]::Text($input) -ne $draft) { throw 'Could not set the pending AC draft.' }
  Choose-Language $main
  Reply-Discard ([uint32]$app.Id) 7 # IDNO: retain the Spanish language and draft.
  Wait-Text $editor 'PWSwitch — Ajustes avanzados'
  if ([LanguageUi]::Text($input) -ne $draft) { throw 'Declining language change lost the AC draft.' }
  if ((Read-IsolatedValue "$relativeRoot\Software\PWSwitch" 'Language') -ne 'es') { throw 'Declining language change modified the saved preference.' }

  Choose-Language $main
  Reply-Discard ([uint32]$app.Id) 6 # IDYES: discard and switch to English.
  Wait-Language 'en'
  Wait-Text $editor 'PWSwitch — Advanced settings'
  Wait-Text $apply 'Apply'
  if ((Read-IsolatedValue "$relativeRoot\Software\PWSwitch" 'Language') -ne 'en') { throw 'English preference was not saved in the isolated root.' }
  if ([LanguageUi]::Text($input) -eq $draft) { throw 'Confirming discard unexpectedly retained the draft.' }
} finally {
  if ($app -and -not $app.HasExited) {
    $owned = @([LanguageUi]::Windows([uint32]$app.Id, 'PWSwitchMain'))
    if ($owned.Count -eq 1) { [void][LanguageUi]::PostMessageW($owned[0], 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
    if (-not $app.WaitForExit(5000)) { $forced = $true; Stop-Process -Id $app.Id -Force; $app.WaitForExit() }
  }
  [Environment]::SetEnvironmentVariable('PWSWITCH_LANGUAGE_TEST_ID', $priorToken, 'Process')
  # A forced stop can bypass the harness destructor. Delete only the fresh,
  # uniquely named tree that contains this test's matching ownership marker.
  if ((Read-IsolatedValue $relativeRoot 'TestOwner') -eq $token) {
    [Microsoft.Win32.Registry]::CurrentUser.DeleteSubKeyTree($relativeRoot, $false)
  }
}
if ($forced) { throw 'Test process did not exit cleanly and had to be terminated.' }
if ($app.ExitCode -ne 0) { throw "Test process returned exit code $($app.ExitCode)." }
Write-Output 'Language UI test passed: Spanish, preserved draft on cancel, English on discard, isolated preference.'
