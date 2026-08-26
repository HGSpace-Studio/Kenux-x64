from __future__ import annotations

import json
import os
import sys
from pathlib import Path

from .model import BuildGraph
from .runner import BuildSettings
from .state import BuildPaths


def load_settings(paths: BuildPaths) -> BuildSettings:
    """从 JSON 配置文件加载设置（Windows 兼容，无需 tomllib）"""
    defaults = BuildSettings.automatic()
    if not paths.settings.exists():
        save_settings(paths, defaults)
        return defaults
    try:
        raw = json.loads(paths.settings.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return defaults
    build = raw.get("build", {})
    if not isinstance(build, dict):
        return defaults
    return BuildSettings(
        worker_threads=max(1, int(build.get("worker_threads", defaults.worker_threads))),
        max_processes=max(1, int(build.get("max_processes", defaults.max_processes))),
        download_retries=max(0, int(build.get("download_retries", defaults.download_retries))),
    )


def save_settings(paths: BuildPaths, settings: BuildSettings) -> None:
    """保存设置为 JSON 格式"""
    paths.config.mkdir(parents=True, exist_ok=True)
    data = {
        "build": {
            "worker_threads": settings.worker_threads,
            "max_processes": settings.max_processes,
            "download_retries": settings.download_retries,
        }
    }
    paths.settings.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )


def require_tty() -> None:
    if not sys.stdin.isatty() or not sys.stdout.isatty():
        raise RuntimeError("此命令需要交互式终端")


def edit_settings(paths: BuildPaths) -> None:
    """简单的文本菜单编辑设置（不依赖 curses）"""
    require_tty()
    settings = load_settings(paths)
    values = [settings.worker_threads, settings.max_processes, settings.download_retries]
    labels = ["Worker 线程数", "最大进程数", "下载重试次数"]

    while True:
        print("\n" + "=" * 40)
        print("  KenuxK BuildSystem 设置")
        print("=" * 40)
        print(f"  检测到 CPU 核心数: {max(1, os.cpu_count() or 1)}")
        print("-" * 40)
        for index, label in enumerate(labels):
            print(f"  [{index + 1}] {label}: {values[index]}")
        print("-" * 40)
        print("  [s] 保存并退出")
        print("  [q] 不保存退出")
        print("-" * 40)

        choice = input("选择: ").strip().lower()

        if choice in ("1", "2", "3"):
            idx = int(choice) - 1
            try:
                new_val = input(f"  新的 {labels[idx]} 值 (当前 {values[idx]}): ").strip()
                if new_val:
                    values[idx] = max(0 if idx == 2 else 1, int(new_val))
            except ValueError:
                print("  无效输入，请输入数字")
        elif choice == "s":
            save_settings(
                paths,
                BuildSettings(
                    worker_threads=max(1, values[0]),
                    max_processes=max(1, values[1]),
                    download_retries=max(0, values[2]),
                ),
            )
            print("  设置已保存")
            return
        elif choice == "q":
            print("  未保存")
            return


def show_map(graph: BuildGraph) -> None:
    """打印依赖图（分页显示，不依赖 curses）"""
    require_tty()
    lines = graph.map_lines()
    page_size = 20
    for i in range(0, len(lines), page_size):
        chunk = lines[i:i + page_size]
        for line in chunk:
            print(line)
        if i + page_size < len(lines):
            input(f"\n  -- 更多 ({i + page_size}/{len(lines)})，按回车继续 --")
            print()
    print(f"\n  共 {len(lines)} 行")
