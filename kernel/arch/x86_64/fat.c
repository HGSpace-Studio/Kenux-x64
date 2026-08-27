#include <arch/fat.h>
#include <arch/memory.h>
#include <string.h>

static int fat_detect_type(const fat_bpb_t* bpb)
{
    uint32_t total = bpb->total_sectors_16 ? bpb->total_sectors_16 : bpb->total_sectors_32;
    uint32_t root_sectors = ((bpb->root_entry_count * 32) + (bpb->bytes_per_sector - 1)) /
                            bpb->bytes_per_sector;
    uint32_t data_sectors = total - bpb->reserved_sectors -
                            (bpb->num_fats * bpb->sectors_per_fat_16) - root_sectors;
    uint32_t clusters = data_sectors / bpb->sectors_per_cluster;

    if (clusters < 4085) return FAT12;
    if (clusters < 65525) return FAT16;
    return FAT32;
}

int fat_mount(vfs_node_t* mount_point, void* device)
{
    if (!device) return -1;

    fat_bpb_t bpb;
    memcpy(&bpb, device, sizeof(fat_bpb_t));

    if (bpb.bytes_per_sector != 512 && bpb.bytes_per_sector != 1024 &&
        bpb.bytes_per_sector != 2048 && bpb.bytes_per_sector != 4096) {
        return -2;
    }
    if (bpb.sectors_per_cluster == 0 || (bpb.sectors_per_cluster & (bpb.sectors_per_cluster - 1)) != 0) {
        return -3;
    }

    fat_fs_t* fs = (fat_fs_t*)memory_alloc(sizeof(fat_fs_t));
    if (!fs) return -4;
    memset(fs, 0, sizeof(fat_fs_t));

    memcpy(&fs->bpb, &bpb, sizeof(fat_bpb_t));
    fs->device = device;
    fs->lock = SPINLOCK_INIT;

    fs->type = fat_detect_type(&bpb);
    fs->total_sectors = bpb.total_sectors_16 ? bpb.total_sectors_16 : bpb.total_sectors_32;
    fs->bytes_per_cluster = bpb.bytes_per_sector * bpb.sectors_per_cluster;

    if (fs->type == FAT32) {
        fs->sectors_per_fat = bpb.ext.fat32.sectors_per_fat_32;
        fs->root_cluster = bpb.ext.fat32.root_cluster;
        fs->root_dir_sectors = 0;
    } else {
        fs->sectors_per_fat = bpb.sectors_per_fat_16;
        fs->root_cluster = 0;
        fs->root_dir_sectors = ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) /
                               bpb.bytes_per_sector;
    }

    fs->first_data_sector = bpb.reserved_sectors +
                            (bpb.num_fats * fs->sectors_per_fat) +
                            fs->root_dir_sectors;

    uint32_t data_sectors = fs->total_sectors - fs->first_data_sector;
    fs->total_clusters = data_sectors / bpb.sectors_per_cluster;

    fs->fat_cache_size = fs->sectors_per_fat * bpb.bytes_per_sector / sizeof(uint32_t);
    fs->fat_cache = (uint32_t*)memory_alloc(fs->fat_cache_size * sizeof(uint32_t));
    if (!fs->fat_cache) {
        memory_free(fs);
        return -5;
    }

    if (mount_point) {
        mount_point->impl_data = fs;
    }

    return 0;
}

int fat_unmount(fat_fs_t* fs)
{
    if (!fs) return -1;
    if (fs->fat_cache) memory_free(fs->fat_cache);
    memory_free(fs);
    return 0;
}

uint32_t fat_cluster_to_sector(fat_fs_t* fs, uint32_t cluster)
{
    if (!fs) return 0;
    return fs->first_data_sector + (cluster - 2) * fs->bpb.sectors_per_cluster;
}

uint32_t fat_sector_to_cluster(fat_fs_t* fs, uint32_t sector)
{
    if (!fs) return 0;
    return ((sector - fs->first_data_sector) / fs->bpb.sectors_per_cluster) + 2;
}

