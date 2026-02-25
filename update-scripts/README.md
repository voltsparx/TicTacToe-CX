# Update Scripts

This directory provides platform-specific update helpers for people who use a local clone of TicTacToe-CX.

Each updater does:

1. Repository checks (`git` present, inside repo, clean worktree unless forced)
2. `git fetch --all --prune` and `git pull --ff-only`
3. Reconfigure + rebuild with CMake
4. Optional install/copy of the updated binary

Build output is `build-<os>/bin/tictactoe-cx` (or `.exe` on Windows).

## Linux

```bash
bash update-scripts/update-linux.sh --install
```

Options:

- `--install` copy binary to `~/.local/bin/tictactoe-cx`
- `--install-dir <path>` custom install directory
- `--no-gui` build without SDL2 GUI
- `--force-dirty` allow update even with local uncommitted changes

## macOS

```bash
bash update-scripts/update-macos.sh --install
```

Options are the same as Linux.

## Windows (PowerShell)

```powershell
powershell -ExecutionPolicy Bypass -File .\update-scripts\update-windows.ps1 -Install
```

Options:

- `-Install` copy binary to `%USERPROFILE%\bin\tictactoe-cx.exe`
- `-InstallDir <path>` custom install directory
- `-NoGui` build without SDL2 GUI
- `-ForceDirty` allow update even with local uncommitted changes

## Termux

```bash
bash update-scripts/update-termux.sh
```

Options:

- `--no-install` skip copying to `$PREFIX/bin`
- `--force-dirty` allow update even with local uncommitted changes

Termux GUI mode remains unsupported.
