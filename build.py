#!/usr/bin/env python3
"""KenuxK BuildSystem: x86_64 内核构建入口（基于 LeonOS BuildSystem 框架移植）"""

from __future__ import annotations

import argparse
import contextlib
import io
import json
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable, Iterable

from buildsystem.core import (
    BuildFailure,
    BuildGraph,
    BuildPaths,
    BuildRunner,
    BuildSettings,
    GraphError,
    Target,
    TaskStore,
    edit_settings,
    load_settings,
    show_map,
)
from buildsystem.core.runner import ActionContext, CYAN, GREEN, RED, RESET
from buildsystem.core.state import utc_now


ROOT = Path(__file__).resolve().parent
PYTHON = sys.executable

# ===== 工具链配置 =====
# Windows 默认从固定目录找 MinGW/NASM/QEMU；WSL/Linux 默认从 PATH 找交叉编译工具。
IS_WINDOWS = platform.system().lower().startswith("win")

if IS_WINDOWS:
    # MinGW: D:\mingw64\bin
    # NASM:  C:\Program Files\NASM
    # QEMU:  D:\qemu
    MINGW_BIN = os.environ.get("MINGW_BIN", r"D:\mingw64\bin")
    NASM_PATH = os.environ.get("NASM_PATH", r"C:\Program Files\NASM")
    QEMU_PATH = os.environ.get("QEMU_PATH", r"D:\qemu")

    CC = os.environ.get("CC", os.path.join(MINGW_BIN, "gcc.exe"))
    OBJCOPY = os.environ.get("OBJCOPY", os.path.join(MINGW_BIN, "objcopy.exe"))
    AS = os.environ.get("AS", os.path.join(NASM_PATH, "nasm.exe"))
    QEMU = os.environ.get("QEMU", os.path.join(QEMU_PATH, "qemu-system-x86_64.exe"))
else:
    CC = os.environ.get("CC", "gcc")
    OBJCOPY = os.environ.get("OBJCOPY", "objcopy")
    AS = os.environ.get("AS", "nasm")
    QEMU = os.environ.get("QEMU", "qemu-system-x86_64")

EFI_CC = os.environ.get("EFI_CC", CC if IS_WINDOWS else "x86_64-w64-mingw32-gcc")
EFI_OBJCOPY = os.environ.get("EFI_OBJCOPY", OBJCOPY)

# OVMF 固件
OVMF_CODE = ROOT / "resources" / "firmware" / "OVMF_CODE.fd"
if not OVMF_CODE.exists():
    OVMF_CODE = ROOT / "OVMF_CODE.fd"

# ===== 构建参数 =====
BUILDDIR = "build" / Path("kernel")
KERNEL_ELF = ROOT / "img" / "boot" / "kernel.elf.pe"
KERNEL_BIN = ROOT / "img" / "boot" / "kernel.bin"
TRAMPOLINE_BIN = ROOT / "trampoline.bin"
LINKER_SCRIPT = ROOT / "kernel" / "arch" / "x86_64" / "boot" / "linker.ld"

# ===== 用户态组件配置 =====
# 16 个用户态组件: bash, fastfetch, git, make, gcc, clang, ld, mkfs, dd,
# xorriso, qemu, wayland/xorg, kde plasma, gnome, firefox, systemd
COMPONENTS_DIR = ROOT / "src"
COMPONENTS_BUILD = ROOT / "build" / "components"
COMPONENTS_BIN = ROOT / "bin" / "components"

# 每个组件: (名称, 源文件列表, 额外 include 目录)
COMPONENT_SOURCES: list[tuple[str, list[str], list[str]]] = [
    ("bash",       ["src/bash/shell.c"],              ["src/bash"]),
    ("fastfetch",  ["src/fastfetch/fastfetch.c"],     ["src/fastfetch"]),
    ("git",        ["src/git/git.c"],                 ["src/git"]),
    ("make",       ["src/make/make.c"],               ["src/make"]),
    ("gcc",        ["src/gcc/gcc.c"],                 ["src/gcc"]),
    ("clang",      ["src/clang/clang.c"],             ["src/clang"]),
    ("ld",         ["src/ld/ld.c"],                   ["src/ld"]),
    ("mkfs",       ["src/mkfs/mkfs.c"],               ["src/mkfs"]),
    ("dd",         ["src/dd/dd.c"],                   ["src/dd"]),
    ("xorriso",    ["src/xorriso/xorriso.c"],         ["src/xorriso"]),
    ("qemu",       ["src/qemu/qemu.c"],               ["src/qemu"]),
    ("wayland",    ["src/wayland/wayland.c"],         ["src/wayland"]),
    ("kde",        ["src/kde/kde.c"],                 ["src/kde"]),
    ("gnome",      ["src/gnome/gnome.c"],             ["src/gnome"]),
    ("firefox",    ["src/firefox/firefox.c"],         ["src/firefox"]),
    ("systemd",    ["src/systemd/systemd_tools.c"],   ["src/systemd"]),
]

