#include <arch/exfat.h>
#include <arch/memory.h>
#include <arch/ahci.h>
#include <string.h>

static int exfat_read_sector(void* device, uint64_t lba, void* buf)
{
    (void)device;
    return ahci_read_sector(0, lba, buf);
}

static int exfat_write_sector(void* device, uint64_t lba, const void* buf)
{
    (void)device;
    return ahci_write_sector(0, lba, buf);
}

static uint16_t exfat_calc_name_hash(const uint16_t* name, uint8_t len)
{
    uint16_t hash = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint16_t c = name[i];
        hash = ((hash << 15) | (hash >> 1)) + (c << 8);
        hash = ((hash << 15) | (hash >> 1)) + (c & 0xFF);
    }
    return hash;
}

uint16_t exfat_name_hash(const uint16_t* name, uint8_t len)
{
    return exfat_calc_name_hash(name, len);
}

void exfat_init(void)
{
}

int exfat_mount(vfs_node_t* mount_point, void* device)
{
    if (!device) return -1;

    exfat_bpb_t bpb;
    if (exfat_read_sector(device, 0, &bpb) != 0) return -2;

    if (bpb.signature != 0xAA55) return -3;
    if (bpb.jump_boot[0] != 0xEB && bpb.jump_boot[0] != 0xE9) return -4;

    uint8_t bps_shift = bpb.bytes_per_sector_shift;
    uint8_t spc_shift = bpb.sectors_per_cluster_shift;
    if (bps_shift < 9 || bps_shift > 12) return -5;
    if (spc_shift > 25 - bps_shift) return -6;

    exfat_fs_t* fs = (exfat_fs_t*)memory_alloc(sizeof(exfat_fs_t));
    if (!fs) return -7;
    memset(fs, 0, sizeof(exfat_fs_t));

    memcpy(&fs->bpb, &bpb, sizeof(exfat_bpb_t));
    fs->device = device;
    fs->lock = SPINLOCK_INIT;

    fs->bytes_per_sector = 1U << bps_shift;
    fs->sectors_per_cluster = 1U << spc_shift;
    fs->bytes_per_cluster = fs->bytes_per_sector * fs->sectors_per_cluster;
    fs->fat_start_sector = bpb.fat_offset;
    fs->fat_sectors = bpb.fat_length;
    fs->cluster_heap_start = bpb.cluster_heap_offset;
    fs->total_clusters = bpb.cluster_count;
    fs->root_dir_cluster = bpb.root_directory;

    fs->fat_table_size = (uint64_t)fs->fat_sectors * fs->bytes_per_sector;
    fs->fat_table = (uint32_t*)memory_alloc(fs->fat_table_size);
    if (!fs->fat_table) {
        memory_free(fs);
        return -8;
    }

    for (uint32_t i = 0; i < fs->fat_sectors; i++) {
        exfat_read_sector(device, fs->fat_start_sector + i,
                          (uint8_t*)fs->fat_table + (uint64_t)i * fs->bytes_per_sector);
    }

    fs->mounted = 1;
    if (mount_point) {
        mount_point->impl_data = fs;
    }

    return 0;
}

int exfat_unmount(exfat_fs_t* fs)
{
    if (!fs) return -1;
    if (fs->fat_table) memory_free(fs->fat_table);
    fs->mounted = 0;
    memory_free(fs);
    return 0;
}

uint32_t exfat_cluster_to_sector(exfat_fs_t* fs, uint32_t cluster)
{
    if (!fs || cluster < 2) return 0;
    return fs->cluster_heap_start + (cluster - 2) * fs->sectors_per_cluster;
}

uint32_t exfat_get_next_cluster(exfat_fs_t* fs, uint32_t cluster)
{
    if (!fs || !fs->fat_table || cluster < 2 || cluster >= fs->total_clusters + 2) return EXFAT_CLUSTER_END;
    spinlock_acquire(&fs->lock);
    uint32_t next = fs->fat_table[cluster];
    spinlock_release(&fs->lock);
    return next;
}

