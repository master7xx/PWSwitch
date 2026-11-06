param([switch]$Tests, [switch]$IconTests, [switch]$UndoTests, [switch]$LabelsTests, [switch]$TransactionTests, [switch]$SystemTests, [switch]$RangeMetadataTests, [switch]$RangeMetadataUiTests, [switch]$OptionInputTests, [switch]$AutostartTests, [switch]$I18nTests, [switch]$I18nRegistryTests, [switch]$LanguageUiTests, [switch]$Candidate)
$ErrorActionPreference = 'Stop'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if (-not $vs) { throw 'Visual Studio C++ Build Tools are required.' }
$devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'build') | Out-Null
$target = if ($Tests) { 'tests\power_tests.cpp src\power.cpp src\power_transaction.cpp /Fe:build\power_tests.exe' } elseif ($IconTests) { 'tests\icon_style_tests.cpp src\icon_style.cpp src\icon_art.cpp src\menu_art.cpp /Fe:build\icon_style_tests.exe' } elseif ($UndoTests) { 'tests\undo_tests.cpp src\undo.cpp /Fe:build\undo_tests.exe' } elseif ($LabelsTests) { 'tests\labels_tests.cpp src\labels.cpp /Fe:build\labels_tests.exe' } elseif ($TransactionTests) { 'tests\transaction_tests.cpp src\power_transaction.cpp /Fe:build\transaction_tests.exe' } elseif ($SystemTests) { 'tests\system_commands_tests.cpp src\system_commands.cpp /Fe:build\system_commands_tests.exe' } elseif ($RangeMetadataTests) { 'tests\range_metadata_tests.cpp src\power_transaction.cpp /Fe:build\range_metadata_tests.exe' } elseif ($RangeMetadataUiTests) { 'tests\range_metadata_app.cpp src\power_transaction.cpp src\icon_style.cpp src\icon_art.cpp src\menu_art.cpp src\undo.cpp src\labels.cpp src\system_commands.cpp /Fe:build\range_metadata_app.exe' } elseif ($Candidate) { 'src\main.cpp src\power.cpp src\power_transaction.cpp src\icon_style.cpp src\icon_art.cpp src\menu_art.cpp src\undo.cpp src\labels.cpp src\system_commands.cpp /Fe:build\PWSwitch-next.exe' } else { 'src\main.cpp src\power.cpp src\power_transaction.cpp src\icon_style.cpp src\icon_art.cpp src\menu_art.cpp src\undo.cpp src\labels.cpp src\system_commands.cpp /Fe:build\PWSwitch.exe' }
if ($OptionInputTests) { $target = 'tests\option_input_tests.cpp src\power.cpp src\power_transaction.cpp src\icon_style.cpp src\icon_art.cpp src\menu_art.cpp src\undo.cpp src\labels.cpp src\system_commands.cpp /Fe:build\option_input_tests.exe' }
if ($AutostartTests) { $target = 'tests\autostart_tests.cpp src\autostart.cpp /Fe:build\autostart_tests.exe' }
if ($I18nTests) { $target = 'tests\i18n_tests.cpp /Fe:build\i18n_tests.exe' }
if ($I18nRegistryTests) { $target = 'tests\i18n_registry_tests.cpp /Fe:build\i18n_registry_tests.exe' }
if ($LanguageUiTests) {
    New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'build\language-ui-test') | Out-Null
    $target = 'tests\language_ui_app.cpp src\power.cpp src\power_transaction.cpp src\icon_style.cpp src\icon_art.cpp src\menu_art.cpp src\undo.cpp src\labels.cpp src\system_commands.cpp /Fe:build\language-ui-test\language_ui_app.exe'
}
$appBuild = -not ($Tests -or $IconTests -or $UndoTests -or $LabelsTests -or $TransactionTests -or $SystemTests -or $RangeMetadataTests -or $OptionInputTests -or $AutostartTests -or $I18nTests -or $I18nRegistryTests)
$withIconResource = $appBuild -or $IconTests
if ($appBuild -or $OptionInputTests) { $target = $target.Replace(' /Fe:', ' src\autostart.cpp /Fe:') }
$target = $target.Replace(' /Fe:', ' src\i18n.cpp /Fe:')
if ($withIconResource) { $target = $target.Replace(' /Fe:', ' build\app.res /Fe:') }
$line = 'call "' + $devcmd + '" -arch=x64 -host_arch=x64 >nul && '
if ($withIconResource) { $line += 'rc /nologo /fo build\app.res src\app.rc && ' }
$line += 'cl /nologo /EHsc /std:c++17 /W4 /O2 /utf-8 /MT /Brepro /DUNICODE /D_UNICODE /Fobuild\ ' + $target + ' /link /Brepro powrprof.lib user32.lib shell32.lib comctl32.lib gdi32.lib advapi32.lib'
if ($OptionInputTests) { $line += ' /SUBSYSTEM:CONSOLE' }
if ($appBuild) {
    $line += ' /MANIFEST:EMBED /MANIFESTUAC:NO /MANIFESTINPUT:src\app.manifest'
}
Push-Location $PSScriptRoot
try { & cmd.exe /d /c $line; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE } } finally { Pop-Location }
