#include "ntfs.h"
#include <arch/memory.h>
#include <string.h>

static int ntfs_read_sector(ntfs_fs_t* fs, uint64_t sector, void* buf)
{
    if (!fs || !fs->device || !buf) return -1;
    return device_read(fs->device, sector * NTFS_SECTOR_SIZE, buf, NTFS_SECTOR_SIZE);
}

static int ntfs_read_cluster(ntfs_fs_t* fs, uint64_t cluster, void* buf)
{
    if (!fs || !buf) return -1;
    uint64_t sector = cluster * fs->boot.sectors_per_cluster;
    uint8_t* dst = (uint8_t*)buf;
    for (uint16_t i = 0; i < fs->boot.sectors_per_cluster; i++) {
        int result = ntfs_read_sector(fs, sector + i, dst + i * NTFS_SECTOR_SIZE);
        if (result != 0) return result;
    }
    return 0;
}

int ntfs_mount(ntfs_fs_t* fs, void* device)
{
    if (!fs || !device) return -1;

    memset(fs, 0, sizeof(ntfs_fs_t));
    fs->device = device;
    spin_init(&fs->lock);

    uint8_t boot_buf[512];
    int result = device_read(device, 0, boot_buf, 512);
    if (result != 0) return -2;

    memcpy(&fs->boot, boot_buf, sizeof(ntfs_boot_sector_t));

    if (fs->boot.bytes_per_sector != 512 && fs->boot.bytes_per_sector != 1024 &&
        fs->boot.bytes_per_sector != 2048 && fs->boot.bytes_per_sector != 4096) return -3;

    fs->cluster_size = fs->boot.bytes_per_sector * fs->boot.sectors_per_cluster;

    if (fs->boot.clusters_per_mft_record > 0) {
        fs->mft_record_size = fs->cluster_size * fs->boot.clusters_per_mft_record;
    } else {
        fs->mft_record_size = 1 << (-fs->boot.clusters_per_mft_record);
    }

    if (fs->boot.clusters_per_idx_record > 0) {
        fs->idx_record_size = fs->cluster_size * fs->boot.clusters_per_idx_record;
    } else {
        fs->idx_record_size = 1 << (-fs->boot.clusters_per_idx_record);
    }

    fs->mft_start = fs->boot.mft_cluster * fs->boot.sectors_per_cluster;

    return 0;
}

int ntfs_umount(ntfs_fs_t* fs)
{
    if (!fs) return -1;
    if (fs->mft_cache) memory_free(fs->mft_cache);
    fs->mft_cache = NULL;
    return 0;
}

int ntfs_read_mft_record(ntfs_fs_t* fs, uint64_t mft_index, void* buf)
{
    if (!fs || !buf) return -1;

    spinlock_acquire(&fs->lock);
    uint64_t offset = fs->mft_start * NTFS_SECTOR_SIZE + mft_index * fs->mft_record_size;
    int result = device_read(fs->device, offset, buf, fs->mft_record_size);
    spinlock_release(&fs->lock);

    if (result != 0) return result;

    ntfs_mft_record_header_t* hdr = (ntfs_mft_record_header_t*)buf;
    if (hdr->magic != 0x46494C45) return -2;

    return 0;
}

int ntfs_find_attr(ntfs_fs_t* fs, const void* mft_record, uint32_t attr_type,
                    const void** attr_data, uint32_t* attr_size)
{
    if (!fs || !mft_record || !attr_data || !attr_size) return -1;

    const ntfs_mft_record_header_t* hdr = (const ntfs_mft_record_header_t*)mft_record;
    if (hdr->magic != 0x46494C45) return -2;

    uint32_t offset = hdr->attr_offset;
    const uint8_t* record = (const uint8_t*)mft_record;

    while (offset < hdr->used_size - sizeof(ntfs_attr_header_t)) {
        const ntfs_attr_header_t* attr = (const ntfs_attr_header_t*)(record + offset);

        if (attr->type == 0xFFFFFFFF || attr->length == 0) break;

        if (attr->type == attr_type) {
            if (attr->non_resident == 0) {
                uint16_t data_offset = offset + sizeof(ntfs_attr_header_t) + 4;
                *attr_data = record + data_offset;
                *attr_size = attr->length - (data_offset - offset);
            } else {
                *attr_data = record + offset;
                *attr_size = attr->length;
            }
            return 0;
        }

        offset += attr->length;
    }

    return -3;
}

