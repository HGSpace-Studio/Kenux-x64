#!/bin/bash
# 创建 Kenux OS 可启动 ISO 镜像

echo "📀 创建 Kenux OS ISO 镜像"
echo "========================="

# 检查工具
if ! command -v xorriso &> /dev/null; then
    echo "❌ 错误: 未找到 xorriso"
    echo "请安装: sudo apt install xorriso"
    exit 1
fi

if ! command -v mcopy &> /dev/null; then
    echo "❌ 错误: 未找到 mtools (mcopy)"
    echo "请安装: sudo apt install mtools"
    exit 1
fi

# 清理旧文件
rm -rf isodir kenux-os.iso 2>/dev/null

# 创建 ISO 目录结构
mkdir -p isodir/EFI/BOOT
mkdir -p isodir/boot
mkdir -p isodir/apps

# 复制文件
echo "📋 复制系统文件..."

# UEFI Bootloader
cp esp/EFI/BOOT/BOOTX64.EFI isodir/EFI/BOOT/

# 内核
cp img/boot/kernel.elf.pe isodir/boot/KENUXK.BIN

# 应用程序
cp esp/apps/*.kex isodir/apps/ 2>/dev/null || true

# 创建启动配置 (如果需要)
cat > isodir/startup.nsh << 'EOF'
EFI\BOOT\BOOTX64.EFI
EOF

# 创建 ISO
echo "🔥 生成 ISO 镜像..."
xorriso \
    -as mkisofs \
    -R -J -V "KENUX_OS" \
    -o kenux-os.iso \
    -b boot/BOOTX64.EFI \
    -no-emul-boot \
    -append_partition 2 0xef isodir/EFI/BOOT/BOOTX64.EFI \
    -partition_offset 16 \
    --mbr-force-bootable \
    isodir

# 清理临时目录
rm -rf isodir

# 显示结果
if [ -f "kenux-os.iso" ]; then
    echo ""
    echo "✅ ISO 镜像创建成功！"
    ls -lh kenux-os.iso
    echo ""
    echo "🚀 使用方法:"
    echo "   1. QEMU 启动: qemu-system-x86_64 -cdrom kenux-os.iso -boot d"
    echo "   2. 写入 USB:  sudo dd if=kenux-os.iso of=/dev/sdX bs=4M status=progress"
    echo "   3. 虚拟机:     直接加载 kenux-os.iso 作为光驱"
else
    echo "❌ ISO 创建失败"
    exit 1
fi