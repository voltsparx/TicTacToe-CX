#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-termux"
BIN_PATH="${BUILD_DIR}/bin/tictactoe-cx"
RUN_WRAPPER="${PROJECT_ROOT}/building-scripts/run-termux.sh"
SYSTEM_BIN="${PREFIX:-/data/data/com.termux/files/usr}/bin/tictactoe-cx"

MODE=""
ACTION="install"
EXISTING_BIN=""

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

detect_existing_binary() {
  local candidate=""

  candidate="$(command -v tictactoe-cx 2>/dev/null || true)"
  if [[ -n "${candidate}" && "${candidate}" == */* && -x "${candidate}" ]]; then
    EXISTING_BIN="${candidate}"
    return 0
  fi

  if [[ -x "${SYSTEM_BIN}" ]]; then
    EXISTING_BIN="${SYSTEM_BIN}"
    return 0
  fi

  return 1
}

remove_existing_install() {
  [[ -n "${EXISTING_BIN}" ]] || fail "No installed binary detected."
  [[ -e "${EXISTING_BIN}" ]] || fail "Installed binary no longer exists: ${EXISTING_BIN}"

  info "Removing ${EXISTING_BIN}"
  run_cmd rm -f "${EXISTING_BIN}"
  success "Removed: ${EXISTING_BIN}"
}

handle_existing_install() {
  if ! detect_existing_binary; then
    return
  fi

  info "Detected existing installation at: ${EXISTING_BIN}"

  if [[ "${ACTION}" == "update" ]]; then
    return
  fi

  if [[ "${ACTION}" == "uninstall" ]]; then
    remove_existing_install
    exit 0
  fi

  echo -e "${CYAN}Choose action:${RESET}"
  echo "  1) Update existing installation"
  echo "  2) Uninstall/remove existing installation"
  echo "  3) Cancel"
  read -r -p "> " existing_choice || fail "Input aborted."

  case "${existing_choice}" in
    1)
      ACTION="update"
      ;;
    2)
      ACTION="uninstall"
      remove_existing_install
      exit 0
      ;;
    3)
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
Usage: bash building-scripts/install-termux.sh [options]

Options:
  --install            Install mode (copy to \$PREFIX/bin)
  --test               Test mode (build in repo only)
  --update-existing    Update detected existing installation
  --uninstall-existing Uninstall detected existing installation
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
    --update-existing)
      ACTION="update"
      shift
      ;;
    --uninstall-existing)
      ACTION="uninstall"
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

[[ -n "${PREFIX:-}" ]] || fail "PREFIX is not set. Run this script inside Termux."
[[ -d "${PREFIX}/bin" ]] || fail "Invalid Termux prefix: ${PREFIX}"

if [[ "${MODE}" == "install" ]]; then
  handle_existing_install
fi

if [[ "${MODE}" == "install" ]]; then
  info "Installing dependencies"
  run_cmd pkg update -y
  run_cmd pkg install -y clang cmake make git openssl
else
  info "Test mode: skipping dependency installation"
fi

info "Configuring build (GUI disabled on Termux)"
run_cmd cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2=OFF

info "Building"
run_cmd cmake --build "${BUILD_DIR}" -j"$(nproc 2>/dev/null || echo 2)"

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

run_cmd install -m 755 "${BIN_PATH}" "${PREFIX}/bin/tictactoe-cx"
success "Installed: ${PREFIX}/bin/tictactoe-cx"
echo -e "${YELLOW}GUI mode is unavailable on Termux.${RESET}"
