#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-macos"

GREEN="\033[92m"
CYAN="\033[96m"
YELLOW="\033[93m"
RESET="\033[0m"

echo -e "${CYAN}==> TicTacToe-CX macOS installer${RESET}"

if ! command -v brew >/dev/null 2>&1; then
  echo -e "${YELLOW}Homebrew is required. Install it first: https://brew.sh/${RESET}"
  exit 1
fi

echo -e "${CYAN}==> Installing dependencies with Homebrew${RESET}"
brew update
brew install cmake openssl@3 sdl2 sdl2_ttf

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"

echo -e "${CYAN}==> Configuring build${RESET}"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2=ON -DOPENSSL_ROOT_DIR="$(brew --prefix openssl@3)"

echo -e "${CYAN}==> Building${RESET}"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

echo -e "${GREEN}Done. Run:${RESET} ${BUILD_DIR}/tictactoe-cx"
