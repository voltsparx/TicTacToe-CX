#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-macos"
BIN_PATH="${BUILD_DIR}/bin/tictactoe-cx"

MODE=""
INSTALL_TARGET=""
CUSTOM_DIR=""
ENABLE_SDL2="ON"

GREEN="\033[92m"
CYAN="\033[96m"
YELLOW="\033[93m"
RED="\033[91m"
RESET="\033[0m"

fail() {
  echo -e "${RED}Error: $*${RESET}" >&2
  exit 1
}

info() {
  echo -e "${CYAN}==> $*${RESET}"
}

success() {
  echo -e "${GREEN}$*${RESET}"
}

warn() {
  echo -e "${YELLOW}Warning: $*${RESET}"
}

usage() {
  cat <<EOF
Usage: bash building-scripts/install-macos.sh [options]

Options:
  --install            Install mode
  --test               Test mode (build only in repo)
  --system-bin         Install to /usr/local/bin
  --custom-bin <dir>   Install to custom directory
  --no-gui             Build with -DENABLE_SDL2=OFF
  -h, --help           Show this help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install)
      MODE="install"
      shift
      ;;
    --test)
      MODE="test"
      shift
      ;;
    --system-bin)
      INSTALL_TARGET="system"
      shift
      ;;
    --custom-bin)
      [[ $# -ge 2 ]] || fail "--custom-bin requires a path"
      INSTALL_TARGET="custom"
      CUSTOM_DIR="$2"
      shift 2
      ;;
    --no-gui)
      ENABLE_SDL2="OFF"
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "Unknown option: $1"
      ;;
  esac
done

if [[ -z "${MODE}" ]]; then
  echo -e "${CYAN}Select mode:${RESET}"
  echo "  1) Install (for normal use)"
  echo "  2) Test (build in repo only)"
  read -r -p "> " mode_choice
  case "${mode_choice}" in
    1) MODE="install" ;;
    2) MODE="test" ;;
    *) fail "Invalid selection." ;;
  esac
fi

if [[ "${MODE}" == "install" && -z "${INSTALL_TARGET}" ]]; then
  echo -e "${CYAN}Install destination:${RESET}"
  echo "  1) Main system bin (/usr/local/bin)"
  echo "  2) Custom/user bin directory"
  read -r -p "> " install_choice
  case "${install_choice}" in
    1) INSTALL_TARGET="system" ;;
    2) INSTALL_TARGET="custom" ;;
    *) fail "Invalid selection." ;;
  esac
fi

if [[ "${MODE}" == "install" && "${INSTALL_TARGET}" == "custom" && -z "${CUSTOM_DIR}" ]]; then
  read -r -p "Custom bin directory [~/.local/bin]: " custom_input
  CUSTOM_DIR="${custom_input:-$HOME/.local/bin}"
fi

command -v brew >/dev/null 2>&1 || fail "Homebrew is required. Install from https://brew.sh"

if [[ "${MODE}" == "install" ]]; then
  info "Installing dependencies via Homebrew"
  brew update
  brew install cmake openssl@3 sdl2 sdl2_ttf
else
  info "Test mode: skipping dependency installation"
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
OPENSSL_ROOT="$(brew --prefix openssl@3)"

info "Configuring build (ENABLE_SDL2=${ENABLE_SDL2})"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2="${ENABLE_SDL2}" -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT}"

info "Building"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

[[ -x "${BIN_PATH}" ]] || fail "Binary not found: ${BIN_PATH}"

if [[ "${MODE}" == "test" ]]; then
  success "Test build ready at: ${BIN_PATH}"
  exit 0
fi

if [[ "${INSTALL_TARGET}" == "system" ]]; then
  info "Installing to /usr/local/bin"
  sudo install -m 755 "${BIN_PATH}" "/usr/local/bin/tictactoe-cx"
  success "Installed: /usr/local/bin/tictactoe-cx"
else
  [[ -n "${CUSTOM_DIR}" ]] || fail "Custom directory is empty."
  info "Installing to ${CUSTOM_DIR}"
  mkdir -p "${CUSTOM_DIR}"
  install -m 755 "${BIN_PATH}" "${CUSTOM_DIR}/tictactoe-cx"
  success "Installed: ${CUSTOM_DIR}/tictactoe-cx"

  case ":${PATH}:" in
    *":${CUSTOM_DIR}:"*) ;;
    *)
      warn "${CUSTOM_DIR} is not in PATH."
      shell_name="$(basename "${SHELL:-}")"
      if [[ "${shell_name}" == "zsh" ]]; then
        rc_file="${HOME}/.zshrc"
      elif [[ "${shell_name}" == "bash" ]]; then
        rc_file="${HOME}/.bash_profile"
      else
        rc_file="${HOME}/.profile"
      fi
      read -r -p "Add to PATH in ${rc_file}? [y/N]: " add_path_choice
      if [[ "${add_path_choice}" == "y" || "${add_path_choice}" == "Y" ]]; then
        echo "export PATH=\"${CUSTOM_DIR}:\$PATH\"" >> "${rc_file}"
        success "PATH entry added to ${rc_file}"
      fi
      ;;
  esac
fi
