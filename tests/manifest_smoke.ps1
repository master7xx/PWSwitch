param([string]$Path = (Join-Path $PSScriptRoot '..\build\PWSwitch.exe'))
$ErrorActionPreference = 'Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ManifestReader {
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr LoadLibraryEx(string file, IntPtr reserved, uint flags);
  [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr FindResource(IntPtr module, IntPtr name, IntPtr type);
  [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr LoadResource(IntPtr module, IntPtr resource);
  [DllImport("kernel32.dll", SetLastError=true)] static extern uint SizeofResource(IntPtr module, IntPtr resource);
  [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr LockResource(IntPtr resource);
  [DllImport("kernel32.dll")] static extern bool FreeLibrary(IntPtr module);
  public static string Read(string file) {
    IntPtr module=LoadLibraryEx(file,IntPtr.Zero,0x2|0x20); // data/image mapping: no code execution
    if (module==IntPtr.Zero) throw new Exception("Cannot map executable as data: " + Marshal.GetLastWin32Error());
    try {
      IntPtr resource=FindResource(module,new IntPtr(1),new IntPtr(24));
      if (resource==IntPtr.Zero) throw new Exception("Executable has no embedded application manifest");
      uint n=SizeofResource(module,resource); IntPtr data=LockResource(LoadResource(module,resource));
      if (n==0 || data==IntPtr.Zero || n>1024*1024) throw new Exception("Invalid embedded manifest");
      byte[] bytes=new byte[n]; Marshal.Copy(data,bytes,0,(int)n);
      return Encoding.UTF8.GetString(bytes).TrimStart('\uFEFF').TrimEnd('\0');
    } finally { FreeLibrary(module); }
  }
}
'@
$absolute = (Resolve-Path -LiteralPath $Path).Path
[xml]$manifest = [ManifestReader]::Read($absolute)
$level = $manifest.SelectSingleNode("//*[local-name()='requestedExecutionLevel']")
if (-not $level -or $level.GetAttribute('level') -ne 'asInvoker' -or $level.GetAttribute('uiAccess') -ne 'false') {
  throw 'Executable must explicitly run asInvoker without uiAccess.'
}
$supported = $manifest.SelectNodes("//*[local-name()='supportedOS']") | ForEach-Object { $_.GetAttribute('Id') }
if ($supported -notcontains '{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}') { throw 'Windows 10/11 compatibility declaration missing.' }
Write-Output 'Manifest smoke passed: asInvoker, uiAccess=false, Windows 10/11 declared.'
