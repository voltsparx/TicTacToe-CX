# Building Scripts

This folder contains install/build helpers for each platform:

- `install-linux.sh`
- `install-macos.sh`
- `install-windows.ps1`
- `install-termux.sh`
- `run-termux.sh`

All installers include OpenSSL because secure LAN mode uses AES-GCM authenticated encryption.

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