# 用户态编译标志 (使用宿主工具链,不是内核 freestanding)
CFLAGS_COMPONENTS = [
    "-Wall", "-Wextra", "-std=c17", "-O2",
]

# 编译器标志
CFLAGS_BASE = [
    "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Wcast-align",
    "-Wconversion", "-Wsign-conversion", "-Wnull-dereference",
    "-Wdouble-promotion", "-Wformat=2", "-std=c17", "-O2",
]
CFLAGS_KERNEL = [
    "-m64", "-mcmodel=large", "-ffreestanding", "-fno-pic",
    "-nostdlib", "-nostartfiles", "-nodefaultlibs",
    "-fno-stack-protector",
    "-mno-stack-arg-probe", "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
    "-DKAL_KERNEL", "-DVGA_NATIVE",
]
INCLUDES = [
    "-Iinclude", "-Ikernel", "-Ikernel/include", "-Ikernel/kernel",
    "-Ikernel/arch/x86_64/include", "-Ikernel/lib/libc/include",
    "-Ikernel/systemd/include", "-Iapps/container-os/include",
    "-Ikernel/gui/include", "-Ikernel/compat", "-Ikal/include",
]
import sys
import os

# 根据操作系统选择汇编格式
if sys.platform.startswith('win'):
    ASFLAGS = ["-f", "win64"]
else:
    ASFLAGS = ["-f", "elf64"]

# 源文件目录（自动扫描）
KERNEL_SOURCE_DIRS = [
    "kernel/kernel",
    "kernel/arch/x86_64",
    "kernel/api",
    "kernel/lib/libc",
    "kernel/systemd",
]

# 需要 cc_boot 规则（特殊编译标志）的文件
BOOT_COMPILE_SOURCES = {"kernel/kernel/kernel.c", "kernel/kernel/syscall.c"}

# 排除的源文件（UEFI 启动模式下不需要 Multiboot 入口 boot.S，
# 它与 start64.s 都定义了 _start，会导致链接冲突）
EXCLUDED_SOURCES = {
    "kernel/arch/x86_64/boot/boot.S",
}

# 构建 OVMF 时需要的 ESP 目录
ESP_DIR = ROOT / "esp"
SERIAL_LOG = ROOT / "serial.log"

_COLLECT_CACHE: dict[tuple[str, ...], tuple[Path, ...]] = {}


def relative(path: Path) -> str:
    """将路径转换为相对于 ROOT 的 POSIX 字符串"""
    if path.is_absolute():
        try:
            return path.relative_to(ROOT).as_posix()
        except ValueError:
            return path.as_posix()
    return path.as_posix()


def collect_kernel_sources() -> tuple[list[Path], list[Path]]:
    ninja_file = ROOT / "kernelbuild.ninja"
    c_sources: list[Path] = []
    asm_sources: list[Path] = []

    def collect_paths(*patterns: str) -> list[Path]:
        found: list[Path] = []
        for pattern in patterns:
            found.extend(ROOT.glob(pattern))
        return found

    if ninja_file.exists():
        import re
        content = ninja_file.read_text(encoding="utf-8")
        for match in re.finditer(r'build\s+\$builddir/\S+\.o:\s+\w+\s+(\S+)', content):
            src_path = match.group(1)
            if src_path.startswith("$"):
                continue
            if src_path.replace("\\", "/") in EXCLUDED_SOURCES:
                continue
            full_path = ROOT / src_path
            if full_path.suffix.lower() == ".c":
                c_sources.append(full_path)
            elif full_path.suffix.lower() == ".s":
                asm_sources.append(full_path)
        c_sources = sorted(set(c_sources))
        asm_sources = sorted(set(asm_sources))
    else:
        c_sources = collect_paths("kernel/kernel/*.c", "kernel/arch/x86_64/*.c",
                                  "kernel/api/*.c", "kernel/lib/libc/*.c")
        asm_sources = collect_paths("kernel/arch/x86_64/*.S", "kernel/arch/x86_64/*.s")

    # kernelbuild.ninja 可能漏掉 GUI 应用、兼容层和 KAL 适配层，这些文件会被
    # 当前 Kenux GUI/系统调用路径引用，链接时必须参与构建。
    c_sources.extend(collect_paths(
        "kernel/gui/src/*.c",
        "kernel/compat/*.c",
        "kal/src/*.c",
        "kal/adapters/*.c",
        "apps/container-os/src/*.c",
        "apps/fastfetch.c",
        "apps/calculator.c",
        "apps/snake_game.c",
        "apps/tetris.c",
    ))
    c_sources = sorted(set(c_sources))

    # Windows 大小写不敏感去重
    seen: set[Path] = set()
    unique_asm: list[Path] = []
    for source in asm_sources:
        key = source.resolve()
        if key not in seen:
            seen.add(key)
            unique_asm.append(source)

    return c_sources, unique_asm


