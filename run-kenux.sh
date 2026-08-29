#!/bin/bash
# Kenux OS 完整系统启动脚本

echo "🚀 Kenux OS 完整系统启动器"
echo "=========================="

# 检查 QEMU
if ! command -v qemu-system-x86_64 &> /dev/null; then
    echo "❌ 错误: 未找到 qemu-system-x86_64"
    echo "请安装: sudo apt install qemu-system-x86"
    exit 1
fi

# 检查内核文件
if [ ! -f "img/boot/kernel.elf.pe" ]; then
    echo "❌ 错误: 内核文件不存在"
    echo "请先运行: python3 build.py run all"
    exit 1
fi

# 检查 ESP 目录
if [ ! -d "esp" ]; then
    echo "❌ 错误: ESP 目录不存在"
    exit 1
fi

# 创建临时磁盘镜像 (如果不存在)
if [ ! -f "kenux-system.img" ]; then
    echo "📦 创建系统磁盘镜像..."
    dd if=/dev/zero of=kenux-system.img bs=1M count=64 > /dev/null 2>&1
    mkfs.fat -F 32 kenux-system.img > /dev/null 2>&1
    
    # 使用 mtools 复制文件
    mmd -i kenux-system.img ::EFI ::EFI/BOOT ::apps 2>/dev/null || true
    mcopy -i kenux-system.img esp/EFI/BOOT/BOOTX64.EFI ::EFI/BOOT/ 2>/dev/null || true
    mcopy -i kenux-system.img esp/kernel.elf :: 2>/dev/null || true
    mcopy -i kenux-system.img esp/apps/* ::apps/ 2>/dev/null || true
fi

# 启动参数
QEMU_OPTS=(
    # 基本设置
    -m 512M                    # 内存大小
    -smp 2                     # CPU 核心数
    
    # 显示设置
    -display gtk               # GTK 窗口
    
    # UEFI 固件
    -bios /usr/share/OVMF/OVMF_CODE.fd  # UEFI BIOS (如果存在)
    
    # 磁盘设置
    -drive if=pflash,format=raw,unit=0,file=/usr/share/OVMF/OVMF_CODE.fd,readonly=on
    -drive if=pflash,format=raw,unit=1,file=/usr/share/OVMF/OVMF_VARS.fd
    -drive file=kenux-system.img,format=raw,index=0,media=disk
    
    # 网络设置
    -netdev user,id=net0,hostfwd=tcp::2222-:22
    -device virtio-net-pci,netdev=net0
    
    # 其他选项
    -boot menu=on
    -no-reboot
    -serial stdio
)

echo ""
echo "✅ 启动 Kenux OS..."
echo "   内存: 512MB"
echo "   CPU: 2 核"
echo "   磁盘: kenux-system.img (64MB)"
echo ""
echo "💡 提示:"
echo "   - 按 Ctrl+A 然后 X 退出 QEMU"
echo "   - 端口转发: localhost:2222 -> guest:22"
echo ""

# 启动 QEMU
qemu-system-x86_64 "${QEMU_OPTS[@]}" "$@"
