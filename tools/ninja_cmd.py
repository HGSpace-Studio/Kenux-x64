#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shlex
import shutil
import struct
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str], env: dict[str, str] | None = None) -> int:
    return subprocess.run(command, cwd=ROOT, env=env).returncode


def ensure_dir(path: str) -> None:
    if path and path != ".":
        (ROOT / path).mkdir(parents=True, exist_ok=True)


def cmd_exec(args: argparse.Namespace) -> int:
    ensure_dir(args.out_dir)
    return run(args.command)


def cmd_check_gnu_efi(args: argparse.Namespace) -> int:
    base = ROOT / args.gnu_efi_dir if not Path(args.gnu_efi_dir).is_absolute() else Path(args.gnu_efi_dir)
    missing = [
        item
        for item in [
            "crt0-efi-x86_64.o",
            "elf_x86_64_efi.lds",
            "libefi.a",
            "libgnuefi.a",
        ]
        if not (base / item).exists()
    ]

    if missing:
        print(f"gnu-efi files not found in {base}:", file=sys.stderr)
        for item in missing:
            print(f"  missing {item}", file=sys.stderr)
        print("Install gnu-efi or regenerate with --gnu-efi-dir /path/to/gnu-efi/lib.", file=sys.stderr)
        return 1

    stamp = ROOT / args.stamp
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.touch()
    return 0


