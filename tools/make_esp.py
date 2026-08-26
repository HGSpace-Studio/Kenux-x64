#!/usr/bin/env python3
"""
make_esp.py - 创建 FAT32 ESP (EFI System Partition) 镜像
用法: python tools/make_esp.py
输出: esp.img (包含 esp/ 目录下所有文件)
"""

import struct
import os
import sys
from pathlib import Path

# 镜像参数
IMAGE_SIZE = 64 * 1024 * 1024  # 64MB
SECTOR_SIZE = 512
CLUSTER_SIZE = 4096  # 8 扇区/簇

# MBR 分区表: 分区从扇区 2048 开始 (1MB 对齐)
PARTITION_START = 2048  # 扇区
RESERVED_SECTORS = 32   # FAT32 保留扇区 (在分区内)
NUM_FATS = 2
ROOT_ENTRIES = 0  # FAT32 根目录在簇中

# FAT32 类型
FAT32_EOC = 0x0FFFFFFF  # End of cluster chain
FAT32_EOC_FLAG = 0x0FFFFFF8

# 目录项属性
ATTR_READ_ONLY = 0x01
ATTR_HIDDEN = 0x02
ATTR_SYSTEM = 0x04
ATTR_VOLUME_ID = 0x08
ATTR_DIRECTORY = 0x10
ATTR_ARCHIVE = 0x20
ATTR_LONG_NAME = 0x0F