def object_path(paths: BuildPaths, source: Path) -> Path:
    """源文件对应的目标文件路径"""
    rel = source.relative_to(ROOT)
    # 保持目录结构: build/kernel/kernel/kernel.o, build/kernel/arch/x86_64/gdt.o 等
    return paths.objects / rel.with_suffix(rel.suffix + ".o")


def add_compile(
    graph: BuildGraph,
    paths: BuildPaths,
    name: str,
    source: Path,
    flags: list[str],
    implicit: Iterable[Path] = (),
    *,
    kind: str = "compile",
) -> Path:
    """添加编译目标"""
    output = object_path(paths, source)
    depfile = output.with_suffix(output.suffix + ".d")
    command = tuple(flags + ["-MMD", "-MF", relative(depfile), "-c", relative(source), "-o", relative(output)])
    graph.add(
        Target(
            name=name,
            outputs=(output,),
            inputs=(source,),
            implicit_inputs=tuple(implicit),
            kind=kind,
            source=source,
            command=command,
            depfile=depfile,
        )
    )
    return output


def add_nasm(
    graph: BuildGraph,
    paths: BuildPaths,
    name: str,
    source: Path,
    implicit: Iterable[Path] = (),
) -> Path:
    """添加 NASM 汇编目标"""
    output = object_path(paths, source)
    command = tuple([AS] + ASFLAGS + [relative(source), "-o", relative(output)])
    graph.add(
        Target(
            name=name,
            outputs=(output,),
            inputs=(source,),
            implicit_inputs=tuple(implicit),
            kind="assemble",
            source=source,
            command=command,
        )
    )
    return output


def add_link(
    graph: BuildGraph,
    name: str,
    output: Path,
    inputs: Iterable[Path],
    command: list[str],
    implicit: Iterable[Path] = (),
) -> Target:
    """添加链接目标"""
    return graph.add(
        Target(
            name=name,
            outputs=(output,),
            inputs=tuple(inputs),
            implicit_inputs=tuple(implicit),
            kind="link",
            command=tuple(command + ["-o", relative(output)]),
        )
    )


def ensure_tools(*, require_kernel: bool = False, require_bootloader: bool = False,
                 require_qemu: bool = False) -> None:
    required = [("gcc", CC)]
    if require_kernel:
        required.extend((("objcopy", OBJCOPY), ("nasm", AS)))
    if require_bootloader:
        required.append(("EFI gcc", EFI_CC))
    if require_qemu:
        required.append(("qemu-system-x86_64", QEMU))
    missing = []
    for name, configured in required:
        if shutil.which(configured) is None and not Path(configured).is_file():
            missing.append(f"{name} ({configured})")
    if missing:
        raise BuildFailure(
            "缺少工具链: " + ", ".join(missing) +
            "\n请安装目标平台所需的 gcc、objcopy、NASM；运行任务还需要 QEMU。"
        )


def validate_sources(
    *, require_kernel: bool = False, require_components: bool = False,
    require_bootloader: bool = False,
) -> None:
    missing: list[str] = []
    if require_components:
        for source in (ROOT / path for _, sources, _ in COMPONENT_SOURCES for path in sources):
            if not source.is_file():
                missing.append(relative(source))
    if require_kernel:
        for source in (ROOT / path for path in BOOT_COMPILE_SOURCES):
            if not source.is_file():
                missing.append(relative(source))
        if not LINKER_SCRIPT.is_file():
            missing.append(relative(LINKER_SCRIPT))
    if require_bootloader and not (ROOT / "bootloader" / "bootx64.c").is_file():
        missing.append("bootloader/bootx64.c")
    if missing:
        raise BuildFailure("缺少构建输入，拒绝继续: " + ", ".join(sorted(set(missing))))


