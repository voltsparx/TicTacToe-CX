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
