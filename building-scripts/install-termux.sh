#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-termux"
BIN_PATH="${BUILD_DIR}/bin/tictactoe-cx"
RUN_WRAPPER="${PROJECT_ROOT}/building-scripts/run-termux.sh"

MODE=""

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

usage() {
  cat <<EOF
Usage: bash building-scripts/install-termux.sh [options]

Options:
  --install      Install mode (copy to \$PREFIX/bin)
  --test         Test mode (build in repo only)
  -h, --help     Show this help
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

if [[ "${MODE}" == "install" ]]; then
  info "Installing dependencies"
  pkg update -y
  pkg install -y clang cmake make git openssl
else
  info "Test mode: skipping dependency installation"
fi

info "Configuring build (GUI disabled on Termux)"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2=OFF

info "Building"
cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 2)"

[[ -x "${BIN_PATH}" ]] || fail "Binary not found: ${BIN_PATH}"

cat > "${RUN_WRAPPER}" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${PROJECT_ROOT}/build-termux/bin/tictactoe-cx"

for arg in "$@"; do
  if [[ "$arg" == "--gui" || "$arg" == "-g" ]]; then
    echo -e "\033[91mError: GUI mode is not supported on Termux.\033[0m"
    exit 1
  fi
done

exec "${BIN}" "$@"
EOF

chmod +x "${RUN_WRAPPER}"

if [[ "${MODE}" == "test" ]]; then
  success "Test build ready at: ${BIN_PATH}"
  echo -e "${YELLOW}Run with: ${RUN_WRAPPER}${RESET}"
  echo -e "${YELLOW}Note: GUI mode is unavailable on Termux.${RESET}"
  exit 0
fi

install -m 755 "${BIN_PATH}" "${PREFIX}/bin/tictactoe-cx"
success "Installed: ${PREFIX}/bin/tictactoe-cx"
echo -e "${YELLOW}GUI mode is unavailable on Termux.${RESET}"