def qemu_command(*, debug: bool = False, vars_path: Path | None = None) -> tuple[str, ...]:
    """生成 QEMU 启动命令（UEFI 模式，VVFAT 挂载 esp 目录）"""
    command = [QEMU, "-m", "256M"]
    firmware_code = OVMF_CODE
    system_code = Path("/usr/share/edk2/x64/OVMF_CODE.4m.fd")
    if not IS_WINDOWS and system_code.exists():
        firmware_code = system_code
    if firmware_code.exists() and vars_path is not None:
        command += [
            "-drive", f"if=pflash,format=raw,readonly=on,file={firmware_code}",
            "-drive", f"if=pflash,format=raw,file={vars_path}",
        ]
    elif firmware_code.exists():
        command += ["-drive", f"if=pflash,format=raw,readonly=on,file={firmware_code}"]
    # VVFAT 模式直接挂载 esp 目录，并把 Kenux EFI 盘设为固件第一启动项。
    command += ["-drive", f"if=none,id=kenuxesp,file=fat:rw:{ESP_DIR},format=raw"]
    command += ["-device", "ide-hd,drive=kenuxesp,bootindex=0"]
    command += ["-boot", "menu=off,strict=on"]
    command += ["-serial", f"file:{SERIAL_LOG}"]
    command += ["-debugcon", "file:/tmp/kenux-debugcon.log", "-global", "isa-debugcon.iobase=0xe9"]
    command += ["-display", "none" if debug else "gtk"]
    command += ["-no-reboot"]
    return tuple(command)


