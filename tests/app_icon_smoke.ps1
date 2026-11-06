param([string]$Path = (Join-Path $PSScriptRoot '..\build\PWSwitch.exe'))
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class IconResourceCheck {
    [StructLayout(LayoutKind.Sequential)] public struct IconInfo { public bool icon; public uint x, y; public IntPtr mask, color; }
    [StructLayout(LayoutKind.Sequential)] public struct Bitmap { public int type, width, height, widthBytes; public ushort planes, bitsPixel; public IntPtr bits; }
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll")] public static extern bool FreeLibrary(IntPtr module);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr LoadImage(IntPtr module, IntPtr name, uint type, int width, int height, uint flags);
    [DllImport("user32.dll")] public static extern bool GetIconInfo(IntPtr icon, out IconInfo info);
    [DllImport("user32.dll")] public static extern bool DestroyIcon(IntPtr icon);
    [DllImport("gdi32.dll", EntryPoint="GetObjectW")] public static extern int GetBitmap(IntPtr bitmap, int size, out Bitmap info);
    [DllImport("gdi32.dll")] public static extern bool DeleteObject(IntPtr obj);
}
'@
$file = (Resolve-Path -LiteralPath $Path).Path
$module = [IconResourceCheck]::LoadLibraryEx($file, [IntPtr]::Zero, 0x22)
if ($module -eq [IntPtr]::Zero) { throw 'Could not load executable resources.' }
try {
    foreach ($size in @(16, 20, 24, 32, 48, 256)) {
        $icon = [IconResourceCheck]::LoadImage($module, [IntPtr]101, 1, $size, $size, 0)
        if ($icon -eq [IntPtr]::Zero) { throw "Application icon could not be loaded at $size pixels." }
        try {
            $info = New-Object IconResourceCheck+IconInfo
            if (-not [IconResourceCheck]::GetIconInfo($icon, [ref]$info)) { throw 'Cannot inspect icon.' }
            try {
                $bitmap = New-Object IconResourceCheck+Bitmap
                if ($info.color -eq [IntPtr]::Zero -or [IconResourceCheck]::GetBitmap($info.color, [Runtime.InteropServices.Marshal]::SizeOf($bitmap), [ref]$bitmap) -eq 0 -or $bitmap.width -ne $size -or $bitmap.height -ne $size) { throw "Unexpected icon dimensions at $size pixels." }
            } finally {
                if ($info.color -ne [IntPtr]::Zero) { [void][IconResourceCheck]::DeleteObject($info.color) }
                if ($info.mask -ne [IntPtr]::Zero) { [void][IconResourceCheck]::DeleteObject($info.mask) }
            }
        } finally { [void][IconResourceCheck]::DestroyIcon($icon) }
    }
    Write-Output 'Application icon resource passed: 16, 20, 24, 32, 48 and 256 pixels.'
} finally { [void][IconResourceCheck]::FreeLibrary($module) }