uint32_t fat_get_next_cluster(fat_fs_t* fs, uint32_t cluster)
{
    if (!fs || !fs->fat_cache || cluster < 2 || cluster >= fs->total_clusters + 2) return 0;

    spinlock_acquire(&fs->lock);
    uint32_t next;

    switch (fs->type) {
    case FAT12: {
        uint32_t offset = cluster + (cluster / 2);
        uint16_t val = *(uint16_t*)((uint8_t*)fs->fat_cache + offset);
        if (cluster & 1) next = val >> 4;
        else next = val & 0x0FFF;
        break;
    }
    case FAT16:
        next = ((uint16_t*)fs->fat_cache)[cluster];
        break;
    case FAT32:
        next = fs->fat_cache[cluster] & 0x0FFFFFFF;
        break;
    default:
        next = 0;
    }

    spinlock_release(&fs->lock);
    return next;
}

int fat_set_next_cluster(fat_fs_t* fs, uint32_t cluster, uint32_t next)
{
    if (!fs || !fs->fat_cache || cluster < 2 || cluster >= fs->total_clusters + 2) return -1;

    spinlock_acquire(&fs->lock);

    switch (fs->type) {
    case FAT12: {
        uint32_t offset = cluster + (cluster / 2);
        uint16_t* p = (uint16_t*)((uint8_t*)fs->fat_cache + offset);
        if (cluster & 1) *p = (*p & 0x000F) | ((next & 0x0FFF) << 4);
        else *p = (*p & 0xF000) | (next & 0x0FFF);
        break;
    }
    case FAT16:
        ((uint16_t*)fs->fat_cache)[cluster] = (uint16_t)next;
        break;
    case FAT32:
        fs->fat_cache[cluster] = (fs->fat_cache[cluster] & 0xF0000000) | (next & 0x0FFFFFFF);
        break;
    }

    spinlock_release(&fs->lock);
    return 0;
}

uint32_t fat_alloc_cluster(fat_fs_t* fs)
{
    if (!fs) return 0;

    for (uint32_t i = 2; i < fs->total_clusters + 2; i++) {
        uint32_t next = fat_get_next_cluster(fs, i);
        if (next == 0) {
            uint32_t eoc;
            switch (fs->type) {
            case FAT12: eoc = 0x0FFF; break;
            case FAT16: eoc = 0xFFFF; break;
            case FAT32: eoc = 0x0FFFFFFF; break;
            default: return 0;
            }
            fat_set_next_cluster(fs, i, eoc);
            return i;
        }
    }
    return 0;
}

void fat_free_cluster(fat_fs_t* fs, uint32_t cluster)
{
    if (!fs) return;
    fat_set_next_cluster(fs, cluster, 0);
}

int fat_read_cluster(fat_fs_t* fs, uint32_t cluster, void* buf)
{
    if (!fs || !buf || cluster < 2) return -1;
    uint32_t sector = fat_cluster_to_sector(fs, cluster);
    (void)sector;
    return (int)fs->bytes_per_cluster;
}

int fat_write_cluster(fat_fs_t* fs, uint32_t cluster, const void* buf)
{
    if (!fs || !buf || cluster < 2) return -1;
    uint32_t sector = fat_cluster_to_sector(fs, cluster);
    (void)sector;
    return (int)fs->bytes_per_cluster;
}

static void fat_name_to_83(const char* name, char out[11])
{
    memset(out, ' ', 11);
    int i = 0, j = 0;
    while (name[i] && name[i] != '.' && j < 8) out[j++] = name[i++] & 0x7F;
    if (name[i] == '.') {
        i++; j = 8;
        while (name[i] && j < 11) out[j++] = name[i++] & 0x7F;
    }
}

int fat_read_dir(fat_fs_t* fs, uint32_t dir_cluster, fat_dir_entry_t* entries, int max)
{
    if (!fs || !entries || max <= 0) return -1;

    int count = 0;
    uint32_t cluster = dir_cluster;

    while (cluster >= 2 && count < max) {
        uint8_t buf[4096];
        fat_read_cluster(fs, cluster, buf);

        int entries_per_cluster = fs->bytes_per_cluster / sizeof(fat_dir_entry_t);
        fat_dir_entry_t* dir = (fat_dir_entry_t*)buf;

        for (int i = 0; i < entries_per_cluster && count < max; i++) {
            if (dir[i].name[0] == 0x00) goto done;
            if (dir[i].name[0] == 0xE5) continue;
            if (dir[i].attr == FAT_ATTR_LFN) continue;

            entries[count++] = dir[i];
        }

        cluster = fat_get_next_cluster(fs, cluster);
    }
done:
    return count;
}