def build_graph(paths: BuildPaths) -> BuildGraph:
    """构建 KenuxK 的完整依赖图"""
    graph = BuildGraph(ROOT)

    # ===== 1. 编译所有 .c 和 .S 源文件 =====
    kernel_objects: list[Path] = []

    # 从 kernelbuild.ninja 解析源文件列表，确保与原构建系统一致
    c_sources, asm_sources = collect_kernel_sources()

    # C 源文件
    for source in c_sources:
        rel = relative(source)
        is_boot = rel in BOOT_COMPILE_SOURCES
        flags = [CC] + CFLAGS_BASE + CFLAGS_KERNEL + INCLUDES
        if is_boot:
            # boot 编译使用相同标志但标记不同 kind
            obj = add_compile(graph, paths, f"compile:boot:{rel}", source, flags, kind="compile")
        else:
            obj = add_compile(graph, paths, f"compile:{rel}", source, flags)
        kernel_objects.append(obj)

    # 汇编源文件 (.S 和 .s) — 已在 collect_kernel_sources 中去重
    for source in asm_sources:
        rel = relative(source)
        obj = add_nasm(graph, paths, f"assemble:{rel}", source)
        kernel_objects.append(obj)

    # 确保 start64.o 在链接列表第一位，这样 _start 位于 .text 段开头。
    # 实际加载地址由 linker.ld 中的 KERNEL_VMA/KERNEL_PHYS 决定。
    start64_obj = None
    for obj in kernel_objects:
        if "start64" in obj.name:
            start64_obj = obj
            break
    if start64_obj is not None:
        kernel_objects.remove(start64_obj)
        kernel_objects.insert(0, start64_obj)

    # ===== 2. 链接内核 =====
    # Windows: 使用 PE 格式，ImageBase=0 让 RVA=物理地址
    # Linux: 使用 ELF 格式，直接链接
    link_command = [
        CC, "-m64", "-T", relative(LINKER_SCRIPT),
        "-nostdlib", "-nodefaultlibs", "-nostartfiles",
        "-Wl,-e,_start", "-no-pie",
        "-Wl,--allow-multiple-definition",
    ]
    if sys.platform.startswith('win'):
        link_command.append("-Wl,--image-base=0x0")
    link_command.extend([
        "-o", relative(KERNEL_ELF),
    ] + [relative(obj) for obj in kernel_objects])

    add_link(graph, "kernel-image", KERNEL_ELF, kernel_objects, link_command,
             implicit=(LINKER_SCRIPT,))

    # ===== 3. objcopy 生成 flat binary =====
    graph.add(
        Target(
            name="kernel-bin",
            outputs=(KERNEL_BIN,),
            inputs=(KERNEL_ELF,),
            kind="generate",
            command=(OBJCOPY, "-O", "binary",
                     "--only-section=.text",
                     "--only-section=.rodata",
                     "--only-section=.rdata",
                     "--only-section=.data",
                     relative(KERNEL_ELF), relative(KERNEL_BIN)),
        )
    )

    # ===== 3. (已合并到链接步骤) =====

    graph.add(Target(name="kernel", depends_on=("kernel-bin",), group=True, kind="aggregate"))

    # ===== 4. 更新 ESP 目录中的内核文件 =====
    esp_kernel = ESP_DIR / "kernel.elf"

    def copy_kernel(context: ActionContext) -> None:
        ESP_DIR.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(KERNEL_ELF, esp_kernel)

    graph.add(
            Target(
                name="esp-update",
                outputs=(esp_kernel,),
                inputs=(KERNEL_ELF, KERNEL_BIN),
            kind="generate",
            action=copy_kernel,
            action_key="copy-kernel-v1",
        )
    )

    # ===== 5. 构建 bootloader (bootx64.efi) =====
    bootloader_source = ROOT / "bootloader" / "bootx64.c"
    bootloader_efi = ESP_DIR / "EFI" / "BOOT" / "BOOTX64.EFI"
    bootloader_pe = ROOT / "build" / "bootloader" / "BOOTX64.EFI.pe"

    if bootloader_source.exists():
        efi_flags = [
            EFI_CC, "-m64", "-ffreestanding", "-fno-builtin", "-mno-red-zone",
            "-fno-stack-protector", "-fno-short-enums", "-fshort-wchar",
            "-fno-asynchronous-unwind-tables", "-fno-unwind-tables",
            "-Wall", "-Wextra", "-O2",
            "-Wl,-e,efi_main", "-Wl,--subsystem,10", "-Wl,--image-base=0x0",
            "-nostdlib", "-nostartfiles",
            "-I", relative(ROOT / "bootloader"),
        ]
        def build_bootloader(context: ActionContext) -> None:
            bootloader_efi.parent.mkdir(parents=True, exist_ok=True)
            bootloader_pe.parent.mkdir(parents=True, exist_ok=True)
            context.run(
                tuple(efi_flags + ["-o", relative(bootloader_pe), relative(bootloader_source)]),
                announce=True,
            )
            context.run(
                (EFI_OBJCOPY, "-j", ".text", "-j", ".rdata", "-j", ".data", "-j", ".reloc",
                 "--target=efi-app-x86_64", relative(bootloader_pe), relative(bootloader_efi)),
                announce=True,
            )

        graph.add(
            Target(
                name="bootloader",
                outputs=(bootloader_efi,),
                inputs=(bootloader_source,),
                implicit_inputs=(ROOT / "bootloader" / "include" / "efi.h",
                                 ROOT / "bootloader" / "include" / "fbc.h"),
                kind="generate",
                action=build_bootloader,
                action_key="bootloader-v4",
            )
        )
    else:
        graph.add(Target(name="bootloader", group=True, kind="aggregate"))

    # ===== 6. 聚合目标 =====
    graph.add(Target(name="all", depends_on=("kernel", "esp-update", "bootloader"), group=True, kind="aggregate"))

    # ===== 7. 运行目标 =====
    graph.add(Target(name="run", inputs=(KERNEL_BIN, esp_kernel), depends_on=("all",),
                    kind="command", command=qemu_command()))
    graph.add(Target(name="run-debug", inputs=(KERNEL_BIN, esp_kernel), depends_on=("all",),
                    kind="command", command=qemu_command(debug=True)))

    # ===== 8. 清理目标 =====
    def clean(context: ActionContext) -> None:
        for directory in (paths.out, paths.legacy_out, paths.target_state, paths.tmp):
            if directory.exists():
                shutil.rmtree(directory)
        context.runner.store.clear_target_states()
        paths.ensure()

    graph.add(Target(name="clean", kind="command", action=clean, action_key="clean-v1", always=True))

    # ===== 9. 重新构建 (强制) =====
    def rebuild(context: ActionContext) -> None:
        clean(context)
        # 通过依赖 all 目标，让 runner 正常调度
        all_target = graph.targets["all"]
        context.runner.run((all_target,), "rebuild-all")

    graph.add(Target(name="rebuild", depends_on=("clean", "all"), group=True, kind="aggregate"))

    # ===== 10. 用户态组件构建 =====
    # 16 个组件: bash, fastfetch, git, make, gcc, clang, ld, mkfs, dd,
    # xorriso, qemu, wayland/xorg, kde, gnome, firefox, systemd
    component_exes: list[Path] = []
    exe_suffix = ".exe" if IS_WINDOWS else ""

    for comp_name, src_files, inc_dirs in COMPONENT_SOURCES:
        comp_sources_full: list[Path] = []
        for sf in src_files:
            src_path = ROOT / sf
            if not src_path.is_file():
                raise BuildFailure(f"组件 {comp_name} 缺少源文件: {relative(src_path)}")
            comp_sources_full.append(src_path)

        if not comp_sources_full:
            graph.add(Target(name=f"component:{comp_name}", group=True, kind="aggregate"))
            continue

        comp_exe = COMPONENTS_BIN / f"{comp_name}{exe_suffix}"
        comp_flags = [CC] + CFLAGS_COMPONENTS + [f"-I{d}" for d in inc_dirs]

        def build_component(context: ActionContext, *,
                            sources: list[Path] = comp_sources_full,
                            output: Path = comp_exe,
                            flags: list[str] = comp_flags,
                            name: str = comp_name) -> None:
            output.parent.mkdir(parents=True, exist_ok=True)
            # 先编译每个 .c 为 .o，再链接
            obj_files: list[str] = []
            for src in sources:
                obj_path = COMPONENTS_BUILD / f"{name}_{src.stem}.o"
                obj_path.parent.mkdir(parents=True, exist_ok=True)
                context.run(
                    tuple(flags + ["-c", relative(src), "-o", str(obj_path)]),
                    announce=True,
                )
                obj_files.append(str(obj_path))
            # 链接为可执行文件
            link_cmd = [CC] + obj_files + ["-o", str(output)]
            if not IS_WINDOWS:
                link_cmd.append("-no-pie")
            context.run(tuple(link_cmd), announce=True)

        graph.add(
            Target(
                name=f"component:{comp_name}",
                outputs=(comp_exe,),
                inputs=tuple(comp_sources_full),
                kind="generate",
                action=build_component,
                action_key=f"component-{comp_name}-v1",
            )
        )
        component_exes.append(comp_exe)

    # 聚合所有组件
    component_dep_names = tuple(f"component:{name}" for name, _, _ in COMPONENT_SOURCES)
    graph.add(Target(
        name="components",
        depends_on=component_dep_names,
        group=True,
        kind="aggregate",
    ))

    return graph


