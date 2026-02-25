#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-windows-cross"
BIN_PATH="${BUILD_DIR}/bin/tictactoe-cx.exe"

MINGW_PREFIX="${MINGW_PREFIX:-x86_64-w64-mingw32}"
ENABLE_SDL2="OFF"
OPENSSL_ROOT_DIR="${MINGW_OPENSSL_ROOT:-}"

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

warn() {
  echo -e "${YELLOW}Warning: $*${RESET}"
}

success() {
  echo -e "${GREEN}$*${RESET}"
}

usage() {
  cat <<EOF
Usage: bash building-scripts/build-windows-from-linux.sh [options]

Cross-compiles TicTacToe-CX on Linux for Windows using MinGW-w64.

Options:
  --with-gui                Try building with SDL2 GUI (requires MinGW SDL2/SDL2_ttf)
  --no-gui                  Build CLI-only binary (default)
  --openssl-root <path>     Windows-target OpenSSL root for MinGW
  --mingw-prefix <prefix>   MinGW triplet prefix (default: x86_64-w64-mingw32)
  --build-dir <path>        Custom build directory
  --clean                   Remove build directory before configure
  -h, --help                Show this help

Environment:
  MINGW_PREFIX              Same as --mingw-prefix
  MINGW_OPENSSL_ROOT        Same as --openssl-root

Notes:
  - This script requires Linux and a MinGW-w64 toolchain.
  - OpenSSL is mandatory for this project. It must be Windows-target OpenSSL.
EOF
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}

auto_detect_openssl_root() {
  local candidates=(
    "/usr/${MINGW_PREFIX}"
    "/usr/${MINGW_PREFIX}/sys-root/mingw"
    "/opt/${MINGW_PREFIX}"
  )

  for root in "${candidates[@]}"; do
    if [[ -f "${root}/include/openssl/evp.h" ]] && \
       [[ -f "${root}/lib/libcrypto.a" || -f "${root}/lib/libcrypto.dll.a" ]]; then
      echo "${root}"
      return 0
    fi
  done

  return 1
}

CLEAN_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --with-gui)
      ENABLE_SDL2="ON"
      shift
      ;;
    --no-gui)
      ENABLE_SDL2="OFF"
      shift
      ;;
    --openssl-root)
      [[ $# -ge 2 ]] || fail "--openssl-root requires a path"
      OPENSSL_ROOT_DIR="$2"
      shift 2
      ;;
    --mingw-prefix)
      [[ $# -ge 2 ]] || fail "--mingw-prefix requires a prefix"
      MINGW_PREFIX="$2"
      shift 2
      ;;
    --build-dir)
      [[ $# -ge 2 ]] || fail "--build-dir requires a path"
      BUILD_DIR="$2"
      BIN_PATH="${BUILD_DIR}/bin/tictactoe-cx.exe"
      shift 2
      ;;
    --clean)
      CLEAN_BUILD=1
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

if [[ "$(uname -s)" != "Linux" ]]; then
  fail "This script is for Linux hosts only."
fi

require_cmd cmake
require_cmd "${MINGW_PREFIX}-gcc"
require_cmd "${MINGW_PREFIX}-g++"
require_cmd "${MINGW_PREFIX}-windres"

if [[ -z "${OPENSSL_ROOT_DIR}" ]]; then
  if OPENSSL_ROOT_DIR="$(auto_detect_openssl_root)"; then
    info "Detected MinGW OpenSSL root: ${OPENSSL_ROOT_DIR}"
  else
    fail "Could not detect MinGW OpenSSL root. Install MinGW OpenSSL (for example on Arch: mingw-w64-x86_64-openssl), then use --openssl-root <path> or set MINGW_OPENSSL_ROOT."
  fi
fi

[[ -f "${OPENSSL_ROOT_DIR}/include/openssl/evp.h" ]] || fail "Missing OpenSSL headers under ${OPENSSL_ROOT_DIR}/include"
if [[ ! -f "${OPENSSL_ROOT_DIR}/lib/libcrypto.a" && ! -f "${OPENSSL_ROOT_DIR}/lib/libcrypto.dll.a" ]]; then
  fail "Missing libcrypto for Windows target under ${OPENSSL_ROOT_DIR}/lib"
fi

if [[ "${CLEAN_BUILD}" -eq 1 ]]; then
  info "Cleaning build directory ${BUILD_DIR}"
  rm -rf "${BUILD_DIR}"
fi

info "Configuring Windows cross-build"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER="${MINGW_PREFIX}-gcc" \
  -DCMAKE_CXX_COMPILER="${MINGW_PREFIX}-g++" \
  -DCMAKE_RC_COMPILER="${MINGW_PREFIX}-windres" \
  -DCMAKE_FIND_ROOT_PATH="${OPENSSL_ROOT_DIR}" \
  -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
  -DENABLE_SDL2="${ENABLE_SDL2}" \
  -DOPENSSL_ROOT_DIR="${OPENSSL_ROOT_DIR}" \
  -DOPENSSL_USE_STATIC_LIBS=TRUE

info "Building Windows executable"
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 2)"

[[ -f "${BIN_PATH}" ]] || fail "Build completed but executable not found: ${BIN_PATH}"

if [[ "${ENABLE_SDL2}" == "ON" ]]; then
  warn "GUI build requested. Ensure SDL2/SDL2_ttf Windows runtime DLLs are provided with the .exe."
fi

success "Windows binary created: ${BIN_PATH}"
