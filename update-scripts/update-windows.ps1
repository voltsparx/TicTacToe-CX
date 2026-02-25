param(
    [switch]$Install,
    [string]$InstallDir = "$env:USERPROFILE\bin",
    [switch]$NoGui,
    [switch]$ForceDirty,
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
  powershell -ExecutionPolicy Bypass -File .\update-scripts\update-windows.ps1 [options]

Options:
  -Install                 Copy binary to %USERPROFILE%\bin\tictactoe-cx.exe
  -InstallDir <path>       Custom install directory
  -NoGui                   Build with -DENABLE_SDL2=OFF
  -ForceDirty              Allow update with uncommitted changes
  -Help                    Show this help
"@ | Write-Host
}

if ($Help) {
    Show-Usage
    exit 0
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Fail "git is required but was not found in PATH."
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Fail "cmake is required but was not found in PATH."
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

if (-not (Test-Path (Join-Path $projectRoot ".git"))) {
    Fail "Not a git repository: $projectRoot"
}

Set-Location $projectRoot

if (-not $ForceDirty) {
    $dirty = git status --porcelain
    if ($dirty) {
        Fail "Working tree has uncommitted changes. Commit/stash first or use -ForceDirty."
    }
}

Write-Info "Fetching latest changes"
git fetch --all --prune | Out-Null

$branch = git rev-parse --abbrev-ref HEAD
if ($branch -eq "HEAD") {
    Fail "Detached HEAD detected. Checkout a branch first."
}

Write-Info "Pulling latest commit for branch $branch"
try {
    git pull --ff-only | Out-Null
} catch {
    Fail "git pull failed. Resolve conflicts or upstream issues, then retry."
}

$buildDir = Join-Path $projectRoot "build-windows"
$enableSdl = if ($NoGui) { "OFF" } else { "ON" }

Write-Info "Configuring build (ENABLE_SDL2=$enableSdl)"
cmake -S $projectRoot -B $buildDir -DENABLE_SDL2=$enableSdl

Write-Info "Building"
cmake --build $buildDir --config Release

$candidates = @(
    (Join-Path $buildDir "bin\tictactoe-cx.exe"),
    (Join-Path $buildDir "Release\tictactoe-cx.exe"),
    (Join-Path $buildDir "tictactoe-cx.exe")
)

$binary = $null
foreach ($candidate in $candidates) {
    if (Test-Path $candidate) {
        $binary = $candidate
        break
    }
}

if (-not $binary) {
    Fail "Build finished but binary was not found in $buildDir"
}

try {
    & $binary --help *> $null
} catch {
    Write-Warn "Binary returned non-zero for --help; verify runtime manually."
}

if ($Install) {
    Write-Info "Installing binary to $InstallDir"
    New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
    Copy-Item -Path $binary -Destination (Join-Path $InstallDir "tictactoe-cx.exe") -Force
    Write-Host "Installed: $(Join-Path $InstallDir 'tictactoe-cx.exe')" -ForegroundColor Green
} else {
    Write-Host "Updated binary: $binary" -ForegroundColor Green
}