def create_runner(paths: BuildPaths, graph: BuildGraph, task_id: str) -> BuildRunner:
    return BuildRunner(graph, paths, load_settings(paths), task_id)


def build_roots(graph: BuildGraph, target: Target) -> tuple[Target, ...]:
    return (target,)


def display_help() -> str:
    return """KenuxK BuildSystem

命令:
  build.py help              显示帮助
  build.py run <task>        执行构建任务
  build.py info <file>       显示目标信息
  build.py why <file>        解释为什么需要重建
  build.py affected <file>   显示受影响的文件
  build.py profile <task>    构建并显示性能分析
  build.py cache <stats|prune>  缓存管理
  build.py settings          编辑设置
  build.py map               显示依赖图

任务:
  all          构建所有内容 (内核 + bootloader + ESP 更新)
  kernel       仅构建内核 (kernel.bin)
  bootloader   构建 UEFI bootloader (BOOTX64.EFI)
  esp-update   更新 ESP 目录中的内核文件
  components   构建 16 个用户态组件 (bash, gcc, git, firefox 等)
  run          构建并启动 QEMU (VGA 窗口)
  run-debug    构建并启动 QEMU (无显示, 仅串口日志)
  clean        清理构建目录
  rebuild      清理并重新构建
"""


def complete_simple(store: TaskStore, task_id: str, command: str, text: str, success: bool = True) -> None:
    log = store.log_path(task_id)
    log.parent.mkdir(parents=True, exist_ok=True)
    log.write_text(text.rstrip() + "\n", encoding="utf-8", newline="\n")
    store.update(task_id, status="done" if success else "failed",
                 started_at=utc_now(), finished_at=utc_now(), task=command,
                 error="" if success else text)


def run_foreground(paths: BuildPaths, graph: BuildGraph, task_id: str, target: Target, label: str) -> int:
    if target.name in {"run", "run-debug"}:
        vars_template = ROOT / "OVMF_VARS_4M.fd"
        system_vars = Path("/usr/share/edk2/x64/OVMF_VARS.4m.fd")
        if not IS_WINDOWS and system_vars.exists():
            vars_template = system_vars
        vars_path = Path("/tmp") / f"kenux-ovmf-vars-{os.getpid()}.fd"
        shutil.copyfile(vars_template, vars_path)
        target.command = qemu_command(debug=target.name == "run-debug", vars_path=vars_path)
    runner = create_runner(paths, graph, task_id)
    runner.run(build_roots(graph, target), label)
    return 0


