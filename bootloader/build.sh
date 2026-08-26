#!/bin/bash
# KEGD UEFI bootloader build wrapper.
# 需要: gcc, ld, objcopy, gnu-efi, ninja

set -e

echo "=== KEGD UEFI Bootloader Builder ==="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

command -v gcc >/dev/null 2>&1 || { echo "gcc not found!"; exit 1; }
command -v ld >/dev/null 2>&1 || { echo "ld not found!"; exit 1; }
command -v objcopy >/dev/null 2>&1 || { echo "objcopy not found!"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 not found!"; exit 1; }
command -v ninja >/dev/null 2>&1 || { echo "ninja not found!"; exit 1; }

cd "$ROOT_DIR"
python3 tools/gen_ninja.py
ninja bootloader

echo ""
echo "=== Build Complete ==="
echo "Output: bootloader/BOOTX64.EFI"
echo ""
echo "To create bootable USB:"
echo "  1. Format USB as FAT32"
echo "  2. Copy BOOTX64.EFI to /EFI/BOOT/BOOTX64.EFI"
echo "  3. Copy kernel.elf to /boot/kernel.elf"