int exfat_set_next_cluster(exfat_fs_t* fs, uint32_t cluster, uint32_t next)
{
    if (!fs || !fs->fat_table || cluster < 2 || cluster >= fs->total_clusters + 2) return -1;
    spinlock_acquire(&fs->lock);
    fs->fat_table[cluster] = next;
    uint32_t fat_index = cluster / (fs->bytes_per_sector / sizeof(uint32_t));
    uint64_t lba = fs->fat_start_sector + fat_index;
    uint32_t sector_offset = fat_index * fs->bytes_per_sector;
    exfat_write_sector(fs->device, lba, (uint8_t*)fs->fat_table + sector_offset);
    spinlock_release(&fs->lock);
    return 0;
}

uint32_t exfat_alloc_cluster(exfat_fs_t* fs)
{
    if (!fs || !fs->fat_table) return 0;
    spinlock_acquire(&fs->lock);
    for (uint32_t i = 2; i < fs->total_clusters + 2; i++) {
        if (fs->fat_table[i] == EXFAT_CLUSTER_FREE) {
            fs->fat_table[i] = EXFAT_CLUSTER_END;
            spinlock_release(&fs->lock);
            return i;
        }
    }
    spinlock_release(&fs->lock);
    return 0;
}

void exfat_free_cluster(exfat_fs_t* fs, uint32_t cluster)
{
    if (!fs) return;
    exfat_set_next_cluster(fs, cluster, EXFAT_CLUSTER_FREE);
}

int exfat_read_cluster(exfat_fs_t* fs, uint32_t cluster, void* buf)
{
    if (!fs || !buf || cluster < 2) return -1;
    uint32_t sector = exfat_cluster_to_sector(fs, cluster);
    uint8_t* dst = (uint8_t*)buf;
    for (uint32_t i = 0; i < fs->sectors_per_cluster; i++) {
        if (exfat_read_sector(fs->device, sector + i, dst + (uint64_t)i * fs->bytes_per_sector) != 0)
            return -2;
    }
    return (int)fs->bytes_per_cluster;
}

int exfat_write_cluster(exfat_fs_t* fs, uint32_t cluster, const void* buf)
{
    if (!fs || !buf || cluster < 2) return -1;
    uint32_t sector = exfat_cluster_to_sector(fs, cluster);
    const uint8_t* src = (const uint8_t*)buf;
    for (uint32_t i = 0; i < fs->sectors_per_cluster; i++) {
        if (exfat_write_sector(fs->device, sector + i, src + (uint64_t)i * fs->bytes_per_sector) != 0)
            return -2;
    }
    return (int)fs->bytes_per_cluster;
}

static int exfat_read_file_entry_set(exfat_fs_t* fs, uint8_t* cluster_buf,
                                     int offset, exfat_dir_info_t* info)
{
    exfat_file_entry_t* file = (exfat_file_entry_t*)(cluster_buf + offset);
    if (file->type != EXFAT_ENTRY_FILE) return -1;

    uint8_t secondary = file->secondary_count;
    info->attributes = file->attributes;
    info->create_time = file->create_time;
    info->modify_time = file->modify_time;
    info->access_time = file->access_time;

    int pos = offset + 32;
    exfat_stream_entry_t* stream = (exfat_stream_entry_t*)(cluster_buf + pos);
    if (stream->type != EXFAT_ENTRY_STREAM) return -2;

    info->start_cluster = stream->start_cluster;
    info->data_length = stream->data_length;
    info->name_hash = stream->name_hash;
    info->name_len = stream->name_length;

    pos += 32;
    info->name_len = 0;
    for (uint8_t i = 2; i < secondary && info->name_len < EXFAT_MAX_NAME_LEN; i++) {
        exfat_name_entry_t* ne = (exfat_name_entry_t*)(cluster_buf + pos);
        if (ne->type != EXFAT_ENTRY_FILE_NAME) break;
        for (int j = 0; j < EXFAT_MAX_DENTRY_NAME_LEN && info->name_len < EXFAT_MAX_NAME_LEN; j++) {
            info->name[info->name_len++] = ne->name[j];
        }
        pos += 32;
    }

    return 0;
}

