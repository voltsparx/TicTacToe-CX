#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-termux"

FORCE_DIRTY=0
INSTALL_AFTER=1

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
Usage: bash update-scripts/update-termux.sh [options]

Options:
  --no-install    Do not copy binary into \$PREFIX/bin
  --force-dirty   Allow update with uncommitted changes
  -h, --help      Show this help
EOF
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-install)
      INSTALL_AFTER=0
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

[[ -n "${PREFIX:-}" ]] || fail "This updater is intended for Termux (PREFIX is not set)."

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

info "Configuring build (GUI disabled on Termux)"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2=OFF

JOBS="$(nproc 2>/dev/null || echo 2)"
info "Building with ${JOBS} jobs"
cmake --build "${BUILD_DIR}" -j"${JOBS}"

BIN="${BUILD_DIR}/bin/tictactoe-cx"
[[ -x "${BIN}" ]] || fail "Build finished but binary not found: ${BIN}"

if ! "${BIN}" --help >/dev/null 2>&1; then
  warn "Binary returned non-zero for --help; verify runtime manually."
fi

if [[ "${INSTALL_AFTER}" -eq 1 ]]; then
  TARGET_DIR="${PREFIX}/bin"
  info "Installing binary to ${TARGET_DIR}"
  cp "${BIN}" "${TARGET_DIR}/tictactoe-cx"
  chmod +x "${TARGET_DIR}/tictactoe-cx"
  success "Installed: ${TARGET_DIR}/tictactoe-cx"
fi

warn "GUI mode is not supported on Termux."
success "Update complete."