class Fat32Image:
    def __init__(self, size=IMAGE_SIZE):
        self.size = size
        self.sector_size = SECTOR_SIZE
        self.cluster_size = CLUSTER_SIZE
        self.bytes_per_cluster = CLUSTER_SIZE
        self.partition_start = PARTITION_START  # 分区起始扇区
        self.partition_size = (size // SECTOR_SIZE) - self.partition_start  # 分区扇区数
        self.total_sectors = self.partition_size  # FAT32 内部的总扇区数
        self.reserved_sectors = RESERVED_SECTORS
        self.num_fats = NUM_FATS

        # 计算每 FAT 扇区数
        data_sectors = self.total_sectors - self.reserved_sectors
        # 估算簇数
        sectors_per_cluster = CLUSTER_SIZE // SECTOR_SIZE
        approx_clusters = data_sectors // sectors_per_cluster
        # FAT32 每 FAT 需要 approx_clusters * 4 / 512 扇区
        self.fat_size = ((approx_clusters * 4 + SECTOR_SIZE - 1) // SECTOR_SIZE)
        # 加一些余量
        self.fat_size += 4

        self.fat_start = self.reserved_sectors
        self.data_start = self.reserved_sectors + self.num_fats * self.fat_size
        self.total_clusters = (self.total_sectors - self.data_start) // sectors_per_cluster

        # 限制簇数在 FAT32 范围内
        if self.total_clusters > 0x0FFFFFFF:
            self.total_clusters = 0x0FFFFFFF

        # 数据缓冲区
        self.data = bytearray(size)

        # FAT 表 (在内存中)
        self.fat = bytearray(self.fat_size * SECTOR_SIZE)

        # 下一可用簇
        self.next_free = 3  # 簇 0,1 保留, 簇 2 是根目录

        # 初始化 FAT
        self._init_fat()

    def _init_fat(self):
        """初始化 FAT 表"""
        # FAT[0] = 0x0FFFFFF8 (介质描述符)
        # FAT[1] = 0x0FFFFFFF (EOC)
        struct.pack_into('<I', self.fat, 0, 0x0FFFFFF8)
        struct.pack_into('<I', self.fat, 4, 0x0FFFFFFF)
        # 根目录占簇 2
        struct.pack_into('<I', self.fat, 8, FAT32_EOC)

    def _cluster_to_offset(self, cluster):
        """簇号转字节偏移 (包含分区起始偏移)"""
        sector = self.partition_start + self.data_start + (cluster - 2) * (self.cluster_size // SECTOR_SIZE)
        return sector * SECTOR_SIZE

    def _alloc_cluster(self):
        """分配一个新簇"""
        cluster = self.next_free
        self.next_free += 1
        struct.pack_into('<I', self.fat, cluster * 4, FAT32_EOC)
        return cluster

    def _write_fat_entry(self, cluster, value):
        """写 FAT 表项"""
        struct.pack_into('<I', self.fat, cluster * 4, value & 0x0FFFFFFF)

    def _read_fat_entry(self, cluster):
        """读 FAT 表项"""
        return struct.unpack_from('<I', self.fat, cluster * 4)[0] & 0x0FFFFFFF

    def _alloc_chain(self, count):
        """分配 count 个簇的链，返回首簇号"""
        if count == 0:
            return 0
        first = self._alloc_cluster()
        prev = first
        for _ in range(count - 1):
            curr = self._alloc_cluster()
            self._write_fat_entry(prev, curr)
            prev = curr
        self._write_fat_entry(prev, FAT32_EOC)
        return first

    def _write_cluster_data(self, cluster, data):
        """写入簇数据"""
        offset = self._cluster_to_offset(cluster)
        self.data[offset:offset + len(data)] = data

    def _write_chain_data(self, first_cluster, data):
        """将数据写入簇链"""
        cluster = first_cluster
        offset = 0
        while cluster < FAT32_EOC_FLAG and offset < len(data):
            chunk = data[offset:offset + self.bytes_per_cluster]
            self._write_cluster_data(cluster, chunk)
            offset += self.bytes_per_cluster
            cluster = self._read_fat_entry(cluster)

    def _make_short_name(self, name):
        """生成 8.3 短文件名"""
        name = name.upper()
        if '.' in name:
            base, ext = name.rsplit('.', 1)
        else:
            base, ext = name, ''

        # 移除无效字符
        valid = ''
        for c in base:
            if c.isalnum() or c in '!#$%&\'()-@^_`{}~':
                valid += c
        base = valid[:8]

        ext = ext[:3]
        # 填充
        base = base.ljust(8)
        ext = ext.ljust(3)
        return base, ext

    def _make_dir_entry(self, name, attr, cluster, size):
        """创建目录项 (32字节)"""
        base, ext = self._make_short_name(name)
        entry = bytearray(32)

        # 文件名 (8字节) + 扩展名 (3字节)
        entry[0:8] = base.encode('ascii')
        entry[8:11] = ext.encode('ascii')

        # 属性
        entry[11] = attr

        # 保留
        entry[12] = 0
        entry[13] = 0  # 创建时间(毫秒)
        # 创建时间
        entry[14] = 0  # 创建时间
        entry[15] = 0
        # 创建日期
        entry[16] = 0x0021  # 2020-01-01
        entry[17] = 0x004A
        # 最后访问日期
        entry[18] = 0x21
        entry[19] = 0x4A
        # 高16位起始簇号
        entry[20] = (cluster >> 16) & 0xFF
        entry[21] = (cluster >> 24) & 0xFF
        # 最后修改时间
        entry[22] = 0
        entry[23] = 0
        # 最后修改日期
        entry[24] = 0x21
        entry[25] = 0x4A
        # 低16位起始簇号
        entry[26] = cluster & 0xFF
        entry[27] = (cluster >> 8) & 0xFF
        # 文件大小
        struct.pack_into('<I', entry, 28, size)

        return entry

    def _make_long_name_entries(self, long_name, short_name_entry):
        """创建长文件名目录项"""
        # UTF-16 编码
        name_utf16 = long_name.encode('utf-16-le')
        # 填充到 13 个 UTF-16 字符 (26字节)
        padded = name_utf16 + b'\x00' * (26 - len(name_utf16))
        # 如果不足 26 字节，在末尾加 0x00 和 0xFF 填充
        if len(name_utf16) < 26:
            padded = name_utf16 + b'\x00\x00'
            if len(padded) < 26:
                padded += b'\xFF' * (26 - len(padded))

        # 计算需要的 LFN 项数
        num_chars = len(long_name)
        num_lfn = (num_chars + 12) // 13  # 每项 13 字符

        entries = []
        for i in range(num_lfn - 1, -1, -1):
            entry = bytearray(32)
            seq = i + 1
            if i == num_lfn - 1:
                seq |= 0x40  # 最后一项标志

            entry[0] = seq
            entry[11] = ATTR_LONG_NAME  # 属性

            # 校验和
            checksum = 0
            for c in short_name_entry[0:11]:
                checksum = ((checksum & 1) << 7) + (checksum >> 1) + c
                checksum &= 0xFF
            entry[12] = checksum

            # 类型
            entry[13] = 0

            # 高16位起始簇号 (LFN 中必须为0)
            entry[14] = 0
            entry[15] = 0
            entry[20] = 0
            entry[21] = 0
            entry[24] = 0
            entry[25] = 0

            # 名称字符 (13个UTF-16字符, 分3段)
            char_start = i * 13 * 2
            # 第一段: 5个字符 (偏移 1-10)
            for j in range(5):
                pos = char_start + j * 2
                entry[1 + j * 2] = padded[pos] if pos < len(padded) else 0xFF
                entry[2 + j * 2] = padded[pos + 1] if pos + 1 < len(padded) else 0xFF
            # 第二段: 6个字符 (偏移 14-25)
            for j in range(6):
                pos = char_start + 10 + j * 2
                entry[14 + j * 2] = padded[pos] if pos < len(padded) else 0xFF
                entry[15 + j * 2] = padded[pos + 1] if pos + 1 < len(padded) else 0xFF
            # 第三段: 2个字符 (偏移 28-31)
            for j in range(2):
                pos = char_start + 22 + j * 2
                entry[28 + j * 2] = padded[pos] if pos < len(padded) else 0xFF
                entry[29 + j * 2] = padded[pos + 1] if pos + 1 < len(padded) else 0xFF

            entries.append(bytes(entry))

        return entries

    def _create_subdir(self, parent_cluster, name):
        """创建子目录，返回首簇号"""
        # 分配一个簇
        cluster = self._alloc_cluster()

        # 创建 . 和 .. 目录项
        dot_entry = self._make_dir_entry('.', ATTR_DIRECTORY, cluster, 0)
        dotdot_entry = self._make_dir_entry('..', ATTR_DIRECTORY, parent_cluster, 0)

        # 写入子目录簇
        offset = self._cluster_to_offset(cluster)
        self.data[offset:offset + 32] = dot_entry
        self.data[offset + 32:offset + 64] = dotdot_entry

        return cluster

    def _add_file_to_dir(self, dir_cluster, name, file_cluster, file_size):
        """向目录添加文件项"""
        # 读取目录簇内容
        dir_offset = self._cluster_to_offset(dir_cluster)
        dir_data = self.data[dir_offset:dir_offset + self.bytes_per_cluster]

        # 生成短文件名目录项
        short_entry = self._make_dir_entry(name, ATTR_ARCHIVE, file_cluster, file_size)

        # 检查是否需要长文件名
        needs_lfn = False
        upper_name = name.upper()
        base, ext = self._make_short_name(name)
        short_name = base + ext
        orig_name = upper_name.replace('.', '')

        # 如果短文件名与原名不同，需要 LFN
        if short_name.rstrip() != upper_name and len(name) > 0:
            needs_lfn = True

        # 查找空闲位置
        entries_needed = (2 if needs_lfn else 0) + 1  # LFN项数 + 1个短名项
        if needs_lfn:
            lfn_entries = self._make_long_name_entries(name, short_entry)
            entries_needed = len(lfn_entries) + 1

        # 在目录簇中查找连续空闲项
        free_start = -1
        free_count = 0
        for i in range(0, self.bytes_per_cluster, 32):
            if dir_data[i] == 0x00 or dir_data[i] == 0xE5:
                if free_start == -1:
                    free_start = i
                    free_count = 1
                else:
                    free_count += 1
                if free_count >= entries_needed:
                    break
            else:
                free_start = -1
                free_count = 0

        if free_start == -1 or free_count < entries_needed:
            print(f"错误: 目录簇空间不足")
            return False

        # 写入目录项
        write_offset = dir_offset + free_start
        if needs_lfn:
            for lfn_entry in lfn_entries:
                self.data[write_offset:write_offset + 32] = lfn_entry
                write_offset += 32

        self.data[write_offset:write_offset + 32] = short_entry
        return True

    def _add_dir_to_dir(self, dir_cluster, name, subdir_cluster):
        """向目录添加子目录项"""
        dir_offset = self._cluster_to_offset(dir_cluster)
        dir_data = self.data[dir_offset:dir_offset + self.bytes_per_cluster]

        short_entry = self._make_dir_entry(name, ATTR_DIRECTORY, subdir_cluster, 0)

        needs_lfn = False
        base, ext = self._make_short_name(name)
        short_name = base + ext
        upper_name = name.upper().replace('.', '')
        if short_name.rstrip() != upper_name:
            needs_lfn = True

        entries_needed = 1
        lfn_entries = []
        if needs_lfn:
            lfn_entries = self._make_long_name_entries(name, short_entry)
            entries_needed = len(lfn_entries) + 1

        free_start = -1
        free_count = 0
        for i in range(0, self.bytes_per_cluster, 32):
            if dir_data[i] == 0x00 or dir_data[i] == 0xE5:
                if free_start == -1:
                    free_start = i
                    free_count = 1
                else:
                    free_count += 1
                if free_count >= entries_needed:
                    break
            else:
                free_start = -1
                free_count = 0

        if free_start == -1 or free_count < entries_needed:
            print(f"错误: 目录簇空间不足")
            return False

        write_offset = dir_offset + free_start
        if needs_lfn:
            for lfn_entry in lfn_entries:
                self.data[write_offset:write_offset + 32] = lfn_entry
                write_offset += 32

        self.data[write_offset:write_offset + 32] = short_entry
        return True

    def add_file(self, name, file_data):
        """添加文件到根目录"""
        num_clusters = (len(file_data) + self.bytes_per_cluster - 1) // self.bytes_per_cluster
        first_cluster = self._alloc_chain(num_clusters)

        # 写入文件数据
        if num_clusters > 0:
            self._write_chain_data(first_cluster, file_data)

        # 添加目录项到根目录 (簇2)
        return self._add_file_to_dir(2, name, first_cluster, len(file_data))

    def add_directory(self, path):
        """创建目录路径 (如 EFI/BOOT)，返回最后一级目录的簇号"""
        parts = path.strip('/').split('/')
        current_cluster = 2  # 根目录

        for part in parts:
            # 检查目录是否已存在
            existing = self._find_in_dir(current_cluster, part)
            if existing is not None:
                current_cluster = existing
            else:
                # 创建子目录
                new_cluster = self._create_subdir(current_cluster, part)
                self._add_dir_to_dir(current_cluster, part, new_cluster)
                current_cluster = new_cluster

        return current_cluster

    def add_file_to_dir(self, dir_path, name, file_data):
        """添加文件到指定目录"""
        dir_cluster = self.add_directory(dir_path)
        num_clusters = (len(file_data) + self.bytes_per_cluster - 1) // self.bytes_per_cluster
        first_cluster = self._alloc_chain(num_clusters)

        if num_clusters > 0:
            self._write_chain_data(first_cluster, file_data)

        return self._add_file_to_dir(dir_cluster, name, first_cluster, len(file_data))

    def _find_in_dir(self, dir_cluster, name):
        """在目录中查找文件/目录，返回簇号或 None"""
        dir_offset = self._cluster_to_offset(dir_cluster)

        for i in range(0, self.bytes_per_cluster, 32):
            entry = self.data[dir_offset + i:dir_offset + i + 32]
            if entry[0] == 0x00:
                break  # 目录结束
            if entry[0] == 0xE5:
                continue  # 已删除
            if entry[11] == ATTR_LONG_NAME:
                continue  # 跳过 LFN

            # 短文件名
            base = entry[0:8].decode('ascii', errors='ignore').rstrip()
            ext = entry[8:11].decode('ascii', errors='ignore').rstrip()
            if ext:
                short_name = f"{base}.{ext}"
            else:
                short_name = base

            if short_name.upper() == name.upper():
                cluster_lo = struct.unpack_from('<H', entry, 26)[0]
                cluster_hi = struct.unpack_from('<H', entry, 20)[0]
                return (cluster_hi << 16) | cluster_lo

        return None

    def _make_boot_sector(self):
        """创建 FAT32 引导扇区"""
        bs = bytearray(SECTOR_SIZE)

        # 跳转指令
        bs[0] = 0xEB
        bs[1] = 0x58
        bs[2] = 0x90

        # OEM 名称
        bs[3:11] = b'MSDOS5.0'

        # BPB (BIOS Parameter Block)
        struct.pack_into('<H', bs, 11, self.sector_size)  # 每扇区字节数
        bs[13] = self.cluster_size // self.sector_size  # 每簇扇区数
        struct.pack_into('<H', bs, 14, self.reserved_sectors)  # 保留扇区数
        bs[16] = self.num_fats  # FAT 表数
        struct.pack_into('<H', bs, 17, 0)  # 根目录项数 (FAT32=0)
        struct.pack_into('<H', bs, 19, 0)  # 总扇区数 (16位, FAT32=0)
        bs[21] = 0xF8  # 介质描述符 (硬盘)
        struct.pack_into('<H', bs, 22, 0)  # 每FAT扇区数 (FAT32=0, 用偏移36)
        struct.pack_into('<H', bs, 24, 63)  # 每磁道扇区数
        struct.pack_into('<H', bs, 26, 255)  # 磁头数
        struct.pack_into('<I', bs, 28, self.partition_start)  # 隐藏扇区数 = 分区起始LBA
        struct.pack_into('<I', bs, 32, self.total_sectors)  # 总扇区数 (32位)

        # FAT32 扩展 BPB
        struct.pack_into('<I', bs, 36, self.fat_size)  # 每 FAT 扇区数
        struct.pack_into('<H', bs, 40, 0)  # 扩展标志
        struct.pack_into('<H', bs, 42, 0)  # 文件系统版本
        struct.pack_into('<I', bs, 44, 2)  # 根目录首簇号
        struct.pack_into('<H', bs, 48, 1)  # 文件系统信息扇区号
        struct.pack_into('<H', bs, 50, 6)  # 备份引导扇区号
        # 保留 (12字节)
        bs[52:64] = b'\x00' * 12
        bs[64] = 0x80  # 驱动器号
        bs[65] = 0  # 保留
        bs[66] = 0x29  # 扩展引导签名
        struct.pack_into('<I', bs, 67, 0x12345678)  # 卷序列号
        bs[71:82] = b'NO NAME    '  # 卷标
        bs[82:90] = b'FAT32   '  # 文件系统类型

        # 引导代码 (简单)
        bs[90:510] = b'\x00' * (510 - 90)

        # 引导扇区签名
        struct.pack_into('<H', bs, 510, 0xAA55)

        return bs

    def _make_fsinfo(self):
        """创建 FSINFO 扇区"""
        fsi = bytearray(SECTOR_SIZE)

        struct.pack_into('<I', fsi, 0, 0x41615252)  # 前导签名
        struct.pack_into('<I', fsi, 484, 0x61417272)  # 结构签名
        struct.pack_into('<I', fsi, 488, 0xFFFFFFFF)  # 空闲簇数 (未知)
        struct.pack_into('<I', fsi, 492, 0xFFFFFFFF)  # 下一空闲簇
        struct.pack_into('<I', fsi, 508, 0xAA550000)  # 尾签名

        return fsi

    def _make_mbr(self):
        """创建 MBR 分区表 (包含一个 ESP 分区)"""
        mbr = bytearray(SECTOR_SIZE)

        # 引导代码 (前446字节留空)
        # 分区表 (偏移 446, 4个分区项 x 16字节)

        # 分区 1: EFI System Partition
        # 类型 0xEF = EFI System Partition
        part_offset = 446
        mbr[part_offset + 0] = 0x00  # 启动标志 (不可启动)
        # CHS 起始 (24位, 3字节) - 使用 0x000000 表示 LBA 0
        mbr[part_offset + 1] = 0x00
        mbr[part_offset + 2] = 0x02
        mbr[part_offset + 3] = 0x00
        mbr[part_offset + 4] = 0xEF  # 分区类型: EFI System Partition
        # CHS 结束 (24位, 3字节)
        mbr[part_offset + 5] = 0xFE
        mbr[part_offset + 6] = 0xFF
        mbr[part_offset + 7] = 0xFF
        # LBA 起始 (32位)
        struct.pack_into('<I', mbr, part_offset + 8, self.partition_start)
        # LBA 扇区数 (32位)
        struct.pack_into('<I', mbr, part_offset + 12, self.partition_size)

        # MBR 签名
        struct.pack_into('<H', mbr, 510, 0xAA55)

        return mbr

    def build(self, output_path):
        """构建完整镜像"""
        # 0. MBR 分区表 (扇区 0)
        mbr = self._make_mbr()
        self.data[0:SECTOR_SIZE] = mbr

        # 1. 引导扇区 (分区起始扇区)
        boot_sector = self._make_boot_sector()
        bs_offset = self.partition_start * SECTOR_SIZE
        self.data[bs_offset:bs_offset + SECTOR_SIZE] = boot_sector

        # 2. FSINFO (分区起始+1 扇区)
        fsinfo = self._make_fsinfo()
        self.data[bs_offset + SECTOR_SIZE:bs_offset + SECTOR_SIZE * 2] = fsinfo

        # 3. 备份引导扇区 (分区起始+6 扇区)
        self.data[bs_offset + SECTOR_SIZE * 6:bs_offset + SECTOR_SIZE * 7] = boot_sector
        self.data[bs_offset + SECTOR_SIZE * 7:bs_offset + SECTOR_SIZE * 8] = fsinfo

        # 4. 写入 FAT 表 (相对于分区起始)
        for i in range(self.num_fats):
            fat_sector = self.partition_start + self.fat_start + i * self.fat_size
            fat_offset = fat_sector * SECTOR_SIZE
            self.data[fat_offset:fat_offset + len(self.fat)] = self.fat

        # 写入文件
        with open(output_path, 'wb') as f:
            f.write(self.data)

        print(f"ESP 镜像已创建: {output_path} ({self.size // (1024*1024)}MB)")
        print(f"  分区起始: 扇区 {self.partition_start} (偏移 {self.partition_start * SECTOR_SIZE})")
        print(f"  分区大小: {self.partition_size} 扇区 ({self.partition_size * SECTOR_SIZE // (1024*1024)}MB)")
        print(f"  FAT 大小: {self.fat_size} 扇区/FAT")


def main():
    project_root = Path(__file__).parent.parent
    esp_dir = project_root / "esp"
    output = project_root / "esp.img"

    if not esp_dir.exists():
        print(f"错误: ESP 目录不存在: {esp_dir}")
        sys.exit(1)

    img = Fat32Image()

    # 递归添加 esp 目录下所有文件
    for root, dirs, files in os.walk(esp_dir):
        for fname in files:
            fpath = Path(root) / fname
            rel_path = fpath.relative_to(esp_dir)
            file_data = fpath.read_bytes()

            # 确定目标路径
            parent = rel_path.parent
            if str(parent) == '.':
                # 根目录文件
                print(f"  添加: {fname} ({len(file_data)} bytes)")
                img.add_file(fname, file_data)
            else:
                # 子目录文件
                dir_path = str(parent).replace('\\', '/')
                print(f"  添加: {dir_path}/{fname} ({len(file_data)} bytes)")
                img.add_file_to_dir(dir_path, fname, file_data)

    img.build(str(output))
    print("完成!")


if __name__ == '__main__':
    main()
