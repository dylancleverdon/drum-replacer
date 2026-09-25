# CI test for scripts/install-windows.ps1: fresh install, then an update while another process
# has the installed plugin loaded (the situation when Live is open).
#
#   powershell -ExecutionPolicy Bypass -File tests\test-installer-windows.ps1 Killroom-Windows.zip

param([Parameter(Mandatory = $true)][string]$Zip)

$ErrorActionPreference = 'Stop'
$installer = Join-Path $PSScriptRoot '..\scripts\install-windows.ps1'
$zipPath = (Resolve-Path $Zip).Path
$dest = Join-Path ([IO.Path]::GetTempPath()) ('killroom-test-' + [guid]::NewGuid().ToString('N'))
$binary = Join-Path $dest 'Killroom.vst3\Contents\x86_64-win\Killroom.vst3'

function Invoke-Installer {
    $output = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $installer -Zip $zipPath -Dest $dest -NoPause 2>&1
    $output | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "installer exited with $LASTEXITCODE" }
    if (-not ($output | Select-String -Pattern '^KILLROOM_INSTALL_OK' -Quiet)) { throw 'installer did not report success' }
}

Write-Host '== fresh install'
Invoke-Installer
if (-not (Test-Path $binary)) { throw 'plugin binary missing after install' }

Write-Host '== update while the plugin is loaded'
$holderScript = Join-Path ([IO.Path]::GetTempPath()) 'killroom-hold.ps1'
@'
param([string]$Path)
Add-Type -Namespace Native -Name Kernel32 -MemberDefinition '[DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] public static extern System.IntPtr LoadLibraryW(string path);'
if ([Native.Kernel32]::LoadLibraryW($Path) -eq [IntPtr]::Zero) { exit 3 }
Start-Sleep -Seconds 120
'@ | Set-Content -Path $holderScript -Encoding ASCII

$holder = Start-Process powershell.exe -PassThru -WindowStyle Hidden -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$holderScript`"", '-Path', "`"$binary`"")
Start-Sleep -Seconds 8
if ($holder.HasExited) { throw "couldn't load the plugin binary (exit code $($holder.ExitCode))" }

# Prove the binary really is locked, so the test exercises the rename path.
$locked = $false
try { [IO.File]::Open($binary, 'Open', 'ReadWrite', 'None').Close() } catch { $locked = $true }
if (-not $locked) { throw 'the loaded plugin binary was not locked; test is not meaningful' }

Invoke-Installer
if (-not (Test-Path $binary)) { throw 'plugin binary missing after update' }
if ($holder.HasExited) { throw 'the process that had the plugin loaded died during the update' }
if (-not (Get-ChildItem (Split-Path $binary) -Filter 'Killroom.vst3.old-*')) { throw 'old binary was not moved aside' }
Stop-Process -Id $holder.Id -Force
Start-Sleep -Seconds 2

Write-Host '== next update cleans up the old binary'
Invoke-Installer
if (Get-ChildItem (Split-Path $binary) -Filter 'Killroom.vst3.old-*') { throw 'old binary was not cleaned up' }

Remove-Item -Recurse -Force $dest
Write-Host 'installer tests passed'
