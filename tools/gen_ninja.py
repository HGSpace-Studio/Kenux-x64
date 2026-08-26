#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BUILD_DIR = Path("build")
CONFIG_FILE = BUILD_DIR / "ninja_config.json"
OBJ_DIR = BUILD_DIR / "obj"

KERNEL_DIR = Path("kernel")
ARCH_DIR = KERNEL_DIR / "arch" / "x86_64"
LIBC_DIR = KERNEL_DIR / "lib" / "libc"
BOOTLOADER_DIR = Path("bootloader")
BIN_DIR = Path("bin")


def rel(path: Path | str) -> Path:
    path = Path(path)
    if path.is_absolute():
        try:
            return path.relative_to(ROOT)
        except ValueError:
            return path
    return path


def slash(path: Path | str) -> str:
    return str(path).replace("\\", "/")


def ninja_escape(value: Path | str) -> str:
    text = slash(value)
    return text.replace("$", "$$").replace(" ", "$ ").replace(":", "$:")


def obj_for(source: Path) -> Path:
    return OBJ_DIR / rel(source).with_suffix(".o")


def discover_gnu_efi_dir(override: str | None) -> Path:
    if override:
        return Path(override)

    env_override = os.environ.get("GNU_EFI_DIR") or os.environ.get("GNU_EFI_PATH")
    if env_override:
        return Path(env_override)

    candidates = [
        Path("/usr/lib"),
        Path("/usr/lib64"),
        Path("/usr/lib/gnu-efi"),
        Path("/usr/lib64/gnu-efi"),
        Path("/usr/lib64/gnuefi"),
        Path("/usr/lib/x86_64-linux-gnu"),
        Path.home() / "gnu-efi",
    ]
    required = [
        "crt0-efi-x86_64.o",
        "elf_x86_64_efi.lds",
        "libefi.a",
        "libgnuefi.a",
    ]

    for candidate in candidates:
        if all((candidate / item).exists() for item in required):
            return candidate

    return Path("/usr/lib")


def write_var(lines: list[str], name: str, value: str) -> None:
    lines.append(f"{name} = {value}")


def write_cc_edge(lines: list[str], source: Path, cflags: str) -> Path:
    output = obj_for(source)
    lines.append(f"build {ninja_escape(output)}: cc {ninja_escape(source)}")
    lines.append(f"  cflags = {cflags}")
    lines.append(f"  out_dir = {ninja_escape(output.parent)}")
    lines.append("")
    return output


def write_as_edge(lines: list[str], source: Path) -> Path:
    output = obj_for(source)
    lines.append(f"build {ninja_escape(output)}: as {ninja_escape(source)}")
    lines.append(f"  out_dir = {ninja_escape(output.parent)}")
    lines.append("")
    return output


def sorted_sources(root: Path, suffix: str) -> list[Path]:
    return sorted(Path(path).relative_to(ROOT) for path in (ROOT / root).rglob(f"*{suffix}"))


def build_kernel(lines: list[str], kernel_cflags: str) -> Path:
    boot_obj = write_as_edge(lines, ARCH_DIR / "boot" / "boot.S")
    interrupt_obj = write_as_edge(lines, ARCH_DIR / "interrupt.S")

    arch_sources = [
        source
        for source in sorted_sources(ARCH_DIR, ".c")
        if "boot" not in source.parts and source.name != "page_fault.c"
    ]
    arch_objects = [write_cc_edge(lines, source, kernel_cflags) for source in arch_sources]

    core_sources = [
        KERNEL_DIR / "kernel" / "kernel.c",
        KERNEL_DIR / "kernel" / "syscall.c",
        KERNEL_DIR / "kernel" / "interrupt.c",
    ]
    core_objects = [write_cc_edge(lines, source, kernel_cflags) for source in core_sources]

    libc_sources = [
        source
        for source in sorted_sources(LIBC_DIR, ".c")
        if source.name != "memory.c"
    ]
    libc_objects = [write_cc_edge(lines, source, kernel_cflags) for source in libc_sources]

    kernel_elf = Path("iso") / "boot" / "kernel.elf"
    objects = [boot_obj, interrupt_obj] + arch_objects + core_objects + libc_objects
    inputs = " ".join(ninja_escape(obj) for obj in objects)
    linker = ARCH_DIR / "boot" / "linker.ld"
    lines.append(
        f"build {ninja_escape(kernel_elf)}: link_kernel {inputs} | {ninja_escape(linker)}"
    )
    lines.append(f"  out_dir = {ninja_escape(kernel_elf.parent)}")
    lines.append("")
    return kernel_elf