def wsl_repo_path() -> str | None:
    result = subprocess.run(
        ["bash", "-lc", f"wslpath -a {shlex.quote(str(ROOT))}"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    return result.stdout.strip()


def delegate_check_ovmf_to_wsl(output: str) -> int:
    repo = wsl_repo_path()
    if not repo:
        print("OVMF auto-download needs Linux/WSL when running from Windows.", file=sys.stderr)
        return 1
    command = (
        f"cd {shlex.quote(repo)} && "
        f"python3 tools/ninja_cmd.py check-ovmf {shlex.quote(output)}"
    )
    return subprocess.run(["bash", "-lc", command], cwd=ROOT).returncode


def ovmf_candidates() -> list[tuple[Path, Path]]:
    return [
        (Path("/usr/share/OVMF/OVMF_CODE.fd"), Path("/usr/share/OVMF/OVMF_VARS.fd")),
        (Path("/usr/share/OVMF/OVMF_CODE_4M.fd"), Path("/usr/share/OVMF/OVMF_VARS_4M.fd")),
        (Path("/usr/share/edk2/ovmf/OVMF_CODE.fd"), Path("/usr/share/edk2/ovmf/OVMF_VARS.fd")),
        (Path("/usr/share/edk2-ovmf/x64/OVMF_CODE.fd"), Path("/usr/share/edk2-ovmf/x64/OVMF_VARS.fd")),
        (Path("/usr/share/qemu/OVMF_CODE.fd"), Path("/usr/share/qemu/OVMF_VARS.fd")),
    ]


def find_ovmf_pair() -> tuple[Path, Path] | None:
    env_code = os.environ.get("OVMF_CODE")
    env_vars = os.environ.get("OVMF_VARS_TEMPLATE")
    if env_code and env_vars and Path(env_code).exists() and Path(env_vars).exists():
        return Path(env_code), Path(env_vars)

    for code, vars_template in ovmf_candidates():
        if code.exists() and vars_template.exists():
            return code, vars_template

    roots = [
        Path("/usr/share/OVMF"),
        Path("/usr/share/edk2"),
        Path("/usr/share/edk2-ovmf"),
        Path("/usr/share/qemu"),
    ]
    for root in roots:
        if not root.exists():
            continue
        for code in sorted(root.rglob("OVMF_CODE*.fd")):
            name = code.name
            if "secboot" in name.lower():
                continue
            vars_template = code.with_name(name.replace("CODE", "VARS", 1))
            if vars_template.exists():
                return code, vars_template

    return None


def sudo_prefix() -> list[str] | None:
    if hasattr(os, "geteuid") and os.geteuid() == 0:
        return []
    sudo = shutil.which("sudo")
    if sudo:
        return [sudo]
    return None


def install_ovmf() -> int:
    if os.environ.get("KENUX_OVMF_AUTO_INSTALL", "1").lower() in {"0", "false", "no"}:
        print("OVMF auto-install is disabled by KENUX_OVMF_AUTO_INSTALL.", file=sys.stderr)
        return 1

    prefix = sudo_prefix()
    if prefix is None:
        print("OVMF is missing and sudo is not available for package installation.", file=sys.stderr)
        return 1

    installers = [
        ("apt-get", [prefix + ["apt-get", "update"], prefix + ["apt-get", "install", "-y", "ovmf"]]),
        ("dnf", [prefix + ["dnf", "install", "-y", "edk2-ovmf"]]),
        ("pacman", [prefix + ["pacman", "-Sy", "--needed", "--noconfirm", "edk2-ovmf"]]),
        ("zypper", [prefix + ["zypper", "--non-interactive", "install", "ovmf"]]),
        ("apk", [prefix + ["apk", "add", "ovmf"]]),
    ]

    for executable, commands in installers:
        if not shutil.which(executable):
            continue
        print(f"OVMF not found; installing with {executable}...")
        for command in commands:
            status = run(command)
            if status != 0:
                return status
        return 0

    print("OVMF is missing and no supported package manager was found.", file=sys.stderr)
    return 1


def install_run_tools() -> int:
    missing_commands = [
        command
        for command in ["sgdisk", "mformat", "mmd", "mcopy", "mdir"]
        if not shutil.which(command)
    ]
    if not missing_commands:
        return 0

    if os.environ.get("KENUX_RUN_TOOLS_AUTO_INSTALL", "1").lower() in {"0", "false", "no"}:
        print(
            "Run-tool auto-install is disabled by KENUX_RUN_TOOLS_AUTO_INSTALL.",
            file=sys.stderr,
        )
        return 1

    prefix = sudo_prefix()
    if prefix is None:
        print(
            f"Missing run tools ({', '.join(missing_commands)}) and sudo is not available.",
            file=sys.stderr,
        )
        return 1

    installers = [
        ("apt-get", [prefix + ["apt-get", "update"], prefix + ["apt-get", "install", "-y", "mtools", "gdisk"]]),
        ("dnf", [prefix + ["dnf", "install", "-y", "mtools", "gdisk"]]),
        ("pacman", [prefix + ["pacman", "-Sy", "--needed", "--noconfirm", "mtools", "gptfdisk"]]),
        ("zypper", [prefix + ["zypper", "--non-interactive", "install", "mtools", "gptfdisk"]]),
        ("apk", [prefix + ["apk", "add", "mtools", "gptfdisk"]]),
    ]

    for executable, commands in installers:
        if not shutil.which(executable):
            continue
        print(f"Missing run tools ({', '.join(missing_commands)}); installing with {executable}...")
        for command in commands:
            status = run(command)
            if status != 0:
                return status
        still_missing = [command for command in missing_commands if not shutil.which(command)]
        if still_missing:
            print(f"Run tools are still missing: {', '.join(still_missing)}", file=sys.stderr)
            return 1
        return 0

    print(
        f"Missing run tools ({', '.join(missing_commands)}) and no supported package manager was found.",
        file=sys.stderr,
    )
    return 1


def write_ovmf_env(output: Path, code: Path, vars_template: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        f"OVMF_CODE={code}\n"
        f"OVMF_VARS_TEMPLATE={vars_template}\n",
        encoding="utf-8",
    )


def cmd_check_ovmf(args: argparse.Namespace) -> int:
    if os.name == "nt" and shutil.which("bash"):
        return delegate_check_ovmf_to_wsl(args.output)

    pair = find_ovmf_pair()
    if pair is None:
        status = install_ovmf()
        if status != 0:
            return status
        pair = find_ovmf_pair()

    if pair is None:
        print("OVMF install completed, but OVMF_CODE/OVMF_VARS files were not found.", file=sys.stderr)
        return 1

    status = install_run_tools()
    if status != 0:
        return status

    code, vars_template = pair
    write_ovmf_env(ROOT / args.output, code, vars_template)
    print(f"OVMF_CODE={code}")
    print(f"OVMF_VARS_TEMPLATE={vars_template}")
    return 0


def cmd_efi_image(args: argparse.Namespace) -> int:
    ensure_dir(args.out_dir)
    sections = [
        ".text",
        ".sdata",
        ".data",
        ".dynamic",
        ".dynsym",
        ".rel",
        ".rela",
        ".reloc",
    ]
    command = [args.objcopy]
    for section in sections:
        command.extend(["-j", section])
    command.extend(["-O", "efi-app-x86_64", args.source, args.output])

    status = run(command)
    if status != 0:
        return status

    return validate_efi_application(ROOT / args.output)


def validate_efi_application(path: Path) -> int:
    try:
        data = path.read_bytes()
    except OSError as exc:
        print(f"failed to read EFI image {path}: {exc}", file=sys.stderr)
        return 1

    def fail(message: str) -> int:
        print(f"{path.relative_to(ROOT)} is not a valid x86_64 UEFI application: {message}", file=sys.stderr)
        return 1

    if len(data) < 0x40 or data[:2] != b"MZ":
        return fail("missing MZ DOS header")

    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    coff_offset = pe_offset + 4
    optional_offset = coff_offset + 20
    if pe_offset + 4 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        return fail("missing PE signature")
    if optional_offset + 0x48 > len(data):
        return fail("truncated PE optional header")

    machine, section_count, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", data, coff_offset)
    if machine != 0x8664:
        return fail(f"unexpected COFF machine 0x{machine:04x}")
    if section_count == 0:
        return fail("no sections")
    if optional_size < 0x70:
        return fail(f"optional header too small ({optional_size} bytes)")

    magic = struct.unpack_from("<H", data, optional_offset)[0]
    section_alignment = struct.unpack_from("<I", data, optional_offset + 0x20)[0]
    file_alignment = struct.unpack_from("<I", data, optional_offset + 0x24)[0]
    size_of_image = struct.unpack_from("<I", data, optional_offset + 0x38)[0]
    size_of_headers = struct.unpack_from("<I", data, optional_offset + 0x3C)[0]
    subsystem = struct.unpack_from("<H", data, optional_offset + 0x44)[0]

    if magic != 0x20B:
        return fail(f"optional header is not PE32+ (magic 0x{magic:04x})")
    if subsystem != 0x0A:
        return fail(f"subsystem is not EFI_APPLICATION (0x{subsystem:04x})")
    if section_alignment == 0:
        return fail("SectionAlignment is zero")
    if file_alignment == 0:
        return fail("FileAlignment is zero")
    if size_of_image == 0:
        return fail("SizeOfImage is zero")
    if size_of_headers == 0:
        return fail("SizeOfHeaders is zero")

    return 0


def cmd_iso(args: argparse.Namespace) -> int:
    tree = ROOT / "build" / "iso-tree"
    if tree.exists():
        shutil.rmtree(tree)

    efi_dir = tree / "EFI" / "BOOT"
    boot_dir = tree / "boot"
    efi_dir.mkdir(parents=True, exist_ok=True)
    boot_dir.mkdir(parents=True, exist_ok=True)

    shutil.copy2(ROOT / "bootloader" / "BOOTX64.EFI", efi_dir / "BOOTX64.EFI")
    shutil.copy2(ROOT / "iso" / "boot" / "kernel.elf", boot_dir / "kernel.elf")

    return run(
        [
            "xorriso",
            "-as",
            "mkisofs",
            "-R",
            "-J",
            "-V",
            "KENUX_OS",
            "-o",
            args.output,
            "-isohybrid-gpt-basdat",
            str(tree.relative_to(ROOT)),
        ]
    )


def remove(path: Path) -> None:
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def cmd_clean(args: argparse.Namespace) -> int:
    for path in [
        ROOT / "build" / "obj",
        ROOT / "build" / "iso-tree",
        ROOT / "build" / "esp",
        ROOT / "build" / "kenux-esp.img",
        ROOT / "build" / "test-esp.img",
        ROOT / "build" / "ovmf.env",
        ROOT / "iso" / "boot" / "kernel.elf",
        ROOT / "bootloader" / "BOOTX64.EFI",
        ROOT / "bootloader" / "BOOTX64.EFI.so",
        ROOT / "bootloader" / "BOOTX64-simple.EFI",
        ROOT / "bootloader" / "BOOTX64-simple.EFI.so",
        ROOT / "bin" / "init",
        ROOT / "bin" / "shell",
        ROOT / "kenux.iso",
        ROOT / ".ninja_log",
        ROOT / ".ninja_deps",
    ]:
        remove(path)

    for folder in [ROOT / "kernel", ROOT / "bootloader", ROOT / "bin"]:
        for obj in folder.rglob("*.o"):
            obj.unlink()

    stamp = ROOT / args.stamp
    stamp.parent.mkdir(parents=True, exist_ok=True)
    stamp.touch()
    return 0


def cmd_run(args: argparse.Namespace) -> int:
    env = os.environ.copy()
    env["KENUX_SKIP_BUILD"] = "1"
    ovmf_env = ROOT / "build" / "ovmf.env"
    if ovmf_env.exists():
        for line in ovmf_env.read_text(encoding="utf-8").splitlines():
            if "=" not in line:
                continue
            key, value = line.split("=", 1)
            env[key] = value
    if os.name == "nt":
        command = ["bash", "./start_kenux.sh"]
    else:
        command = ["./start_kenux.sh"]

    status = run(command, env=env)
    if status == 0:
        stamp = ROOT / args.stamp
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.touch()
    return status


def main() -> int:
    parser = argparse.ArgumentParser(description="Portable commands used by build.ninja.")
    subparsers = parser.add_subparsers(dest="command_name", required=True)

    exec_parser = subparsers.add_parser("exec")
    exec_parser.add_argument("out_dir")
    exec_parser.add_argument("command", nargs=argparse.REMAINDER)
    exec_parser.set_defaults(func=cmd_exec)

    gnu_efi_parser = subparsers.add_parser("check-gnu-efi")
    gnu_efi_parser.add_argument("gnu_efi_dir")
    gnu_efi_parser.add_argument("stamp")
    gnu_efi_parser.set_defaults(func=cmd_check_gnu_efi)

    ovmf_parser = subparsers.add_parser("check-ovmf")
    ovmf_parser.add_argument("output")
    ovmf_parser.set_defaults(func=cmd_check_ovmf)

    efi_parser = subparsers.add_parser("efi-image")
    efi_parser.add_argument("out_dir")
    efi_parser.add_argument("objcopy")
    efi_parser.add_argument("source")
    efi_parser.add_argument("output")
    efi_parser.set_defaults(func=cmd_efi_image)

    iso_parser = subparsers.add_parser("iso")
    iso_parser.add_argument("output")
    iso_parser.set_defaults(func=cmd_iso)

    clean_parser = subparsers.add_parser("clean")
    clean_parser.add_argument("stamp")
    clean_parser.set_defaults(func=cmd_clean)

    run_parser = subparsers.add_parser("run")
    run_parser.add_argument("stamp")
    run_parser.set_defaults(func=cmd_run)

    args = parser.parse_args()
    if getattr(args, "command", None) and args.command[0] == "--":
        args.command = args.command[1:]
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
