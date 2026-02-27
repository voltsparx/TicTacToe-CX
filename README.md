# TicTacToe-CX

<p align="center">
  <img src="https://img.shields.io/badge/Version-v3.1-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/C-99-green.svg" alt="C Standard">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License">
</p>

A Tic Tac Toe game written in C with CLI, Nano AI opponents, LAN multiplayer, and cloudflared-based Internet multiplayer.

## Features

- **Single Player vs AI**
  - Easy: Random moves (great for beginners)
  - Medium: Blocks wins and tries to win
  - Hard: Minimax algorithm

- **Two Player (Local)**
  - Play on the same machine
  - Choose your symbol (X or O)

- **LAN Multiplayer**
  - Host or join games via TCP sockets
  - Encrypted session with passphrase-based handshake
  - Real-time board synchronization

- **Internet Multiplayer (CLI)**
  - Host a game over the internet using `cloudflared` tunnel service
  - Join with a shared cloudflared link/hostname
  - Uses the same encrypted game handshake as LAN mode
  - If `cloudflared` is missing, the game can prompt to install it

## Encryption Session (LAN)

LAN security is session-based and negotiated when a client connects:

1. The client attempts a secure session using OpenSSL.
2. If OpenSSL negotiation fails, it falls back to legacy compatibility mode.
3. A connection is considered secure only when:
   `connected && security_ready && mode != NONE`.

> ⚠ Legacy mode is provided for compatibility and does not provide encryption.
> It should only be used on trusted local networks.

### OpenSSL Mode (Primary)

- Passphrase key material is derived using PBKDF2-HMAC-SHA256 (200,000 iterations).
- The handshake exchanges a salt and client/server nonces, then derives
  directional keys (`client → server` and `server → client`).
- Game packets are protected using AES-256-GCM authenticated encryption.
- Each frame includes a sequence number; replayed or stale sequence values are rejected.

### Legacy Mode (Compatibility Fallback)

- Uses the older custom key/nonce stream + frame MAC path for compatibility.
- Still enforces monotonic nonce checks to reject replayed frames.
- OpenSSL mode is stronger and preferred for production LAN play.

- **CLI**
  - ANSI color themes (Default, Dark, Light, Retro)
  - Enhanced bright color palette
  - AI thinking indicators
  - Unicode board rendering with ASCII fallback
  - Live full-screen board redraw (video-like terminal flow)
  - About page in main menu (author/contact/version)
  - Code-driven sound effects (toggle in settings)

- **GUI (SDL2)**
  - Main menu flow aligned with CLI (AI, Local, LAN, Scores, Settings, About)
  - Keyboard + mouse navigation
  - In-window LAN host/join setup with encrypted handshake
  - Live board rendering with game-over overlays and rematch flow

- **Customizable Settings**
  - Board sizes: 3x3, 4x4, 5x5
  - Optional timer per move
  - Configurable color themes
  - Toggle sound effects

## Building

### Prerequisites

- C compiler (gcc, clang, or MSVC)
- CMake 3.10 or higher

### Build Instructions

```bash
# Linux/macOS
cmake -S . -B build-linux
cmake --build build-linux -j

# Windows (Visual Studio or Ninja)
cmake -S . -B build-windows
cmake --build build-windows --config Release
```

Executable output is generated under `build-<os>/bin/`.

### Platform Install Scripts

Use the scripts in `building-scripts/`:

- Linux: `bash building-scripts/install-linux.sh`
- macOS: `bash building-scripts/install-macos.sh`
- Windows: `powershell -ExecutionPolicy Bypass -File .\building-scripts\install-windows.ps1`
- Termux: `bash building-scripts/install-termux.sh`
- Linux -> Windows cross-build: `bash building-scripts/build-windows-from-linux.sh`

Each installer asks for:

1. `Install` mode (copy into a bin directory for regular use) or
2. `Test` mode (build only in `build-<os>/bin/` inside the repo)

In `Install` mode, scripts detect existing installs:

1. If not installed yet, they do a normal install flow.
2. If already installed, they ask whether to:
   - Update existing install
   - Uninstall/remove it
   - Install to another path

Termux users should launch with `bash building-scripts/run-termux.sh`.
`--gui` is intentionally blocked on Termux.

### Running

```bash
# Run (Linux/macOS)
./build-linux/bin/tictactoe-cx

# Launch GUI mode (when SDL2 is available)
./build-linux/bin/tictactoe-cx --gui

# Print version and exit
./build-linux/bin/tictactoe-cx --version
```

If your terminal shows visual artifacts, you can force compatibility mode:

```bash
TICTACTOE_NO_LIVE=1 TICTACTOE_ASCII=1 ./build-linux/bin/tictactoe-cx
```

## How to Play

1. Use **Up/Down** arrows and **Enter** to navigate menus
2. Enter moves as `23` or `2 3` (row, column)
3. In game, press `Q` to return to menu
4. Open **About** from main menu for game/author/version details

### Internet Mode (CLI)

1. Open **Internet Multiplayer**.
2. Host:
   - choose **Host Internet Game**
   - share the generated cloudflared link with your opponent.
3. Join:
   - choose **Join Internet Game**
   - paste the host link (or hostname) and connect.

Notes:
- Both players should have `cloudflared` installed.
- The game can attempt automatic install when `cloudflared` is missing.

### GUI Controls

1. Launch with `--gui`
2. Use **Arrow Keys + Enter** or **Mouse Click**
3. In game, click cells to move
4. Press **Esc** to return to main menu

## Configuration

On first run, the game asks where to create its data directory.
Default location:

- Linux/macOS/Termux: `~/.tictactoe-cx-config/`
- Windows: `%USERPROFILE%\.tictactoe-cx-config\`

Inside that directory:

- `config.ini`
- `saves/highscores.txt`

Settings in `config.ini` include:
- Board size (3-5)
- AI difficulty
- Timer settings
- Color theme
- Sound enabled/disabled

The directory and files are auto-created and can be edited manually.

## Project Structure

```
TicTacToe-CX/
├── src/
│   ├── main.c      # Main game loop and menus
│   ├── game.c/h    # Core game logic
│   ├── cli.c/h     # CLI rendering with ANSI colors
│   ├── ai.c/h      # AI opponents
│   ├── network.c/h # LAN multiplayer
│   ├── internet.c/h # Cloudflared tunnel integration
│   └── utils.c/h   # Data/config/score storage helpers
├── building-scripts/      # Install/test build scripts
├── CMakeLists.txt
└── README.md
```

## License

See [LICENSE](LICENSE) file.

## Author

- **voltsparx** - voltsparx@gmail.com
