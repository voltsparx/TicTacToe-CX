#!/usr/bin/env bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-macos"

FORCE_DIRTY=0
INSTALL_DIR=""
ENABLE_SDL2="ON"

RED="\033[91m"
GREEN="\033[92m"
YELLOW="\033[93m"
CYAN="\033[96m"
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
Usage: bash update-scripts/update-macos.sh [options]

Options:
  --install               Copy binary to ~/.local/bin/tictactoe-cx
  --install-dir <path>    Copy binary to <path>/tictactoe-cx
  --no-gui                Build with -DENABLE_SDL2=OFF
  --force-dirty           Allow update with uncommitted changes
  -h, --help              Show this help
EOF
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install)
      INSTALL_DIR="${HOME}/.local/bin"
      shift
      ;;
    --install-dir)
      [[ $# -ge 2 ]] || fail "--install-dir requires a path"
      INSTALL_DIR="$2"
      shift 2
      ;;
    --no-gui)
      ENABLE_SDL2="OFF"
      shift
      ;;
    --force-dirty)
      FORCE_DIRTY=1
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

require_cmd git
require_cmd cmake

[[ -d "${PROJECT_ROOT}/.git" ]] || fail "Not a git repository: ${PROJECT_ROOT}"

cd "${PROJECT_ROOT}"

if [[ "${FORCE_DIRTY}" -ne 1 ]] && [[ -n "$(git status --porcelain)" ]]; then
  fail "Working tree has uncommitted changes. Commit/stash first or use --force-dirty."
fi

info "Fetching latest changes"
git fetch --all --prune

CURRENT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
[[ "${CURRENT_BRANCH}" != "HEAD" ]] || fail "Detached HEAD detected. Checkout a branch first."

info "Pulling latest commit for branch ${CURRENT_BRANCH}"
if ! git pull --ff-only; then
  fail "git pull failed. Resolve conflicts or upstream issues, then retry."
fi

OPENSSL_PREFIX=""
if command -v brew >/dev/null 2>&1; then
  OPENSSL_PREFIX="$(brew --prefix openssl@3 2>/dev/null || true)"
else
  warn "Homebrew not found. Assuming OpenSSL is discoverable by CMake."
fi

info "Configuring build (ENABLE_SDL2=${ENABLE_SDL2})"
if [[ -n "${OPENSSL_PREFIX}" ]]; then
  cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2="${ENABLE_SDL2}" -DOPENSSL_ROOT_DIR="${OPENSSL_PREFIX}"
else
  cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2="${ENABLE_SDL2}"
fi

JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
info "Building with ${JOBS} jobs"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

BIN="${BUILD_DIR}/bin/tictactoe-cx"
[[ -x "${BIN}" ]] || fail "Build finished but binary not found: ${BIN}"

if ! "${BIN}" --help >/dev/null 2>&1; then
  warn "Binary returned non-zero for --help; verify runtime manually."
fi

if [[ -n "${INSTALL_DIR}" ]]; then
  info "Installing binary to ${INSTALL_DIR}"
  mkdir -p "${INSTALL_DIR}"
  cp "${BIN}" "${INSTALL_DIR}/tictactoe-cx"
  chmod +x "${INSTALL_DIR}/tictactoe-cx"
  success "Installed: ${INSTALL_DIR}/tictactoe-cx"
else
  success "Updated binary: ${BIN}"
fi
