#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-macos"
BIN_PATH="${BUILD_DIR}/bin/tictactoe-cx"
SYSTEM_BIN_DIR=""

MODE=""
INSTALL_TARGET=""
CUSTOM_DIR=""
ENABLE_SDL2="ON"
ACTION="install"
EXISTING_BIN=""
SUDO=""

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

run_cmd() {
  if ! "$@"; then
    fail "Command failed: $*"
  fi
}

run_with_privilege() {
  local path_hint="$1"
  shift

  if [[ -e "${path_hint}" ]]; then
    if [[ -w "${path_hint}" ]]; then
      run_cmd "$@"
      return
    fi
  else
    local parent
    parent="$(dirname "${path_hint}")"
    while [[ ! -d "${parent}" && "${parent}" != "/" ]]; do
      parent="$(dirname "${parent}")"
    done

    if [[ -w "${parent}" ]]; then
      run_cmd "$@"
      return
    fi
  fi

  if [[ -n "${SUDO}" ]]; then
    run_cmd "${SUDO}" "$@"
  else
    fail "Need elevated privileges to modify ${path_hint}. Re-run as root or install sudo."
  fi
}

normalize_path() {
  local path="$1"
  if command -v realpath >/dev/null 2>&1; then
    realpath "${path}"
  else
    echo "${path}"
  fi
}

