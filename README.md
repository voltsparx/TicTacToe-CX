# TicTacToe-CX

<p align="center">
  <img src="https://img.shields.io/badge/Version-1.0-blue.svg" alt="Version">
  <img src="https://img.shields.io/badge/C-99-green.svg" alt="C Standard">
  <img src="https://img.shields.io/badge/License-MIT-yellow.svg" alt="License">
</p>

A Tic Tac Toe game written in C with CLI, AI opponents, and LAN multiplayer.

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

- **CLI**
  - ANSI color themes (Default, Dark, Light, Retro)
  - Enhanced bright color palette
  - AI thinking indicators
  - ASCII board rendering
  - Code-driven sound effects (toggle in settings)

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
# Windows (MinGW)
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .

# Windows (Visual Studio)
mkdir build && cd build
cmake .. -G "Visual Studio 16 2019"
cmake --build .

# Linux/macOS
mkdir build && cd build
cmake ..
make
```

### Platform Install Scripts

Use the scripts in `building-scripts/`:

- Linux: `bash building-scripts/install-linux.sh`
- macOS: `bash building-scripts/install-macos.sh`
- Windows: `powershell -ExecutionPolicy Bypass -File .\building-scripts\install-windows.ps1`
- Termux: `bash building-scripts/install-termux.sh`

Termux users should launch with `bash building-scripts/run-termux.sh`.
`--gui` is intentionally blocked on Termux.

### Running

```bash
# Run the executable
./TicTacToe-CX

# Launch GUI mode (when SDL2 is available)
./TicTacToe-CX --gui
```

## How to Play

1. **Select Game Mode** from the main menu
2. **Enter moves** using row and column numbers (e.g., `1 1` for top-left)
3. **Game controls**:
   - During AI game: Your symbol is X
   - Enter coordinates as: `row col`
   - Press Q to quit to menu

## Configuration

Settings are stored in `config/config.ini`:
- Board size (3-5)
- AI difficulty
- Timer settings
- Color theme
- Sound enabled/disabled

## Project Structure

```
TicTacToe-CX/
├── src/
│   ├── main.c      # Main game loop and menus
│   ├── game.c/h    # Core game logic
│   ├── cli.c/h     # CLI rendering with ANSI colors
│   ├── ai.c/h      # AI opponents
│   ├── network.c/h # LAN multiplayer
│   └── utils.c/h   # Config and scoring
├── config/
│   └── config.ini
├── saves/
│   └── highscores.txt
├── CMakeLists.txt
└── README.md
```

## License

See [LICENSE](LICENSE) file.

## Author

- **voltsparx** - voltsparx@gmail.com
