# CI test for the Windows Setup.exe: silent install, reinstall while the plugin is loaded
# (the situation when Live is open), then uninstall.
#
#   powershell -ExecutionPolicy Bypass -File tests\test-setup-windows.ps1 Killroom-Windows-Setup.exe

param([Parameter(Mandatory = $true)][string]$Setup)

$ErrorActionPreference = 'Stop'
$setupPath = (Resolve-Path $Setup).Path
$common = if ($env:CommonProgramW6432) { $env:CommonProgramW6432 } else { $env:CommonProgramFiles }
$bundle = Join-Path $common 'VST3\Killroom.vst3'
$binary = Join-Path $bundle 'Contents\x86_64-win\Killroom.vst3'
$uninstaller = Join-Path $env:ProgramFiles 'Killroom\unins000.exe'

function Invoke-Setup {
    $process = Start-Process -FilePath $setupPath -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait -PassThru
    if ($process.ExitCode -ne 0) { throw "Setup exited with $($process.ExitCode)" }
    if (-not (Test-Path $binary)) { throw 'plugin binary missing after Setup' }
}

Write-Host '== install'
Invoke-Setup
if (-not (Test-Path $uninstaller)) { throw 'uninstaller missing' }

Write-Host '== reinstall while the plugin is loaded'
$holderScript = Join-Path ([IO.Path]::GetTempPath()) 'killroom-hold-setup.ps1'
@'
param([string]$Path)
Add-Type -Namespace Native -Name Kernel32 -MemberDefinition '[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern System.IntPtr LoadLibraryW(string path);'
if ([Native.Kernel32]::LoadLibraryW($Path) -eq [IntPtr]::Zero) { exit 3 }
Start-Sleep -Seconds 120
'@ | Set-Content -Path $holderScript -Encoding ASCII

$holder = Start-Process powershell.exe -PassThru -WindowStyle Hidden -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$holderScript`"", '-Path', "`"$binary`"")
Start-Sleep -Seconds 8
if ($holder.HasExited) { throw "couldn't load the plugin binary (exit code $($holder.ExitCode))" }

Invoke-Setup
if ($holder.HasExited) { throw 'the process that had the plugin loaded died during Setup' }
if (-not (Get-ChildItem (Split-Path $binary) -Filter 'Killroom.vst3.old-*')) { throw 'old binary was not moved aside' }
Stop-Process -Id $holder.Id -Force
Start-Sleep -Seconds 2

Write-Host '== uninstall'
Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait
# The uninstaller hands off to a copy of itself, so wait for the files to go.
for ($i = 0; $i -lt 60 -and (Test-Path $bundle); $i++) { Start-Sleep -Seconds 1 }
if (Test-Path $bundle) { throw 'plugin still present after uninstall' }

Write-Host 'setup tests passed'
