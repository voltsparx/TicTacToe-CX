param(
    [ValidateSet("install", "test")]
    [string]$Mode = "",
    [switch]$NoGui,
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
  -Mode <install|test>    Install mode or test mode
  -NoGui                  Build with -DENABLE_SDL2=OFF
  -Help                   Show this help
"@ | Write-Host
}

if ($Help) {
    Show-Usage
    exit 0
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Fail "CMake not found in PATH."
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildDir = Join-Path $projectRoot "build-windows"
$binary = Join-Path $buildDir "bin\tictactoe-cx.exe"
$enableSdl = if ($NoGui) { "OFF" } else { "ON" }

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

Write-Info "Configuring build (ENABLE_SDL2=$enableSdl)"
cmake -S $projectRoot -B $buildDir -DENABLE_SDL2=$enableSdl

Write-Info "Building"
cmake --build $buildDir --config Release

if (-not (Test-Path $binary)) {
    Fail "Binary not found: $binary"
}

if ($Mode -eq "test") {
    Write-Host "Test build ready at: $binary" -ForegroundColor Green
    exit 0
}

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

New-Item -ItemType Directory -Force -Path $targetDir | Out-Null
Copy-Item -Path $binary -Destination (Join-Path $targetDir "tictactoe-cx.exe") -Force
Write-Host "Installed: $(Join-Path $targetDir 'tictactoe-cx.exe')" -ForegroundColor Green

$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
if (-not $userPath) { $userPath = "" }
$pathEntries = $userPath -split ';' | Where-Object { $_ -ne "" }
if (-not ($pathEntries -contains $targetDir)) {
    $newPath = if ($userPath) { "$userPath;$targetDir" } else { $targetDir }
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Warn "Added $targetDir to User PATH. Open a new terminal to use it."
}
