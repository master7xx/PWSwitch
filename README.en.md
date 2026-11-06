# PWSwitch

Made by artificial intelligence under the supervision of an intoxicated intelligence.

PWSwitch is a small Windows x64 tray application for selecting power plans and editing their settings. It uses native Win32 and Windows Power API calls and does not require a separate .NET or Visual C++ runtime installation. It has been tested on Windows 11; Windows 10 compatibility has not been verified separately.

The interface is available in English, Russian and Spanish. It follows the Windows user interface language by default, falling back to English. You can change the language from the tray menu. All plan names and setting descriptions come from Windows and remain unchanged. Windows supplies the buttons in system dialogs, so those labels may use the system language.

Use the tray menu to choose a power plan, set display and sleep timeouts, open the advanced editor, undo the most recent power-setting change, or exit. Quick timeouts apply to both AC and battery. The editor shows the two values separately and can optionally show settings normally hidden by Windows. Autostart is optional, applies to the current user, and can also be disabled in Windows startup settings.

Build from the repository root with the x64 C++ tools and Windows SDK installed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

The result is `build\PWSwitch.exe`. See [README.md](README.md) for the full test commands. The application does not contain network, telemetry or automatic update features. The selected language is stored under `HKCU\Software\PWSwitch\Language`; autostart is stored separately when enabled. The local executable is unsigned; a source review or matching builds do not prove the absence of every vulnerability. [MIT License](LICENSE).
