#!/usr/bin/env python3
"""检查 ESP 镜像的 FAT32 结构"""
import struct

with open('esp.img', 'rb') as f:
    # MBR (扇区 0)
    mbr = f.read(512)
    print('=== MBR (扇区 0) ===')
    print('签名: %02x%02x' % (mbr[510], mbr[511]))
    part = mbr[446:462]
    print('分区1: 启动=%02x 类型=%02x 起始LBA=%d 扇区数=%d' % (
        part[0], part[4],
        struct.unpack_from('<I', part, 8)[0],
        struct.unpack_from('<I', part, 12)[0]))

    # FAT32 引导扇区 (扇区 2048)
    f.seek(2048 * 512)
    bs = f.read(512)
    print()
    print('=== FAT32 引导扇区 (扇区 2048) ===')
    print('跳转: %02x %02x %02x' % (bs[0], bs[1], bs[2]))
    print('OEM: %s' % bs[3:11])
    print('每扇区字节: %d' % struct.unpack_from('<H', bs, 11)[0])
    print('每簇扇区: %d' % bs[13])
    print('保留扇区: %d' % struct.unpack_from('<H', bs, 14)[0])
    print('FAT数: %d' % bs[16])
    print('根目录项: %d' % struct.unpack_from('<H', bs, 17)[0])
    print('总扇区16: %d' % struct.unpack_from('<H', bs, 19)[0])
    print('介质: %02x' % bs[21])
    print('每FAT扇区16: %d' % struct.unpack_from('<H', bs, 22)[0])
    print('每磁道扇区: %d' % struct.unpack_from('<H', bs, 24)[0])
    print('磁头数: %d' % struct.unpack_from('<H', bs, 26)[0])
    print('隐藏扇区: %d' % struct.unpack_from('<I', bs, 28)[0])
    print('总扇区32: %d' % struct.unpack_from('<I', bs, 32)[0])
    print('每FAT32扇区: %d' % struct.unpack_from('<I', bs, 36)[0])
    print('根目录簇: %d' % struct.unpack_from('<I', bs, 44)[0])
    print('FSINFO扇区: %d' % struct.unpack_from('<H', bs, 48)[0])
    print('备份扇区: %d' % struct.unpack_from('<H', bs, 50)[0])
    print('签名: %02x%02x' % (bs[510], bs[511]))
    print('FS类型: %s' % bs[82:90])

    # FAT 表
    fat_start_sector = 2048 + 32
    f.seek(fat_start_sector * 512)
    fat = f.read(32)
    print()
    print('=== FAT 表前8项 ===')
    for i in range(8):
        val = struct.unpack_from('<I', fat, i * 4)[0] & 0x0FFFFFFF
        print('FAT[%d] = %08X' % (i, val))

    # 根目录 (簇2)
    data_start_sector = 2048 + 32 + 2 * 130
    root_offset = data_start_sector * 512
    f.seek(root_offset)
    print()
    print('=== 根目录 ===')
    for i in range(20):
        entry = f.read(32)
        if entry[0] == 0:
            break
        if entry[0] == 0xE5:
            continue
        name = entry[0:8].decode('ascii', errors='replace').rstrip()
        ext = entry[8:11].decode('ascii', errors='replace').rstrip()
        attr = entry[11]
        cluster_hi = struct.unpack_from('<H', entry, 20)[0]
        cluster_lo = struct.unpack_from('<H', entry, 26)[0]
        cluster = (cluster_hi << 16) | cluster_lo
        size = struct.unpack_from('<I', entry, 28)[0]
        if attr == 0x0F:
            print('  LFN seq=%02x' % entry[0])
        else:
            fname = name + ('.' + ext if ext.strip() else '')
            print('  %s attr=%02x cluster=%d size=%d' % (fname, attr, cluster, size))

    # EFI/BOOT 目录
    print()
    print('=== EFI 目录 ===')
    # 先找到 EFI 目录的簇
    f.seek(root_offset)
    for i in range(20):
        entry = f.read(32)
        if entry[0] == 0:
            break
        if entry[11] == 0x0F or entry[0] == 0xE5:
            continue
        name = entry[0:8].decode('ascii', errors='replace').rstrip()
        if name.upper() == 'EFI':
            cluster_hi = struct.unpack_from('<H', entry, 20)[0]
            cluster_lo = struct.unpack_from('<H', entry, 26)[0]
            efi_cluster = (cluster_hi << 16) | cluster_lo
            efi_offset = (data_start_sector + (efi_cluster - 2) * 8) * 512
            f.seek(efi_offset)
            for j in range(10):
                e = f.read(32)
                if e[0] == 0:
                    break
                if e[11] == 0x0F or e[0] == 0xE5:
                    continue
                n = e[0:8].decode('ascii', errors='replace').rstrip()
                ex = e[8:11].decode('ascii', errors='replace').rstrip()
                attr = e[11]
                ch = struct.unpack_from('<H', e, 20)[0]
                cl = struct.unpack_from('<H', e, 26)[0]
                c = (ch << 16) | cl
                sz = struct.unpack_from('<I', e, 28)[0]
                fn = n + ('.' + ex if ex.strip() else '')
                print('  %s attr=%02x cluster=%d size=%d' % (fn, attr, c, sz))

            # 检查 BOOT 子目录
            f.seek(efi_offset)
            for j in range(10):
                e = f.read(32)
                if e[0] == 0:
                    break
                if e[11] == 0x0F or e[0] == 0xE5:
                    continue
                n = e[0:8].decode('ascii', errors='replace').rstrip()
                if n.upper() == 'BOOT':
                    cluster_hi = struct.unpack_from('<H', e, 20)[0]
                    cluster_lo = struct.unpack_from('<H', e, 26)[0]
                    boot_cluster = (cluster_hi << 16) | cluster_lo
                    boot_offset = (data_start_sector + (boot_cluster - 2) * 8) * 512
                    f.seek(boot_offset)
                    print()
                    print('=== EFI/BOOT 目录 ===')
                    for k in range(10):
                        e2 = f.read(32)
                        if e2[0] == 0:
                            break
                        if e2[11] == 0x0F or e2[0] == 0xE5:
                            continue
                        n2 = e2[0:8].decode('ascii', errors='replace').rstrip()
                        ex2 = e2[8:11].decode('ascii', errors='replace').rstrip()
                        attr2 = e2[11]
                        ch2 = struct.unpack_from('<H', e2, 20)[0]
                        cl2 = struct.unpack_from('<H', e2, 26)[0]
                        c2 = (ch2 << 16) | cl2
                        sz2 = struct.unpack_from('<I', e2, 28)[0]
                        fn2 = n2 + ('.' + ex2 if ex2.strip() else '')
                        print('  %s attr=%02x cluster=%d size=%d' % (fn2, attr2, c2, sz2))
            break
