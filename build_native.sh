#!/bin/bash
set -euo pipefail

ROOT=$(CDPATH= cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
if [ "${1:-}" = "--run-demo" ]; then
    printf '%s\n' 'build_native.sh 不再运行未声明的 host demo；请使用 build.py run components。' >&2
    exit 2
fi
exec python3 "$ROOT/build.py" run components
