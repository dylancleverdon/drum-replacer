# Installs or updates the Killroom VST3 on Windows.
#
#   In PowerShell:  irm https://raw.githubusercontent.com/dylancleverdon/drum-replacer/HEAD/scripts/install-windows.ps1 | iex
#
# Run it again any time to update. The plugin's own "Update" button runs this same script.
# It asks for admin rights (UAC) because VST3 plugins live in Program Files.
#
# Parameters:
#   -Dest DIR   VST3 folder to install into (default: C:\Program Files\Common Files\VST3)
#   -Tag TAG    release to install, e.g. v1.0.3 (default: the latest release)
#   -Zip FILE   install from a local Killroom-Windows.zip instead of downloading
#   -NoPause    don't wait for Enter before closing (for CI)
#
# Prints KILLROOM_INSTALL_OK as its last line when the install worked.

param(
    [string]$Dest = "",
    [string]$Tag = "",
    [string]$Zip = "",
    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue' # makes Invoke-WebRequest much faster
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12

$Repo = if ($env:KILLROOM_REPO) { $env:KILLROOM_REPO } else { 'dylancleverdon/drum-replacer' }
$Asset = 'Killroom-Windows.zip'
$Bundle = 'Killroom.vst3'

if (-not $Dest) {
    $common = if ($env:CommonProgramW6432) { $env:CommonProgramW6432 } else { $env:CommonProgramFiles }
    $Dest = Join-Path $common 'VST3'
}

function Test-Admin {
    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Test-Writable([string]$dir) {
    try {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        $probe = Join-Path $dir ".killroom-write-test"
        [IO.File]::WriteAllText($probe, 'ok')
        Remove-Item $probe -Force
        return $true
    } catch {
        return $false
    }
}

# Returns 0 when installed, 1 on failure, 2 when an elevated copy took over.
function Install-Killroom {
    if (-not (Test-Writable $Dest) -and -not (Test-Admin)) {
        Write-Host "Killroom needs admin rights to install into $Dest"
        $script = $PSCommandPath
        if (-not $script) {
            # Running from "irm ... | iex": save a copy so the elevated window can run it.
            $script = Join-Path $env:TEMP 'killroom-install.ps1'
            Invoke-WebRequest -UseBasicParsing "https://raw.githubusercontent.com/$Repo/HEAD/scripts/install-windows.ps1" -OutFile $script
        }
        $arguments = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$script`"", '-Dest', "`"$Dest`"")
        if ($Tag) { $arguments += @('-Tag', $Tag) }
        if ($Zip) { $arguments += @('-Zip', "`"$((Resolve-Path $Zip).Path)`"") }
        if ($NoPause) { $arguments += '-NoPause' }
        try {
            Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments | Out-Null
            Write-Host 'Continuing in the admin window.'
            return 2
        } catch {
            Write-Host 'Error: admin rights were not granted, nothing was installed.'
            return 1
        }
    }

    $work = Join-Path ([IO.Path]::GetTempPath()) ("killroom-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $work | Out-Null

    try {
        $zipFile = $Zip
        if (-not $zipFile) {
            $url = if ($Tag) { "https://github.com/$Repo/releases/download/$Tag/$Asset" }
                   else      { "https://github.com/$Repo/releases/latest/download/$Asset" }
            Write-Host "Downloading $(if ($Tag) { $Tag } else { 'latest release' })"
            $zipFile = Join-Path $work $Asset
            Invoke-WebRequest -UseBasicParsing $url -OutFile $zipFile
        }

        Write-Host 'Unpacking'
        $unpacked = Join-Path $work 'unpacked'
        Expand-Archive -Path $zipFile -DestinationPath $unpacked -Force
        $new = Join-Path $unpacked $Bundle
        if (-not (Test-Path (Join-Path $new 'Contents'))) { throw "$Asset doesn't contain $Bundle" }

        $target = Join-Path $Dest $Bundle
        $binaryDir = Join-Path $target 'Contents\x86_64-win'
        $binary = Join-Path $binaryDir $Bundle

        Write-Host 'Installing'
        if (Test-Path $binaryDir) {
            # Clear out binaries left behind by earlier updates (skipped while one is still loaded).
            Get-ChildItem -Path $binaryDir -Filter "$Bundle.old-*" -ErrorAction SilentlyContinue |
                Remove-Item -Force -ErrorAction SilentlyContinue
        }

        # Live keeps the plugin binary locked while it's loaded, but Windows still allows renaming
        # it. Move it aside: the running Live keeps using it and loads the new one next start.
        if (Test-Path $binary) {
            try { Remove-Item -Path $binary -Force } catch { }
        }
        if (Test-Path $binary) {
            $old = "$Bundle.old-" + (Get-Date -Format 'yyyyMMddHHmmss')
            while ($true) {
                try {
                    Rename-Item -Path $binary -NewName $old
                    break
                } catch {
                    if ($NoPause) { throw }
                    Write-Host "Killroom is in use and can't be replaced. Close Ableton Live, then press Enter to try again."
                    Read-Host | Out-Null
                }
            }
        }

        robocopy $new $target /E /NFL /NDL /NJH /NJS /NP | Out-Null
        if ($LASTEXITCODE -ge 8) { throw "couldn't copy $Bundle into $Dest (robocopy exit code $LASTEXITCODE)" }

        $version = 'unknown'
        $info = Join-Path $target 'Contents\Resources\moduleinfo.json'
        if ((Test-Path $info) -and ((Get-Content $info -Raw) -match '"Version":\s*"([^"]+)"')) {
            $version = $Matches[1]
        }

        Write-Host "Installed Killroom $version to $target"
        Write-Host 'Restart Ableton Live to use it.'
        Write-Host "KILLROOM_INSTALL_OK $version"
        return 0
    } catch {
        Write-Host "Error: $($_.Exception.Message)"
        return 1
    } finally {
        Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
    }
}

$result = Install-Killroom

if ($result -ne 2 -and -not $NoPause) {
    Read-Host 'Press Enter to close' | Out-Null
}

# "exit" would close the window of someone who ran this with "irm ... | iex".
if ($PSCommandPath) {
    exit $(if ($result -eq 1) { 1 } else { 0 })
}