int exfat_read_dir(exfat_fs_t* fs, uint32_t dir_cluster, exfat_dir_info_t* entries, int max)
{
    if (!fs || !entries || max <= 0) return -1;

    int count = 0;
    uint32_t cluster = dir_cluster;
    uint8_t* buf = (uint8_t*)memory_alloc(fs->bytes_per_cluster);
    if (!buf) return -2;

    while (cluster >= 2 && cluster != EXFAT_CLUSTER_END && count < max) {
        if (exfat_read_cluster(fs, cluster, buf) < 0) break;

        int entries_per_cluster = fs->bytes_per_cluster / 32;
        for (int i = 0; i < entries_per_cluster && count < max; ) {
            uint8_t type = buf[i * 32];
            if (type == EXFAT_ENTRY_END) goto done;
            if (type == EXFAT_ENTRY_FILE) {
                exfat_dir_info_t info;
                memset(&info, 0, sizeof(info));
                if (exfat_read_file_entry_set(fs, buf, i * 32, &info) == 0) {
                    entries[count++] = info;
                }
                exfat_file_entry_t* fe = (exfat_file_entry_t*)(buf + i * 32);
                i += 1 + fe->secondary_count;
            } else {
                i++;
            }
        }
        cluster = exfat_get_next_cluster(fs, cluster);
    }

done:
    memory_free(buf);
    return count;
}

int exfat_create_entry(exfat_fs_t* fs, uint32_t dir_cluster, const uint16_t* name,
                       uint8_t name_len, uint16_t attr, uint32_t* out_cluster)
{
    if (!fs || !name || name_len == 0) return -1;

    uint32_t data_cluster = exfat_alloc_cluster(fs);
    if (data_cluster == 0) return -2;

    uint8_t name_entries = (name_len + EXFAT_MAX_DENTRY_NAME_LEN - 1) / EXFAT_MAX_DENTRY_NAME_LEN;
    uint8_t secondary = 1 + name_entries;

    uint8_t* buf = (uint8_t*)memory_alloc(fs->bytes_per_cluster);
    if (!buf) return -3;
    memset(buf, 0, fs->bytes_per_cluster);

    int pos = 0;
    exfat_file_entry_t* fe = (exfat_file_entry_t*)(buf + pos);
    fe->type = EXFAT_ENTRY_FILE;
    fe->secondary_count = secondary;
    fe->attributes = attr;
    pos += 32;

    exfat_stream_entry_t* se = (exfat_stream_entry_t*)(buf + pos);
    se->type = EXFAT_ENTRY_STREAM;
    se->flags = 0x01;
    se->name_length = name_len;
    se->name_hash = exfat_calc_name_hash(name, name_len);
    se->start_cluster = data_cluster;
    se->data_length = 0;
    pos += 32;

    for (uint8_t i = 0; i < name_entries; i++) {
        exfat_name_entry_t* ne = (exfat_name_entry_t*)(buf + pos);
        ne->type = EXFAT_ENTRY_FILE_NAME;
        ne->flags = 0;
        for (int j = 0; j < EXFAT_MAX_DENTRY_NAME_LEN; j++) {
            int idx = i * EXFAT_MAX_DENTRY_NAME_LEN + j;
            ne->name[j] = (idx < name_len) ? name[idx] : 0;
        }
        pos += 32;
    }

    if (out_cluster) *out_cluster = data_cluster;
    memory_free(buf);
    return 0;
}