def profile_target(
    paths: BuildPaths,
    graph: BuildGraph,
    target: Target,
    task_id: str,
    label: str,
    graph_seconds: float,
    *,
    json_output: bool,
) -> dict[str, object]:
    runner = create_runner(paths, graph, task_id)
    roots = build_roots(graph, target)
    if json_output:
        with contextlib.redirect_stdout(io.StringIO()):
            runner.run(roots, label)
    else:
        runner.run(roots, label)
    report = runner.profile_data()
    report["graph_build_seconds"] = round(graph_seconds, 3)
    return report


def human_label(value: object) -> str:
    return str(value).replace("_", " ").replace("-", " ").capitalize()


def format_human(title: str, value: object) -> str:
    lines = [title]
    if isinstance(value, dict):
        for key, item in value.items():
            label = human_label(key)
            if isinstance(item, (dict, list)):
                lines.append(f"{label}:")
                for sub in (item if isinstance(item, list) else [item]):
                    lines.append(f"  {sub}")
            else:
                lines.append(f"{label}: {item}")
    else:
        lines.append(str(value))
    return "\n".join(lines)


def emit_data(value: object, *, json_output: bool, title: str) -> str:
    text = (
        json.dumps(value, ensure_ascii=False, indent=2)
        if json_output
        else format_human(title, value)
    )
    print(text)
    return text


def tree_stats(path: Path) -> dict[str, int]:
    files = 0
    size = 0
    if path.exists():
        for candidate in path.rglob("*"):
            if candidate.is_file():
                files += 1
                size += candidate.stat().st_size
    return {"files": files, "bytes": size}


def cache_report(paths: BuildPaths, store: TaskStore) -> dict[str, object]:
    states = store.target_states()
    return {
        "tracked_targets": len(states),
        "target_state": tree_stats(paths.state),
        "objects": tree_stats(paths.objects),
    }


def prune_cache(paths: BuildPaths, store: TaskStore) -> dict[str, object]:
    result = store.prune_target_states()
    result["cache"] = cache_report(paths, store)
    return result


def explain_target(paths: BuildPaths, graph: BuildGraph, subject: str, task_id: str) -> dict[str, object]:
    target = graph.resolve_target(subject)
    runner = create_runner(paths, graph, task_id)
    try:
        if not target.group:
            return runner.explain(target)
        checks = [runner.explain(c) for c in graph.closure((target,)) if not c.group]
        dirty = [check for check in checks if bool(check["will_rebuild"])]
        return {
            "target": target.name,
            "kind": target.kind,
            "will_rebuild": bool(dirty),
            "checked_targets": len(checks),
            "dirty_targets": dirty,
        }
    finally:
        runner.close()


def affected_targets(paths: BuildPaths, graph: BuildGraph, subject: str) -> dict[str, object]:
    path = graph.path(subject)
    roots = {target.name: target for target in graph.related_targets(path)}
    store = TaskStore(paths)
    for name, state in store.target_states().items():
        target = graph.targets.get(name)
        if target is None:
            continue
        for raw in state.get("depfile_dependencies", []):
            if isinstance(raw, str) and graph.path(raw) == path:
                roots[target.name] = target
                break
    if not roots:
        raise GraphError(f"no graph or depfile target references {graph.relative(path)}")
    affected = graph.dependents(roots.values())
    return {
        "file": graph.relative(path),
        "direct_targets": sorted(roots),
        "affected_count": len(affected),
        "affected_targets": [
            {
                "name": target.name,
                "kind": target.kind,
                "outputs": [graph.relative(output) for output in target.outputs],
            }
            for target in affected
        ],
    }


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="build.py")
    p.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    p.add_argument("--task-id", help=argparse.SUPPRESS)
    p.add_argument("--json", dest="json_output", action="store_true", help="输出 JSON 格式")
    commands = p.add_subparsers(dest="command")
    commands.add_parser("help")
    run = commands.add_parser("run")
    run.add_argument("task")
    info = commands.add_parser("info")
    info.add_argument("subject")
    why = commands.add_parser("why")
    why.add_argument("subject")
    affected = commands.add_parser("affected")
    affected.add_argument("file")
    profile = commands.add_parser("profile")
    profile.add_argument("task")
    cache = commands.add_parser("cache")
    cache.add_argument("action", choices=("stats", "prune"))
    commands.add_parser("settings")
    commands.add_parser("map")
    return p


