param(
    [switch]$NoGui
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$buildDir = Join-Path $projectRoot "build-windows"
$enableSdl = if ($NoGui) { "OFF" } else { "ON" }

Write-Host "==> TicTacToe-CX Windows installer" -ForegroundColor Cyan

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "CMake was not found in PATH." -ForegroundColor Yellow
    Write-Host "Install CMake first (winget install Kitware.CMake) and rerun." -ForegroundColor Yellow
    exit 1
}

$opensslRoot = $env:OPENSSL_ROOT_DIR
if (-not $opensslRoot) {
    $candidates = @(
        "C:\Program Files\OpenSSL-Win64",
        "C:\Program Files\OpenSSL-Win32"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $opensslRoot = $candidate
            break
        }
    }
}

if (-not $opensslRoot) {
    Write-Host "OpenSSL was not found. Attempting install via winget..." -ForegroundColor Cyan
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        winget install -e --id ShiningLight.OpenSSL.Light --accept-source-agreements --accept-package-agreements
        $candidates = @(
            "C:\Program Files\OpenSSL-Win64",
            "C:\Program Files\OpenSSL-Win32"
        )
        foreach ($candidate in $candidates) {
            if (Test-Path $candidate) {
                $opensslRoot = $candidate
                break
            }
        }
    }
}

if (-not $opensslRoot) {
    Write-Host "OpenSSL not found. Install OpenSSL and rerun. You can set OPENSSL_ROOT_DIR." -ForegroundColor Yellow
    exit 1
}

Write-Host "==> Configuring build (ENABLE_SDL2=$enableSdl)" -ForegroundColor Cyan
cmake -S $projectRoot -B $buildDir -DENABLE_SDL2=$enableSdl -DOPENSSL_ROOT_DIR="$opensslRoot"

Write-Host "==> Building" -ForegroundColor Cyan
cmake --build $buildDir --config Release

Write-Host "Done. Run: $buildDir\\Release\\tictactoe-cx.exe" -ForegroundColor Green