int ntfs_decode_runs(const uint8_t* run_data, ntfs_run_t* runs, int max_runs)
{
    if (!run_data || !runs || max_runs <= 0) return -1;

    int count = 0;
    int offset = 0;
    uint64_t lcn = 0;
    uint64_t vcn = 0;

    while (count < max_runs) {
        uint8_t header = run_data[offset];
        if (header == 0) break;

        uint8_t length_size = header & 0x0F;
        uint8_t offset_size = (header >> 4) & 0x0F;
        offset++;

        if (length_size == 0 || offset_size == 0) break;

        uint64_t length = 0;
        for (uint8_t i = 0; i < length_size; i++) {
            length |= (uint64_t)run_data[offset++] << (i * 8);
        }

        int64_t lcn_offset = 0;
        for (uint8_t i = 0; i < offset_size; i++) {
            lcn_offset |= (int64_t)run_data[offset++] << (i * 8);
        }
        if (lcn_offset < 0 && offset_size < 8) {
            lcn_offset -= (int64_t)1 << (offset_size * 8);
        }

        lcn += lcn_offset;

        runs[count].start_vcn = vcn;
        runs[count].start_lcn = lcn_offset >= 0 ? lcn : 0;
        runs[count].length = length;

        vcn += length;
        count++;
    }

    return count;
}

int ntfs_read_attr(ntfs_fs_t* fs, const void* mft_record, uint32_t attr_type,
                    uint64_t offset, void* buf, uint32_t size)
{
    if (!fs || !mft_record || !buf) return -1;

    const void* attr_data;
    uint32_t attr_size;
    int result = ntfs_find_attr(fs, mft_record, attr_type, &attr_data, &attr_size);
    if (result != 0) return result;

    const ntfs_attr_header_t* attr = (const ntfs_attr_header_t*)attr_data;

    if (attr->non_resident == 0) {
        uint32_t data_offset = sizeof(ntfs_attr_header_t) + 4;
        const uint8_t* data = (const uint8_t*)attr + data_offset;
        uint32_t data_size = attr->length - data_offset;

        if (offset >= data_size) return 0;
        uint32_t to_copy = size;
        if (offset + to_copy > data_size) to_copy = data_size - (uint32_t)offset;
        memcpy(buf, data + offset, to_copy);
        return (int)to_copy;
    }

    const uint8_t* run_start = (const uint8_t*)attr + sizeof(ntfs_attr_header_t) + 0x40;
    ntfs_run_t runs[128];
    int run_count = ntfs_decode_runs(run_start, runs, 128);
    if (run_count <= 0) return -2;

    uint8_t* dst = (uint8_t*)buf;
    uint32_t remaining = size;
    uint64_t current_offset = offset;

    while (remaining > 0) {
        uint64_t cluster_offset = current_offset / fs->cluster_size;
        uint32_t byte_offset = (uint32_t)(current_offset % fs->cluster_size);
        uint32_t to_copy = fs->cluster_size - byte_offset;
        if (to_copy > remaining) to_copy = remaining;

        int found = 0;
        for (int i = 0; i < run_count; i++) {
            if (cluster_offset >= runs[i].start_vcn &&
                cluster_offset < runs[i].start_vcn + runs[i].length) {
                uint64_t lcn = runs[i].start_lcn + (cluster_offset - runs[i].start_vcn);
                uint8_t cluster_buf[4096];
                int r = ntfs_read_cluster(fs, lcn, cluster_buf);
                if (r != 0) return r;
                memcpy(dst, cluster_buf + byte_offset, to_copy);
                found = 1;
                break;
            }
        }

        if (!found) memset(dst, 0, to_copy);
        dst += to_copy;
        current_offset += to_copy;
        remaining -= to_copy;
    }

    return (int)size;
}

