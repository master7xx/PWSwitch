# PWSwitch

[Русский](README.ru.md) · [Español](README.es.md)

Made by artificial intelligence under the supervision of an intoxicated intelligence.

Inspired by PowerPlanSwitcher.

PWSwitch is a small Windows x64 tray application for selecting power plans and editing their settings. It uses native Win32 and Windows Power API calls and does not require a separate .NET or Visual C++ runtime installation. It has been tested on Windows 11; Windows 10 compatibility has not been verified separately.

The interface is available in English, Russian and Spanish. It follows the Windows user interface language by default, falling back to English. You can change the language from the tray menu. All plan names and setting descriptions come from Windows and remain unchanged. Windows supplies the buttons in system dialogs, so those labels may use the system language.

Use the tray menu to choose a power plan, set display and sleep timeouts, open the advanced editor, undo the most recent power-setting change, or exit. Quick timeouts apply to both AC and battery. The editor shows the two values separately and can optionally show settings normally hidden by Windows. Autostart is optional, applies to the current user, and can also be disabled in Windows startup settings.

Build from the repository root with the x64 C++ tools and Windows SDK installed:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

The result is `build\PWSwitch.exe`. The application does not contain network, telemetry or automatic update features. The selected language is stored under `HKCU\Software\PWSwitch\Language`; autostart is stored separately when enabled. The local executable is unsigned; a source review or matching builds do not prove the absence of every vulnerability. [MIT License](LICENSE).

## Build and test

Install the x64 C++ tools and Windows SDK, then run from the repository root:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -Tests
.\build\power_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -IconTests
.\build\icon_style_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -UndoTests
.\build\undo_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -LabelsTests
.\build\labels_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -TransactionTests
.\build\transaction_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -SystemTests
.\build\system_commands_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -I18nTests
.\build\i18n_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -I18nRegistryTests
.\build\i18n_registry_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -LanguageUiTests
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\language_ui.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -AutostartTests
.\build\autostart_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -RangeMetadataTests
.\build\range_metadata_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -OptionInputTests
.\build\option_input_tests.exe
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -RangeMetadataUiTests
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\range_metadata_ui.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\manifest_smoke.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\app_icon_smoke.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\ui_smoke.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\ui_roundtrip.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\relaunch_smoke.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\tests\power_options_smoke.ps1
```

GUI tests require an active Windows desktop and no other PWSwitch instance. Write tests create, modify and delete a temporary copy of a power plan without changing the active plan. Windows policies or permissions may prevent changes. Checks for external changes and writes are separate operations, so concurrent changes cannot be completely excluded.

`power_options_smoke.ps1` briefly moves the mouse pointer and opens Windows Power Options, then closes the newly detected window and restores the pointer. It recognizes Russian and English window titles.

Additional tests `tests\undo_smoke.ps1`, `tests\live_extremes.ps1` and `tests\custom_sleep_smoke.ps1` temporarily switch the active plan and attempt to restore it afterward. Forced termination can prevent restoration. A [manual usability checklist](docs/usability-check.md) is also available.
