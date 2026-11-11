# PWSwitch

[Русский](README.ru.md) · [Español](README.es.md)

PWSwitch is a small Windows application for switching power plans from the notification area and adjusting their settings without digging through Control Panel.

The project was inspired by tools such as PowerPlanSwitcher. I wanted to make my own take on the idea: a straightforward native app that is convenient to use, offers more control over plan settings, and keeps the useful options close at hand. It is a personal project, and there is still room to improve.

Made by artificial intelligence under the supervision of an intoxicated intelligence.

## What it does

- Switch between Windows power plans from the tray. The icon's color suggests the plan's category; it is not a measurement of actual power consumption.
- Set display-off and sleep timeouts, or open the advanced editor to adjust AC and battery values separately. Settings normally hidden by Windows can be shown and are marked.
- Undo the most recent change made in PWSwitch during the current session.
- Optionally start with Windows, and choose English, Russian or Spanish from the tray menu. The app follows the Windows display language by default and falls back to English.

Plan names and setting descriptions come from Windows and are left unchanged. Windows supplies system dialog buttons, so those may use the system language. PWSwitch manages Windows power plans; it does not directly change the separate **Power mode** control in Windows Settings.

## Download and run

1. Download `PWSwitch-portable.zip` from the [latest release](https://github.com/master7xx/PWSwitch/releases/latest).
2. Extract the archive to a folder of your choice and run `PWSwitch.exe`.
3. Open the tray icon's menu. The icon may be behind the arrow for hidden notification-area icons.

Close the settings window to keep PWSwitch running in the tray; choose **Exit** from the tray menu to quit. The executable is unsigned, so Windows may show a warning. Autostart is off until you enable it in the menu.

PWSwitch targets 64-bit Intel/AMD Windows PCs. It has been tested on Windows 11; Windows 10 compatibility has not been verified separately. It uses C++, Win32 and the Windows Power API and needs no separate .NET or Visual C++ runtime installation.

## Build from source

1. Install Visual Studio or Visual Studio Build Tools with the **Desktop development with C++** workload, the x64 C++ tools and a Windows SDK.
2. Clone this repository or download and extract its source, then open PowerShell in the project folder.
3. Run:

   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
   ```

The result is `build\PWSwitch.exe`. Close any running copy of that executable before rebuilding it. The script locates the latest installed Visual C++ tools and builds a native x64 executable.

See [testing instructions](docs/TESTING.md) for the test commands and their effects on Windows power settings.

## Privacy and license

The application has no network, telemetry or automatic update features. The chosen interface language is stored under `HKCU\Software\PWSwitch\Language`; optional autostart is stored separately for the current user. The app does not request elevated privileges on its own.

PWSwitch is available under the [MIT License](LICENSE).
