#!/bin/bash
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
task=${1:-all}
case "$task" in
    apps) task=components ;;
    sync|kex)
        printf '%s\n' "旧任务 '$task' 已移除；请使用 build.py run components。" >&2
        exit 2
        ;;
    all|kernel|bootloader|esp-update|components|run|run-debug) ;;
    *) printf '未知构建任务: %s\n' "$task" >&2; exit 2 ;;
esac
exec python3 "$ROOT/build.py" run "$task"