def parse_arguments(argv: list[str] | None) -> argparse.Namespace:
    values = list(argv if argv is not None else sys.argv[1:])
    json_output = "--json" in values
    values = [v for v in values if v != "--json"]
    if json_output:
        values.insert(0, "--json")
    return parser().parse_args(values)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    if arguments.command is None:
        arguments.command = "help"
    paths = BuildPaths(ROOT)
    paths.ensure()
    store = TaskStore(paths)
    task_id = arguments.task_id if arguments.worker and arguments.task_id else store.new_id(list(argv or sys.argv[1:]))
    if arguments.worker and not arguments.task_id:
        raise BuildFailure("background worker requires a task ID")
    try:
        if arguments.command == "help":
            text = display_help()
            if arguments.json_output:
                emit_data({"help": text.rstrip()}, json_output=True, title="Help")
            else:
                print(text, end="")
            complete_simple(store, task_id, "help", text)
            return 0
        if arguments.command == "cache":
            data = cache_report(paths, store) if arguments.action == "stats" else prune_cache(paths, store)
            text = emit_data(data, json_output=arguments.json_output, title=f"Cache {arguments.action}")
            complete_simple(store, task_id, f"cache {arguments.action}", text)
            return 0
        if arguments.command == "settings":
            edit_settings(paths)
            complete_simple(store, task_id, "settings", "settings closed")
            return 0
        needs_build = arguments.command in {"run", "profile"}
        if needs_build:
            requested_task = arguments.task
            require_kernel = requested_task in {"all", "kernel", "run", "run-debug", "rebuild"}
            require_components = requested_task == "components"
            require_bootloader = requested_task in {"all", "bootloader", "run", "run-debug", "rebuild"}
            ensure_tools(
                require_kernel=require_kernel,
                require_bootloader=require_bootloader,
                require_qemu=arguments.command == "run" and requested_task in {"run", "run-debug"},
            )
            validate_sources(
                require_kernel=require_kernel,
                require_components=require_components,
                require_bootloader=require_bootloader,
            )
        graph_started = time.perf_counter()
        graph = build_graph(paths)
        graph_seconds = time.perf_counter() - graph_started
        if arguments.command == "run":
            return run_foreground(paths, graph, task_id, graph.resolve_target(arguments.task), f"run {arguments.task}")
        if arguments.command == "profile":
            target = graph.resolve_target(arguments.task)
            data = profile_target(paths, graph, target, task_id, f"profile {arguments.task}",
                                  graph_seconds, json_output=arguments.json_output)
            store.update(task_id, profile=data)
            emit_data(data, json_output=arguments.json_output, title="Build profile")
            return 0
        if arguments.command == "why":
            data = explain_target(paths, graph, arguments.subject, task_id)
            text = emit_data(data, json_output=arguments.json_output, title="Rebuild explanation")
            complete_simple(store, task_id, f"why {arguments.subject}", text)
            return 0
        if arguments.command == "affected":
            data = affected_targets(paths, graph, arguments.file)
            text = emit_data(data, json_output=arguments.json_output, title="Affected targets")
            complete_simple(store, task_id, f"affected {arguments.file}", text)
            return 0
        if arguments.command == "info":
            target = graph.resolve_target(arguments.subject)
            data = {
                "name": target.name,
                "kind": target.kind,
                "outputs": [graph.relative(p) for p in target.outputs],
                "inputs": [graph.relative(p) for p in target.all_inputs()],
                "depends_on": list(target.depends_on),
            }
            text = emit_data(data, json_output=arguments.json_output, title="Target information")
            complete_simple(store, task_id, f"info {arguments.subject}", text)
            return 0
        if arguments.command == "map":
            show_map(graph)
            complete_simple(store, task_id, "map", "dependency map closed")
            return 0
        raise BuildFailure(f"unsupported command: {arguments.command}")
    except (BuildFailure, GraphError, FileNotFoundError, ValueError) as exc:
        message = f"build: {exc}"
        print(f"{RED}{message}{RESET}", file=sys.stderr)
        try:
            record = store.read(task_id)
            if record.get("status") not in {"done", "failed"}:
                complete_simple(store, task_id, str(arguments.command), message, success=False)
        except (FileNotFoundError, ValueError):
            pass
        return 1


if __name__ == "__main__":
    # Windows 下把固定工具链目录加入 PATH；WSL/Linux 下直接使用系统 PATH。
    if IS_WINDOWS:
        tool_paths = [MINGW_BIN, NASM_PATH, QEMU_PATH]
        os.environ["PATH"] = os.pathsep.join(tool_paths) + os.pathsep + os.environ.get("PATH", "")
    raise SystemExit(main())