int ntfs_lookup(ntfs_fs_t* fs, uint64_t dir_mft, const char* name, uint64_t* out_mft)
{
    if (!fs || !name || !out_mft) return -1;

    uint8_t mft_buf[4096];
    int result = ntfs_read_mft_record(fs, dir_mft, mft_buf);
    if (result != 0) return result;

    const void* index_root;
    uint32_t index_size;
    result = ntfs_find_attr(fs, mft_buf, NTFS_ATTR_INDEX_ROOT, &index_root, &index_size);
    if (result != 0) return -2;

    (void)out_mft;
    return -3;
}

int ntfs_read_dir(ntfs_fs_t* fs, uint64_t dir_mft,
                   int (*callback)(const char* name, uint64_t mft, void* ctx), void* ctx)
{
    if (!fs || !callback) return -1;

    uint8_t mft_buf[4096];
    int result = ntfs_read_mft_record(fs, dir_mft, mft_buf);
    if (result != 0) return result;

    (void)ctx;
    return 0;
}

int ntfs_create(ntfs_fs_t* fs, uint64_t dir_mft, const char* name, uint32_t flags)
{
    if (!fs || !name) return -1;
    (void)dir_mft; (void)flags;
    return -1;
}

int ntfs_unlink(ntfs_fs_t* fs, uint64_t dir_mft, const char* name)
{
    if (!fs || !name) return -1;
    (void)dir_mft;
    return -1;
}

int ntfs_read_file(ntfs_fs_t* fs, uint64_t mft_index, uint64_t offset, void* buf, uint32_t size)
{
    if (!fs || !buf) return -1;

    uint8_t mft_buf[4096];
    int result = ntfs_read_mft_record(fs, mft_index, mft_buf);
    if (result != 0) return result;

    return ntfs_read_attr(fs, mft_buf, NTFS_ATTR_DATA, offset, buf, size);
}

int ntfs_write_file(ntfs_fs_t* fs, uint64_t mft_index, uint64_t offset, const void* buf, uint32_t size)
{
    if (!fs || !buf) return -1;
    (void)mft_index; (void)offset; (void)size;
    return -1;
}

uint64_t ntfs_get_file_size(ntfs_fs_t* fs, uint64_t mft_index)
{
    if (!fs) return 0;

    uint8_t mft_buf[4096];
    int result = ntfs_read_mft_record(fs, mft_index, mft_buf);
    if (result != 0) return 0;

    const void* attr_data;
    uint32_t attr_size;
    result = ntfs_find_attr(fs, mft_buf, NTFS_ATTR_DATA, &attr_data, &attr_size);
    if (result != 0) return 0;

    const ntfs_attr_header_t* attr = (const ntfs_attr_header_t*)attr_data;
    if (attr->non_resident == 0) {
        return attr_size;
    }

    return 0;
}

uint32_t ntfs_get_file_attrs(ntfs_fs_t* fs, uint64_t mft_index)
{
    if (!fs) return 0;

    uint8_t mft_buf[4096];
    int result = ntfs_read_mft_record(fs, mft_index, mft_buf);
    if (result != 0) return 0;

    const void* attr_data;
    uint32_t attr_size;
    result = ntfs_find_attr(fs, mft_buf, NTFS_ATTR_STANDARD_INFORMATION, &attr_data, &attr_size);
    if (result != 0) return 0;

    if (attr_size >= 4) {
        const uint32_t* flags = (const uint32_t*)((const uint8_t*)attr_data + 0x18);
        return *flags;
    }

    return 0;
}

uint64_t ntfs_alloc_cluster(ntfs_fs_t* fs)
{
    if (!fs) return 0;
    return 0;
}

void ntfs_free_cluster(ntfs_fs_t* fs, uint64_t cluster)
{
    if (!fs) return;
    (void)cluster;
}