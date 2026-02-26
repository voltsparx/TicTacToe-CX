param(
    [ValidateSet("install", "test")]
    [string]$Mode = "",
    [switch]$NoGui,
    [switch]$UpdateExisting,
    [switch]$UninstallExisting,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

function Write-Info([string]$Message) {
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Warn([string]$Message) {
    Write-Host "Warning: $Message" -ForegroundColor Yellow
}

function Fail([string]$Message) {
    Write-Host "Error: $Message" -ForegroundColor Red
    exit 1
}

function Show-Usage {
    @"
Usage:
  powershell -ExecutionPolicy Bypass -File .\building-scripts\install-windows.ps1 [options]

Options:
  -Mode <install|test>      Install mode or test mode
  -NoGui                    Build with -DENABLE_SDL2=OFF
  -UpdateExisting           Update detected existing installation
  -UninstallExisting        Uninstall detected existing installation
  -Help                     Show this help
"@ | Write-Host
}

function Invoke-Tool {
    param(
        [Parameter(Mandatory = $true)][string]$Tool,
        [Parameter(Mandatory = $false)][string[]]$Args = @(),
        [Parameter(Mandatory = $true)][string]$FailureMessage
    )

    & $Tool @Args
    if ($LASTEXITCODE -ne 0) {
        Fail "$FailureMessage (exit code $LASTEXITCODE)."
    }
}

function Normalize-Path([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $null
    }
    try {
        return [System.IO.Path]::GetFullPath($Path).TrimEnd('\')
    } catch {
        return $Path.TrimEnd('\')
    }
}

function Resolve-ExistingBinary {
    $commandInfo = Get-Command tictactoe-cx -ErrorAction SilentlyContinue
    if ($commandInfo) {
        $candidate = $commandInfo.Source
        if (-not $candidate) {
            $candidate = $commandInfo.Path
        }
        if ($candidate -and (Test-Path $candidate)) {
            return (Resolve-Path $candidate).Path
        }
    }

    $knownPaths = @(
        (Join-Path $env:USERPROFILE "bin\tictactoe-cx.exe"),
        (Join-Path $env:LOCALAPPDATA "Programs\tictactoe-cx\tictactoe-cx.exe")
    )

    foreach ($path in $knownPaths) {
        if (Test-Path $path) {
            return (Resolve-Path $path).Path
        }
    }

    return $null
}

function Remove-InstalledBinary([string]$BinaryPath) {
    if ([string]::IsNullOrWhiteSpace($BinaryPath)) {
        Fail "No installed binary path provided."
    }
    if (-not (Test-Path $BinaryPath)) {
        Fail "Installed binary no longer exists: $BinaryPath"
    }

    Write-Info "Removing $BinaryPath"
    try {
        Remove-Item -Path $BinaryPath -Force -ErrorAction Stop
    } catch {
        Fail "Could not remove '$BinaryPath'. If it is in a protected directory, run PowerShell as Administrator. Details: $($_.Exception.Message)"
    }
    Write-Host "Removed: $BinaryPath" -ForegroundColor Green

    $installDir = Split-Path -Parent $BinaryPath
    $userPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if ($userPath) {
        $entries = $userPath -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
        $normalizedInstallDir = Normalize-Path $installDir
        $pathHasDir = $false
        foreach ($entry in $entries) {
            if ((Normalize-Path $entry) -ieq $normalizedInstallDir) {
                $pathHasDir = $true
                break
            }
        }

        if ($pathHasDir) {
            $removePathChoice = Read-Host "Remove '$installDir' from User PATH as well? [y/N]"
            if ($removePathChoice -match '^[Yy]$') {
                $filtered = @()
                foreach ($entry in $entries) {
                    if ((Normalize-Path $entry) -ine $normalizedInstallDir) {
                        $filtered += $entry
                    }
                }
                [Environment]::SetEnvironmentVariable("Path", ($filtered -join ';'), "User")
                Write-Warn "Removed $installDir from User PATH. Open a new terminal for changes to apply."
            }
        }
    }
}

function Resolve-BuiltBinary([string]$BuildDir) {
    $candidates = @(
        (Join-Path $BuildDir "bin\tictactoe-cx.exe"),
        (Join-Path $BuildDir "bin\Release\tictactoe-cx.exe"),
        (Join-Path $BuildDir "Release\tictactoe-cx.exe")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

if ($Help) {
    Show-Usage
    exit 0
}

if ($UpdateExisting -and $UninstallExisting) {
    Fail "Use only one of -UpdateExisting or -UninstallExisting."
}

$action = "install"
if ($UpdateExisting) { $action = "update" }
if ($UninstallExisting) { $action = "uninstall" }

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Fail "CMake not found in PATH."
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildDir = Join-Path $projectRoot "build-windows"
$enableSdl = if ($NoGui) { "OFF" } else { "ON" }
$targetDir = $null
$skipDestinationPrompt = $false

if ([string]::IsNullOrWhiteSpace($Mode)) {
    Write-Host "Select mode:"
    Write-Host "  1) Install (for normal use)"
    Write-Host "  2) Test (build in repo only)"
    $choice = Read-Host "> "
    switch ($choice) {
        "1" { $Mode = "install" }
        "2" { $Mode = "test" }
        default { Fail "Invalid selection." }
    }
}

if ($Mode -eq "test" -and $action -ne "install") {
    Fail "-UpdateExisting and -UninstallExisting can only be used with install mode."
}

$existingBinary = $null
if ($Mode -eq "install") {
    $existingBinary = Resolve-ExistingBinary
}

if ($Mode -eq "install" -and $existingBinary) {
    Write-Info "Detected existing installation at: $existingBinary"

    if ($action -eq "update") {
        $targetDir = Split-Path -Parent $existingBinary
        $skipDestinationPrompt = $true
    } elseif ($action -eq "uninstall") {
        Remove-InstalledBinary -BinaryPath $existingBinary
        exit 0
    } else {
        Write-Host "Choose action:"
        Write-Host "  1) Update existing installation"
        Write-Host "  2) Uninstall/remove existing installation"
        Write-Host "  3) Install to another location"
        Write-Host "  4) Cancel"
        $existingChoice = Read-Host "> "
        switch ($existingChoice) {
            "1" {
                $action = "update"
                $targetDir = Split-Path -Parent $existingBinary
                $skipDestinationPrompt = $true
            }
            "2" {
                $action = "uninstall"
                Remove-InstalledBinary -BinaryPath $existingBinary
                exit 0
            }
            "3" { $action = "install" }
            "4" {
                Write-Info "Canceled."
                exit 0
            }
            default { Fail "Invalid selection." }
        }
    }
}

if ($Mode -eq "install" -and -not $existingBinary -and $action -eq "uninstall") {
    Fail "No existing installation was detected to uninstall."
}

Write-Info "Configuring build (ENABLE_SDL2=$enableSdl)"
Invoke-Tool -Tool "cmake" -Args @("-S", $projectRoot, "-B", $buildDir, "-DENABLE_SDL2=$enableSdl") -FailureMessage "CMake configure failed"

Write-Info "Building"
Invoke-Tool -Tool "cmake" -Args @("--build", $buildDir, "--config", "Release") -FailureMessage "Build failed"

$binary = Resolve-BuiltBinary -BuildDir $buildDir
if (-not $binary) {
    Fail "Binary not found under $buildDir"
}

if ($Mode -eq "test") {
    Write-Host "Test build ready at: $binary" -ForegroundColor Green
    exit 0
}

if (-not $skipDestinationPrompt) {
    Write-Host "Install destination:" -ForegroundColor Cyan
    Write-Host "  1) Main user bin folder ($env:USERPROFILE\bin)"
    Write-Host "  2) Custom folder (will be added to user PATH)"
    $installChoice = Read-Host "> "

    switch ($installChoice) {
        "1" { $targetDir = Join-Path $env:USERPROFILE "bin" }
        "2" {
            $custom = Read-Host "Custom directory path"
            if ([string]::IsNullOrWhiteSpace($custom)) {
                Fail "Custom directory cannot be empty."
            }
            $targetDir = $custom
        }
        default { Fail "Invalid selection." }
    }
}

if ([string]::IsNullOrWhiteSpace($targetDir)) {
    Fail "Target directory could not be resolved."
}

try {
    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
} catch {
    Fail "Could not create target directory '$targetDir'. If it is protected, run PowerShell as Administrator. Details: $($_.Exception.Message)"
}

$destination = Join-Path $targetDir "tictactoe-cx.exe"
try {
    Copy-Item -Path $binary -Destination $destination -Force
} catch {
    Fail "Could not copy binary to '$destination'. If it is protected, run PowerShell as Administrator. Details: $($_.Exception.Message)"
}
Write-Host "Installed: $destination" -ForegroundColor Green

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if (-not $userPath) { $userPath = "" }
$pathEntries = $userPath -split ';' | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
$normalizedTarget = Normalize-Path $targetDir
$inPath = $false
foreach ($entry in $pathEntries) {
    if ((Normalize-Path $entry) -ieq $normalizedTarget) {
        $inPath = $true
        break
    }
}

if (-not $inPath) {
    $newPath = if ($userPath) { "$userPath;$targetDir" } else { $targetDir }
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Warn "Added $targetDir to User PATH. Open a new terminal to use it."
}
