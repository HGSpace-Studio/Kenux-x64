#!/bin/bash

# 编译用户空间应用程序。注意：这些应用程序需要内核提供系统调用支持。

echo "编译用户空间应用程序..."

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

command -v python3 >/dev/null 2>&1 || { echo "错误: python3 未找到"; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo "错误: ninja 未找到"; exit 1; }

cd "$ROOT_DIR"
python3 tools/gen_ninja.py
ninja user

echo "编译完成！"
