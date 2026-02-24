#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-termux"
RUN_WRAPPER="${PROJECT_ROOT}/building-scripts/run-termux.sh"

GREEN="\033[92m"
CYAN="\033[96m"
RED="\033[91m"
RESET="\033[0m"

echo -e "${CYAN}==> TicTacToe-CX Termux installer${RESET}"
echo -e "${CYAN}==> Installing dependencies${RESET}"
pkg update -y
pkg install -y clang cmake make git openssl

echo -e "${CYAN}==> Configuring build (GUI disabled on Termux)${RESET}"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DENABLE_SDL2=OFF

echo -e "${CYAN}==> Building${RESET}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

cat > "${RUN_WRAPPER}" <<'EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${PROJECT_ROOT}/build-termux/tictactoe-cx"

for arg in "$@"; do
  if [[ "$arg" == "--gui" || "$arg" == "-g" ]]; then
    echo -e "\033[91mError: GUI mode is not supported on Termux.\033[0m"
    exit 1
  fi
done

exec "${BIN}" "$@"
EOF

chmod +x "${RUN_WRAPPER}"

echo -e "${GREEN}Done. Run:${RESET} ${RUN_WRAPPER}"
echo -e "${RED}Note:${RESET} GUI mode is unavailable on Termux."
