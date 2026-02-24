#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-linux"

GREEN="\033[92m"
CYAN="\033[96m"
YELLOW="\033[93m"
RESET="\033[0m"

echo -e "${CYAN}==> TicTacToe-CX Linux installer${RESET}"

SUDO=""
if command -v sudo >/dev/null 2>&1; then
  SUDO="sudo"
fi

if command -v apt-get >/dev/null 2>&1; then
  echo -e "${CYAN}==> Installing dependencies with apt${RESET}"
  $SUDO apt-get update
  $SUDO apt-get install -y build-essential cmake libssl-dev libsdl2-dev libsdl2-ttf-dev
elif command -v dnf >/dev/null 2>&1; then
  echo -e "${CYAN}==> Installing dependencies with dnf${RESET}"
  $SUDO dnf install -y gcc gcc-c++ make cmake openssl-devel SDL2-devel SDL2_ttf-devel
elif command -v pacman >/dev/null 2>&1; then
  echo -e "${CYAN}==> Installing dependencies with pacman${RESET}"
  $SUDO pacman -Sy --noconfirm base-devel cmake openssl sdl2 sdl2_ttf
else
  echo -e "${YELLOW}Warning: package manager not recognized. Install cmake, compiler, OpenSSL, SDL2, SDL2_ttf manually.${RESET}"
fi

JOBS="$(command -v nproc >/dev/null 2>&1 && nproc || echo 2)"

echo -e "${CYAN}==> Configuring build${RESET}"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2=ON

echo -e "${CYAN}==> Building${RESET}"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

echo -e "${GREEN}Done. Run:${RESET} ${BUILD_DIR}/tictactoe-cx"
