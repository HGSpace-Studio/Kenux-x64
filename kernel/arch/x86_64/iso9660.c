#include <arch/iso9660.h>
#include <arch/memory.h>
#include <arch/ahci.h>
#include <string.h>

static int iso9660_read_sector(void* device, uint32_t lba, void* buf)
{
    (void)device;
    uint8_t* dst = (uint8_t*)buf;
    for (int i = 0; i < ISO9660_SECTOR_SIZE / 512; i++) {
        if (ahci_read_sector(0, (uint64_t)lba * (ISO9660_SECTOR_SIZE / 512) + i,
                             dst + i * 512) != 0)
            return -1;
    }
    return 0;
}

void iso9660_init(void)
{
}

int iso9660_mount(vfs_node_t* mount_point, void* device)
{
    if (!device) return -1;

    iso9660_pvd_t pvd;
    for (int sector = 16; sector < 100; sector++) {
        if (iso9660_read_sector(device, sector, &pvd) != 0) continue;
        if (pvd.type == ISO9660_PRIMARY_VOLUME &&
            memcmp(pvd.identifier, ISO9660_STD_IDENTIFIER, 5) == 0 &&
            pvd.version == 1) {
            break;
        }
        if (pvd.type == ISO9660_VOLUME_TERMINATOR) return -2;
    }

    if (memcmp(pvd.identifier, ISO9660_STD_IDENTIFIER, 5) != 0) return -3;

    iso9660_fs_t* fs = (iso9660_fs_t*)memory_alloc(sizeof(iso9660_fs_t));
    if (!fs) return -4;
    memset(fs, 0, sizeof(iso9660_fs_t));

    memcpy(&fs->pvd, &pvd, sizeof(iso9660_pvd_t));
    fs->device = device;
    fs->lock = SPINLOCK_INIT;
    fs->block_size = pvd.logical_block_size_le;
    fs->total_blocks = pvd.volume_space_size_le;

    iso9660_dir_record_t* root = (iso9660_dir_record_t*)pvd.directory_record;
    fs->root_extent = root->extent_location_le;
    fs->root_size = root->data_length_le;

    fs->mounted = 1;
    if (mount_point) {
        mount_point->impl_data = fs;
    }

    return 0;
}

int iso9660_unmount(iso9660_fs_t* fs)
{
    if (!fs) return -1;
    fs->mounted = 0;
    memory_free(fs);
    return 0;
}

static void iso9660_normalize_name(const char* raw, uint8_t raw_len, char* out, uint8_t* out_len)
{
    uint8_t len = 0;
    uint8_t sep_pos = raw_len;

    for (uint8_t i = 0; i < raw_len; i++) {
        if (raw[i] == ';' || raw[i] == '\0') {
            sep_pos = i;
            break;
        }
    }

    for (uint8_t i = 0; i < sep_pos; i++) {
        char c = raw[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        out[len++] = c;
    }

    while (len > 1 && out[len - 1] == '.') len--;
    out[len] = '\0';
    *out_len = len;
}

int iso9660_read_dir(iso9660_fs_t* fs, uint32_t extent, uint32_t size, iso9660_dirent_t* entries, int max)
{
    if (!fs || !entries || max <= 0) return -1;

    int count = 0;
    uint32_t sectors = (size + ISO9660_SECTOR_SIZE - 1) / ISO9660_SECTOR_SIZE;
    uint8_t* buf = (uint8_t*)memory_alloc((uint64_t)sectors * ISO9660_SECTOR_SIZE);
    if (!buf) return -2;

    for (uint32_t i = 0; i < sectors; i++) {
        iso9660_read_sector(fs->device, extent + i, buf + (uint64_t)i * ISO9660_SECTOR_SIZE);
    }

    uint32_t offset = 0;
    while (offset < size && count < max) {
        iso9660_dir_record_t* rec = (iso9660_dir_record_t*)(buf + offset);

        if (rec->length == 0) {
            uint32_t next_sector = (offset / ISO9660_SECTOR_SIZE + 1) * ISO9660_SECTOR_SIZE;
            if (next_sector >= size) break;
            offset = next_sector;
            continue;
        }

        if (offset + rec->length > size) break;

        if (rec->file_identifier_length == 1 &&
            (rec->file_identifier[0] == 0x00 || rec->file_identifier[0] == 0x01)) {
            offset += rec->length;
            continue;
        }

        iso9660_dirent_t* entry = &entries[count];
        iso9660_normalize_name(rec->file_identifier, rec->file_identifier_length,
                               entry->name, &entry->name_len);
        entry->extent = rec->extent_location_le;
        entry->size = rec->data_length_le;
        entry->flags = rec->file_flags;
        count++;

        offset += rec->length;
    }

    memory_free(buf);
    return count;
}

int iso9660_read_file(iso9660_fs_t* fs, uint32_t extent, void* buf, uint64_t offset, uint64_t size)
{
    if (!fs || !buf) return -1;

    uint32_t start_sector = (uint32_t)(offset / ISO9660_SECTOR_SIZE);
    uint32_t offset_in_sector = (uint32_t)(offset % ISO9660_SECTOR_SIZE);
    uint64_t total_read = 0;

    uint8_t* sector_buf = (uint8_t*)memory_alloc(ISO9660_SECTOR_SIZE);
    if (!sector_buf) return -2;

    while (total_read < size) {
        uint32_t lba = extent + start_sector;
        if (iso9660_read_sector(fs->device, lba, sector_buf) != 0) break;

        uint64_t avail = ISO9660_SECTOR_SIZE - offset_in_sector;
        uint64_t want = size - total_read;
        uint64_t copy = (want < avail) ? want : avail;

        memcpy((uint8_t*)buf + total_read, sector_buf + offset_in_sector, copy);
        total_read += copy;
        start_sector++;
        offset_in_sector = 0;
    }

    memory_free(sector_buf);
    return (int)total_read;
}

int iso9660_find_entry(iso9660_fs_t* fs, uint32_t dir_extent, uint32_t dir_size,
                       const char* name, iso9660_dirent_t* out)
{
    if (!fs || !name || !out) return -1;

    int max_entries = 256;
    iso9660_dirent_t* entries = (iso9660_dirent_t*)memory_alloc((uint64_t)max_entries * sizeof(iso9660_dirent_t));
    if (!entries) return -2;

    int count = iso9660_read_dir(fs, dir_extent, dir_size, entries, max_entries);
    if (count < 0) {
        memory_free(entries);
        return -3;
    }

    char upper_name[ISO9660_MAX_NAME_LEN + 1];
    uint8_t upper_len = 0;
    for (const char* p = name; *p && upper_len < ISO9660_MAX_NAME_LEN; p++) {
        char c = *p;
        if (c >= 'a' && c <= 'z') c -= 32;
        upper_name[upper_len++] = c;
    }
    upper_name[upper_len] = '\0';

    int found = -4;
    for (int i = 0; i < count; i++) {
        if (entries[i].name_len == upper_len &&
            memcmp(entries[i].name, upper_name, upper_len) == 0) {
            memcpy(out, &entries[i], sizeof(iso9660_dirent_t));
            found = 0;
            break;
        }
    }

    memory_free(entries);
    return found;
}