detect_existing_binary() {
  local candidate=""

  candidate="$(command -v tictactoe-cx 2>/dev/null || true)"
  if [[ -n "${candidate}" && "${candidate}" == */* && -x "${candidate}" ]]; then
    EXISTING_BIN="$(normalize_path "${candidate}")"
    return 0
  fi

  for candidate in "${SYSTEM_BIN_DIR}/tictactoe-cx" "${HOME}/.local/bin/tictactoe-cx" "${HOME}/bin/tictactoe-cx"; do
    if [[ -x "${candidate}" ]]; then
      EXISTING_BIN="$(normalize_path "${candidate}")"
      return 0
    fi
  done

  return 1
}

set_update_target_from_existing() {
  local existing_dir
  existing_dir="$(dirname "${EXISTING_BIN}")"
  if [[ "${existing_dir}" == "${SYSTEM_BIN_DIR}" ]]; then
    INSTALL_TARGET="system"
    CUSTOM_DIR=""
  else
    INSTALL_TARGET="custom"
    CUSTOM_DIR="${existing_dir}"
  fi
}

remove_existing_install() {
  [[ -n "${EXISTING_BIN}" ]] || fail "No installed binary detected."
  [[ -e "${EXISTING_BIN}" ]] || fail "Installed binary no longer exists: ${EXISTING_BIN}"

  info "Removing ${EXISTING_BIN}"
  run_with_privilege "${EXISTING_BIN}" rm -f "${EXISTING_BIN}"
  success "Removed: ${EXISTING_BIN}"
}

handle_existing_install() {
  if ! detect_existing_binary; then
    return
  fi

  info "Detected existing installation at: ${EXISTING_BIN}"

  if [[ "${ACTION}" == "update" ]]; then
    set_update_target_from_existing
    return
  fi

  if [[ "${ACTION}" == "uninstall" ]]; then
    remove_existing_install
    exit 0
  fi

  echo -e "${CYAN}Choose action:${RESET}"
  echo "  1) Update existing installation"
  echo "  2) Uninstall/remove existing installation"
  echo "  3) Install to another location"
  echo "  4) Cancel"
  read -r -p "> " existing_choice || fail "Input aborted."

  case "${existing_choice}" in
    1)
      ACTION="update"
      set_update_target_from_existing
      ;;
    2)
      ACTION="uninstall"
      remove_existing_install
      exit 0
      ;;
    3)
      ACTION="install"
      ;;
    4)
      info "Canceled."
      exit 0
      ;;
    *)
      fail "Invalid selection."
      ;;
  esac
}

usage() {
  cat <<EOF
Usage: bash building-scripts/install-macos.sh [options]

Options:
  --install            Install mode
  --test               Test mode (build only in repo)
  --system-bin         Install to Homebrew bin directory
  --custom-bin <dir>   Install to custom directory
  --update-existing    Update detected existing installation
  --uninstall-existing Uninstall detected existing installation
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
    --update-existing)
      ACTION="update"
      shift
      ;;
    --uninstall-existing)
      ACTION="uninstall"
      shift
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
  read -r -p "> " mode_choice || fail "Input aborted."
  case "${mode_choice}" in
    1) MODE="install" ;;
    2) MODE="test" ;;
    *) fail "Invalid selection." ;;
  esac
fi

if [[ "${MODE}" == "test" && "${ACTION}" != "install" ]]; then
  fail "--update-existing and --uninstall-existing can only be used with --install."
fi

command -v brew >/dev/null 2>&1 || fail "Homebrew is required. Install from https://brew.sh"
SYSTEM_BIN_DIR="$(dirname "$(command -v brew)")"
[[ -n "${SYSTEM_BIN_DIR}" ]] || fail "Failed to detect Homebrew bin directory."

if [[ "${EUID}" -ne 0 && -x "$(command -v sudo 2>/dev/null || true)" ]]; then
  SUDO="sudo"
fi

if [[ "${MODE}" == "install" ]]; then
  handle_existing_install
fi

if [[ "${MODE}" == "install" && -z "${INSTALL_TARGET}" ]]; then
  echo -e "${CYAN}Install destination:${RESET}"
  echo "  1) Main system bin (${SYSTEM_BIN_DIR})"
  echo "  2) Custom/user bin directory"
  read -r -p "> " install_choice || fail "Input aborted."
  case "${install_choice}" in
    1) INSTALL_TARGET="system" ;;
    2) INSTALL_TARGET="custom" ;;
    *) fail "Invalid selection." ;;
  esac
fi

if [[ "${MODE}" == "install" && "${INSTALL_TARGET}" == "custom" && -z "${CUSTOM_DIR}" ]]; then
  read -r -p "Custom bin directory [~/.local/bin]: " custom_input || fail "Input aborted."
  CUSTOM_DIR="${custom_input:-$HOME/.local/bin}"
fi

if [[ "${MODE}" == "install" ]]; then
  info "Installing dependencies via Homebrew"
  run_cmd brew update
  run_cmd brew install cmake openssl@3 sdl2 sdl2_ttf
else
  info "Test mode: skipping dependency installation"
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
OPENSSL_ROOT="$(brew --prefix openssl@3)"

info "Configuring build (ENABLE_SDL2=${ENABLE_SDL2})"
run_cmd cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2="${ENABLE_SDL2}" -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT}"

info "Building"
run_cmd cmake --build "${BUILD_DIR}" -j"${JOBS}"

[[ -x "${BIN_PATH}" ]] || fail "Binary not found: ${BIN_PATH}"

if [[ "${MODE}" == "test" ]]; then
  success "Test build ready at: ${BIN_PATH}"
  exit 0
fi

if [[ "${INSTALL_TARGET}" == "system" ]]; then
  info "Installing to ${SYSTEM_BIN_DIR}"
  run_with_privilege "${SYSTEM_BIN_DIR}/tictactoe-cx" install -m 755 "${BIN_PATH}" "${SYSTEM_BIN_DIR}/tictactoe-cx"
  success "Installed: ${SYSTEM_BIN_DIR}/tictactoe-cx"
else
  [[ -n "${CUSTOM_DIR}" ]] || fail "Custom directory is empty."
  info "Installing to ${CUSTOM_DIR}"
  if ! mkdir -p "${CUSTOM_DIR}" 2>/dev/null; then
    run_with_privilege "${CUSTOM_DIR}" mkdir -p "${CUSTOM_DIR}"
  fi
  run_with_privilege "${CUSTOM_DIR}/tictactoe-cx" install -m 755 "${BIN_PATH}" "${CUSTOM_DIR}/tictactoe-cx"
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
      read -r -p "Add to PATH in ${rc_file}? [y/N]: " add_path_choice || fail "Input aborted."
      if [[ "${add_path_choice}" == "y" || "${add_path_choice}" == "Y" ]]; then
        if [[ ! -f "${rc_file}" ]] || ! grep -Fq "export PATH=\"${CUSTOM_DIR}:\$PATH\"" "${rc_file}"; then
          echo "export PATH=\"${CUSTOM_DIR}:\$PATH\"" >> "${rc_file}"
        fi
        success "PATH entry added to ${rc_file}"
      fi
      ;;
  esac
fi