def build_bootloader(lines: list[str], boot_cflags: str) -> tuple[Path, Path]:
    sources = [
        BOOTLOADER_DIR / "main.c",
        BOOTLOADER_DIR / "graphics.c",
        BOOTLOADER_DIR / "logger.c",
        BOOTLOADER_DIR / "progress.c",
        BOOTLOADER_DIR / "kernel.c",
        BOOTLOADER_DIR / "serial.c",
    ]
    objects = [write_cc_edge(lines, source, boot_cflags) for source in sources]

    efi_so = BOOTLOADER_DIR / "BOOTX64.EFI.so"
    efi = BOOTLOADER_DIR / "BOOTX64.EFI"
    inputs = " ".join(ninja_escape(obj) for obj in objects)
    lines.append(f"build {ninja_escape(efi_so)}: link_efi {inputs} || build/gnu-efi.stamp")
    lines.append(f"  out_dir = {ninja_escape(efi_so.parent)}")
    lines.append("")
    lines.append(f"build {ninja_escape(efi)}: efi_image {ninja_escape(efi_so)}")
    lines.append(f"  out_dir = {ninja_escape(efi.parent)}")
    lines.append("")

    simple_obj = write_cc_edge(lines, BOOTLOADER_DIR / "main_simple.c", boot_cflags)
    simple_so = BOOTLOADER_DIR / "BOOTX64-simple.EFI.so"
    simple_efi = BOOTLOADER_DIR / "BOOTX64-simple.EFI"
    lines.append(f"build {ninja_escape(simple_so)}: link_efi {ninja_escape(simple_obj)} || build/gnu-efi.stamp")
    lines.append(f"  out_dir = {ninja_escape(simple_so.parent)}")
    lines.append("")
    lines.append(f"build {ninja_escape(simple_efi)}: efi_image {ninja_escape(simple_so)}")
    lines.append(f"  out_dir = {ninja_escape(simple_efi.parent)}")
    lines.append("")

    return efi, simple_efi


def build_user_programs(lines: list[str], user_cflags: str) -> list[Path]:
    init_obj = write_cc_edge(lines, BIN_DIR / "init.c", user_cflags)
    shell_obj = write_cc_edge(lines, BIN_DIR / "shell_commands.c", user_cflags)

    init_bin = BIN_DIR / "init"
    shell_bin = BIN_DIR / "shell"
    linker = BIN_DIR / "linker.ld"
    lines.append(
        f"build {ninja_escape(init_bin)}: link_user {ninja_escape(init_obj)} | {ninja_escape(linker)}"
    )
    lines.append(f"  out_dir = {ninja_escape(init_bin.parent)}")
    lines.append("")
    lines.append(
        f"build {ninja_escape(shell_bin)}: link_user {ninja_escape(shell_obj)} | {ninja_escape(linker)}"
    )
    lines.append(f"  out_dir = {ninja_escape(shell_bin.parent)}")
    lines.append("")
    return [init_bin, shell_bin]