int exfat_delete_entry(exfat_fs_t* fs, uint32_t dir_cluster, const uint16_t* name, uint8_t name_len)
{
    if (!fs || !name) return -1;

    uint8_t* buf = (uint8_t*)memory_alloc(fs->bytes_per_cluster);
    if (!buf) return -2;

    uint32_t cluster = dir_cluster;
    while (cluster >= 2 && cluster != EXFAT_CLUSTER_END) {
        if (exfat_read_cluster(fs, cluster, buf) < 0) break;
        int entries_per_cluster = fs->bytes_per_cluster / 32;
        for (int i = 0; i < entries_per_cluster; ) {
            uint8_t type = buf[i * 32];
            if (type == EXFAT_ENTRY_END) break;
            if (type == EXFAT_ENTRY_FILE) {
                exfat_dir_info_t info;
                memset(&info, 0, sizeof(info));
                if (exfat_read_file_entry_set(fs, buf, i * 32, &info) == 0 &&
                    info.name_len == name_len) {
                    int match = 1;
                    for (uint8_t j = 0; j < name_len; j++) {
                        if (info.name[j] != name[j]) { match = 0; break; }
                    }
                    if (match) {
                        exfat_file_entry_t* fe = (exfat_file_entry_t*)(buf + i * 32);
                        int total = 1 + fe->secondary_count;
                        for (int k = 0; k < total; k++) {
                            buf[(i + k) * 32] = EXFAT_ENTRY_END;
                        }
                        exfat_write_cluster(fs, cluster, buf);
                        memory_free(buf);
                        return 0;
                    }
                }
                exfat_file_entry_t* fe = (exfat_file_entry_t*)(buf + i * 32);
                i += 1 + fe->secondary_count;
            } else {
                i++;
            }
        }
        cluster = exfat_get_next_cluster(fs, cluster);
    }

    memory_free(buf);
    return -3;
}

int exfat_read_file(exfat_fs_t* fs, uint32_t start_cluster, void* buf, uint64_t offset, uint64_t size)
{
    if (!fs || !buf || start_cluster < 2) return -1;

    uint32_t cluster = start_cluster;
    uint32_t cluster_size = fs->bytes_per_cluster;
    uint64_t skip_clusters = offset / cluster_size;
    uint64_t skip_bytes = offset % cluster_size;

    for (uint64_t i = 0; i < skip_clusters; i++) {
        cluster = exfat_get_next_cluster(fs, cluster);
        if (cluster < 2 || cluster == EXFAT_CLUSTER_END) return -2;
    }

    uint8_t* tmp = (uint8_t*)memory_alloc(cluster_size);
    if (!tmp) return -3;
    uint64_t total_read = 0;

    while (total_read < size && cluster >= 2 && cluster != EXFAT_CLUSTER_END) {
        if (exfat_read_cluster(fs, cluster, tmp) < 0) break;

        uint64_t start = (total_read == 0) ? skip_bytes : 0;
        uint64_t avail = cluster_size - start;
        uint64_t want = size - total_read;
        uint64_t copy = (want < avail) ? want : avail;

        memcpy((uint8_t*)buf + total_read, tmp + start, copy);
        total_read += copy;
        cluster = exfat_get_next_cluster(fs, cluster);
    }

    memory_free(tmp);
    return (int)total_read;
}

int exfat_write_file(exfat_fs_t* fs, uint32_t start_cluster, const void* buf, uint64_t offset, uint64_t size)
{
    if (!fs || !buf || start_cluster < 2) return -1;

    uint32_t cluster = start_cluster;
    uint32_t cluster_size = fs->bytes_per_cluster;
    uint64_t skip_clusters = offset / cluster_size;
    uint64_t skip_bytes = offset % cluster_size;

    for (uint64_t i = 0; i < skip_clusters; i++) {
        cluster = exfat_get_next_cluster(fs, cluster);
        if (cluster < 2 || cluster == EXFAT_CLUSTER_END) return -2;
    }

    uint8_t* tmp = (uint8_t*)memory_alloc(cluster_size);
    if (!tmp) return -3;
    uint64_t total_written = 0;

    while (total_written < size && cluster >= 2 && cluster != EXFAT_CLUSTER_END) {
        if (exfat_read_cluster(fs, cluster, tmp) < 0) break;

        uint64_t start = (total_written == 0) ? skip_bytes : 0;
        uint64_t avail = cluster_size - start;
        uint64_t want = size - total_written;
        uint64_t copy = (want < avail) ? want : avail;

        memcpy(tmp + start, (const uint8_t*)buf + total_written, copy);
        exfat_write_cluster(fs, cluster, tmp);
        total_written += copy;
        cluster = exfat_get_next_cluster(fs, cluster);
    }

    memory_free(tmp);
    return (int)total_written;
}