# Building Scripts

This folder contains install/build helpers for each platform:

- `install-linux.sh`
- `install-macos.sh`
- `install-windows.ps1`
- `install-termux.sh`
- `run-termux.sh`
- `build-windows-from-linux.sh`

All installers include OpenSSL because secure LAN mode uses authenticated encryption.

Each installer now supports two flows:

1. `Install`: build and copy the executable to a chosen bin directory.
2. `Test`: build only in the project under `build-<os>/bin/`.

In `Test` mode, scripts avoid system package installation and only build with existing local toolchains.

To launch GUI mode after build (SDL2 + SDL2_ttf required):

```bash
./build-linux/bin/tictactoe-cx --gui
```

## Quick usage

Linux:

```bash
bash building-scripts/install-linux.sh
```

macOS:

```bash
bash building-scripts/install-macos.sh
```

Windows (PowerShell):

```powershell
powershell -ExecutionPolicy Bypass -File .\building-scripts\install-windows.ps1
```

Termux:

```bash
bash building-scripts/install-termux.sh
bash building-scripts/run-termux.sh
```

Termux does not support GUI mode. `run-termux.sh` will fail fast if `--gui` or `-g` is passed.

Linux -> Windows cross-build:

```bash
bash building-scripts/build-windows-from-linux.sh
```

This produces `build-windows-cross/bin/tictactoe-cx.exe`.
You may need to provide MinGW-target OpenSSL explicitly:

```bash
bash building-scripts/build-windows-from-linux.sh --openssl-root /path/to/mingw/openssl/root
```