def generate(args: argparse.Namespace) -> str:
    gnu_efi_dir = discover_gnu_efi_dir(args.gnu_efi_dir)
    effective = {
        "cc": os.environ.get("CC", args.cc),
        "assembler": os.environ.get("AS", args.assembler),
        "ld": os.environ.get("LD", args.ld),
        "objcopy": os.environ.get("OBJCOPY", args.objcopy),
        "python": args.python,
    }

    kernel_cflags = (
        "-Wall -Wextra -O2 -ffreestanding -fno-builtin -fno-stack-protector "
        "-fno-pic -fno-pie -nostdlib -m64 -mno-red-zone -mcmodel=kernel "
        "-Ikernel/arch/x86_64/include -Ikernel/include -Ikernel/lib/libc/include"
    )
    boot_cflags = (
        "-Wall -O2 -fno-stack-protector -fPIC -pie -mno-red-zone -m64 "
        "-DEFI_FUNCTION_WRAPPER -fshort-wchar -Ibootloader "
        "-ffreestanding -fno-builtin"
    )
    user_cflags = (
        "-m64 -ffreestanding -fno-pic -fno-pie -nostdlib -nostartfiles "
        "-nodefaultlibs -Ikernel/include -Ikernel/arch/x86_64/include "
        "-Ikernel/arch/x86_64/include/arch -Ikernel/lib/libc/include"
    )

    lines: list[str] = [
        "# Generated by tools/gen_ninja.py. Do not edit by hand.",
        "ninja_required_version = 1.10",
        "builddir = build",
        "",
    ]

    write_var(lines, "cc", effective["cc"])
    write_var(lines, "as", effective["assembler"])
    write_var(lines, "ld", effective["ld"])
    write_var(lines, "objcopy", effective["objcopy"])
    write_var(lines, "python", effective["python"])
    write_var(lines, "gnu_efi_dir", slash(gnu_efi_dir))
    write_var(lines, "efi_crt0", "$gnu_efi_dir/crt0-efi-x86_64.o")
    write_var(lines, "efi_lds", "$gnu_efi_dir/elf_x86_64_efi.lds")
    write_var(lines, "efi_lib", "$gnu_efi_dir/libefi.a")
    write_var(lines, "gnuefi_lib", "$gnu_efi_dir/libgnuefi.a")
    lines.append("")

    lines.extend(
        [
            "rule configure",
            "  command = $python tools/gen_ninja.py --config build/ninja_config.json",
            "  description = GEN build.ninja",
            "  generator = 1",
            "",
            "rule cc",
            "  command = $python tools/ninja_cmd.py exec $out_dir -- $cc $cflags -MMD -MF $out.d -c $in -o $out",
            "  depfile = $out.d",
            "  deps = gcc",
            "  description = CC $in",
            "",
            "rule as",
            "  command = $python tools/ninja_cmd.py exec $out_dir -- $as -64 $in -o $out",
            "  description = AS $in",
            "",
            "rule link_kernel",
            "  command = $python tools/ninja_cmd.py exec $out_dir -- $ld -nostdlib -z max-page-size=0x1000 -T kernel/arch/x86_64/boot/linker.ld -o $out $in",
            "  description = LD $out",
            "",
            "rule link_efi",
            "  command = $python tools/ninja_cmd.py exec $out_dir -- $ld -nostdlib -znocombreloc -T $efi_lds -shared -Bsymbolic -L$gnu_efi_dir $efi_crt0 -o $out $in $gnuefi_lib $efi_lib",
            "  description = LD $out",
            "",
            "rule check_gnu_efi",
            "  command = $python tools/ninja_cmd.py check-gnu-efi $gnu_efi_dir $out",
            "  description = CHECK gnu-efi",
            "",
            "rule check_ovmf",
            "  command = $python tools/ninja_cmd.py check-ovmf $out",
            "  description = CHECK OVMF",
            "  pool = console",
            "",
            "rule efi_image",
            "  command = $python tools/ninja_cmd.py efi-image $out_dir $objcopy $in $out",
            "  description = EFI $out",
            "",
            "rule link_user",
            "  command = $python tools/ninja_cmd.py exec $out_dir -- $cc -T bin/linker.ld -no-pie -nostdlib -nostartfiles -nodefaultlibs -o $out $in",
            "  description = LD $out",
            "",
            "rule iso_image",
            "  command = $python tools/ninja_cmd.py iso $out",
            "  description = ISO $out",
            "",
            "rule run_qemu",
            "  command = $python tools/ninja_cmd.py run $out",
            "  description = RUN Kenux",
            "  pool = console",
            "",
            "rule clean_outputs",
            "  command = $python tools/ninja_cmd.py clean $out",
            "  description = CLEAN",
            "",
            "build build.ninja: configure tools/gen_ninja.py tools/ninja_cmd.py build/ninja_config.json",
            "build build/gnu-efi.stamp: check_gnu_efi | build.ninja",
            "",
        ]
    )

    kernel_elf = build_kernel(lines, kernel_cflags)
    bootloader_efi, simple_efi = build_bootloader(lines, boot_cflags)
    user_bins = build_user_programs(lines, user_cflags)

    iso = Path("kenux.iso")
    lines.append(
        f"build {ninja_escape(iso)}: iso_image {ninja_escape(bootloader_efi)} {ninja_escape(kernel_elf)}"
    )
    lines.append("")

    lines.append("build always: phony")
    lines.append("build build/ovmf.env: check_ovmf always | build.ninja")
    lines.append(
        f"build build/run.stamp: run_qemu {ninja_escape(bootloader_efi)} {ninja_escape(kernel_elf)} build/ovmf.env always"
    )
    lines.append("build build/clean.stamp: clean_outputs always")
    lines.append("")

    lines.append(f"build kernel: phony {ninja_escape(kernel_elf)}")
    lines.append(f"build bootloader: phony {ninja_escape(bootloader_efi)}")
    lines.append(f"build bootloader-simple: phony {ninja_escape(simple_efi)}")
    lines.append(f"build user-init: phony {ninja_escape(user_bins[0])}")
    lines.append(f"build user-shell: phony {ninja_escape(user_bins[1])}")
    lines.append("build user: phony user-init")
    lines.append(f"build iso: phony {ninja_escape(iso)}")
    lines.append("build ovmf: phony build/ovmf.env")
    lines.append("build run: phony build/run.stamp")
    lines.append("build clean: phony build/clean.stamp")
    lines.append("build all: phony bootloader kernel")
    lines.append("default all")
    lines.append("")

    return "\n".join(lines)