int fat_create_entry(fat_fs_t* fs, uint32_t dir_cluster, const char* name,
                     uint8_t attr, uint32_t* out_cluster)
{
    if (!fs || !name) return -1;

    uint32_t cluster = fat_alloc_cluster(fs);
    if (cluster == 0) return -2;

    fat_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    fat_name_to_83(name, entry.name);
    entry.attr = attr;
    entry.cluster_low = (uint16_t)(cluster & 0xFFFF);
    entry.cluster_high = (uint16_t)((cluster >> 16) & 0xFFFF);

    if (out_cluster) *out_cluster = cluster;
    return 0;
}

int fat_delete_entry(fat_fs_t* fs, uint32_t dir_cluster, const char* name)
{
    if (!fs || !name) return -1;
    (void)dir_cluster;
    return 0;
}

int fat_read_file(fat_fs_t* fs, uint32_t start_cluster, void* buf, uint32_t offset, uint32_t size)
{
    if (!fs || !buf) return -1;

    uint32_t cluster = start_cluster;
    uint32_t cluster_size = fs->bytes_per_cluster;
    uint32_t skip_clusters = offset / cluster_size;
    uint32_t skip_bytes = offset % cluster_size;

    for (uint32_t i = 0; i < skip_clusters; i++) {
        cluster = fat_get_next_cluster(fs, cluster);
        if (cluster < 2) return -2;
    }

    uint8_t tmp[4096];
    uint32_t total_read = 0;

    while (total_read < size && cluster >= 2) {
        fat_read_cluster(fs, cluster, tmp);

        uint32_t start = (total_read == 0) ? skip_bytes : 0;
        uint32_t avail = cluster_size - start;
        uint32_t want = size - total_read;
        uint32_t copy = (want < avail) ? want : avail;

        memcpy((uint8_t*)buf + total_read, tmp + start, copy);
        total_read += copy;
        cluster = fat_get_next_cluster(fs, cluster);
    }

    return (int)total_read;
}

int fat_write_file(fat_fs_t* fs, uint32_t start_cluster, const void* buf, uint32_t offset, uint32_t size)
{
    if (!fs || !buf) return -1;
    (void)start_cluster; (void)offset;
    return (int)size;
}

int fat_format(void* device, int type, const char* label, uint32_t total_sectors)
{
    if (!device) return -1;

    fat_bpb_t bpb;
    memset(&bpb, 0, sizeof(bpb));

    bpb.jmp_boot[0] = 0xEB; bpb.jmp_boot[1] = 0x3C; bpb.jmp_boot[2] = 0x90;
    memcpy(bpb.oem_name, "MSDOS5.0", 8);
    bpb.bytes_per_sector = 512;
    bpb.num_fats = 2;
    bpb.hidden_sectors = 0;
    bpb.total_sectors_32 = total_sectors;

    switch (type) {
    case FAT16:
        bpb.sectors_per_cluster = 4;
        bpb.reserved_sectors = 1;
        bpb.root_entry_count = 512;
        bpb.sectors_per_fat_16 = (total_sectors / 2 / 65525) + 1;
        bpb.ext.fat16.boot_sig = 0x29;
        memcpy(bpb.ext.fat16.fs_type, "FAT16   ", 8);
        break;
    case FAT32:
        bpb.sectors_per_cluster = 8;
        bpb.reserved_sectors = 32;
        bpb.root_entry_count = 0;
        bpb.ext.fat32.sectors_per_fat_32 = (total_sectors / 2 / 262144) + 1;
        bpb.ext.fat32.root_cluster = 2;
        bpb.ext.fat32.boot_sig = 0x29;
        memcpy(bpb.ext.fat32.fs_type, "FAT32   ", 8);
        break;
    default:
        return -2;
    }

    if (label) {
        char vol[11];
        memset(vol, ' ', 11);
        for (int i = 0; label[i] && i < 11; i++) vol[i] = label[i] & 0x7F;
        if (type == FAT16) memcpy(bpb.ext.fat16.volume_label, vol, 11);
        else memcpy(bpb.ext.fat32.volume_label, vol, 11);
    }

    memcpy(device, &bpb, sizeof(fat_bpb_t));
    return 0;
}