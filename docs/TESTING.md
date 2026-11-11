# Testing PWSwitch

Run these commands from the repository root on Windows after installing the x64 C++ tools and Windows SDK. Each build switch compiles the corresponding test; run the executable or PowerShell script on the following line.

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

Additional tests `tests\undo_smoke.ps1`, `tests\live_extremes.ps1` and `tests\custom_sleep_smoke.ps1` temporarily switch the active plan and attempt to restore it afterward. Forced termination can prevent restoration. A [manual usability checklist](usability-check.md) is also available.
