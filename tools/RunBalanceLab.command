#!/bin/zsh
set -euo pipefail
task_root=${0:A:h:h}
cd -- "$task_root"
exec python3 tools/run_balance_lab.py --open "$@"