def load_config(path: Path) -> dict[str, str | None]:
    with path.open("r", encoding="utf-8") as handle:
        data = json.load(handle)
    if not isinstance(data, dict):
        raise ValueError(f"{path} does not contain a JSON object")
    return data


def apply_config(args: argparse.Namespace, config: dict[str, str | None]) -> None:
    for key in ["gnu_efi_dir", "cc", "assembler", "ld", "objcopy", "python", "output"]:
        value = config.get(key)
        if value is not None:
            setattr(args, key, str(value))


def write_config(args: argparse.Namespace, effective: dict[str, str]) -> None:
    config = {
        "gnu_efi_dir": args.gnu_efi_dir,
        "cc": effective["cc"],
        "assembler": effective["assembler"],
        "ld": effective["ld"],
        "objcopy": effective["objcopy"],
        "python": effective["python"],
        "output": args.output,
    }

    path = ROOT / CONFIG_FILE
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(config, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate build.ninja for Kenux.")
    parser.add_argument("--config", help="Read generator options from a JSON config file.")
    parser.add_argument("--gnu-efi-dir", help="Directory containing gnu-efi crt0, linker script, and libraries.")
    parser.add_argument("--cc", default="gcc")
    parser.add_argument("--assembler", default="as")
    parser.add_argument("--ld", default="ld")
    parser.add_argument("--objcopy", default="objcopy")
    parser.add_argument("--python", default="python3")
    parser.add_argument("-o", "--output", default="build.ninja")
    args = parser.parse_args()

    if args.config:
        apply_config(args, load_config(ROOT / args.config))

    effective = {
        "cc": os.environ.get("CC", args.cc),
        "assembler": os.environ.get("AS", args.assembler),
        "ld": os.environ.get("LD", args.ld),
        "objcopy": os.environ.get("OBJCOPY", args.objcopy),
        "python": args.python,
    }
    write_config(args, effective)

    output = ROOT / args.output
    output.write_text(generate(args), encoding="utf-8")
    print(f"generated {output.relative_to(ROOT).as_posix()}")


if __name__ == "__main__":
    main()
