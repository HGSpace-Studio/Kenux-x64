#include "iso9660.h"
#include <string.h>

int iso9660_mount(iso9660_fs_t* fs, void* device)
{
    if (!fs || !device) return -1;

    memset(fs, 0, sizeof(iso9660_fs_t));
    fs->device = device;
    spin_init(&fs->lock);

    uint8_t sector_buf[ISO9660_SECTOR_SIZE];
    for (int i = 16; i < 100; i++) {
        int result = device_read(device, i * ISO9660_SECTOR_SIZE, sector_buf, ISO9660_SECTOR_SIZE);
        if (result != 0) return -2;

        uint8_t type = sector_buf[0];
        if (memcmp(sector_buf + 1, "CD001", 5) != 0) continue;

        if (type == ISO9660_VD_PRIMARY) {
            memcpy(&fs->pvd, sector_buf, sizeof(iso9660_pvd_t));
            fs->block_size = fs->pvd.logical_block_size_le;
            fs->total_blocks = fs->pvd.volume_space_size_le;

            const uint8_t* root_rec = fs->pvd.root_directory_record;
            fs->root_extent = root_rec[2] | (root_rec[3] << 8) |
                              (root_rec[4] << 16) | (root_rec[5] << 24);
            fs->root_size = root_rec[10] | (root_rec[11] << 8) |
                            (root_rec[12] << 16) | (root_rec[13] << 24);
            break;
        } else if (type == ISO9660_VD_TERMINATOR) {
            break;
        }
    }

    for (int i = 16; i < 100; i++) {
        int result = device_read(device, i * ISO9660_SECTOR_SIZE, sector_buf, ISO9660_SECTOR_SIZE);
        if (result != 0) break;

        uint8_t type = sector_buf[0];
        if (memcmp(sector_buf + 1, "CD001", 5) != 0) continue;

        if (type == ISO9660_VD_SUPPLEMENTARY) {
            if (sector_buf[0x57] == 0x25 && sector_buf[0x58] == 0xBF &&
                sector_buf[0x59] == 0x8E && sector_buf[0x5A] == 0x45) {
                fs->has_joliet = 1;
            }
            break;
        } else if (type == ISO9660_VD_TERMINATOR) {
            break;
        }
    }

    return 0;
}

int iso9660_umount(iso9660_fs_t* fs)
{
    if (!fs) return -1;
    return 0;
}

int iso9660_read_sector(iso9660_fs_t* fs, uint32_t sector, void* buf)
{
    if (!fs || !buf) return -1;
    return device_read(fs->device, (uint64_t)sector * ISO9660_SECTOR_SIZE, buf, ISO9660_SECTOR_SIZE);
}

int iso9660_read_dir(iso9660_fs_t* fs, uint32_t extent, uint32_t size,
                      int (*callback)(const char* name, uint32_t extent, uint32_t size,
                                      uint8_t flags, void* ctx), void* ctx)
{
    if (!fs || !callback) return -1;

    uint8_t* dir_data = (uint8_t*)memory_alloc(size + ISO9660_SECTOR_SIZE);
    if (!dir_data) return -2;

    uint32_t sectors = (size + ISO9660_SECTOR_SIZE - 1) / ISO9660_SECTOR_SIZE;
    for (uint32_t i = 0; i < sectors; i++) {
        int result = iso9660_read_sector(fs, extent + i, dir_data + i * ISO9660_SECTOR_SIZE);
        if (result != 0) {
            memory_free(dir_data);
            return result;
        }
    }

    uint32_t offset = 0;
    while (offset < size) {
        const iso9660_dir_record_t* rec = (const iso9660_dir_record_t*)(dir_data + offset);

        if (rec->length == 0) {
            uint32_t next_sector = (offset / ISO9660_SECTOR_SIZE + 1) * ISO9660_SECTOR_SIZE;
            if (next_sector >= size) break;
            offset = next_sector;
            continue;
        }

        if (rec->identifier_length > 0) {
            char name[256];
            uint32_t name_len = rec->identifier_length;
            if (name_len > 255) name_len = 255;
            memcpy(name, rec->identifier, name_len);
            name[name_len] = '\0';

            if (name_len >= 2 && name[name_len - 2] == ';') {
                name_len -= 2;
                name[name_len] = '\0';
            }

            if (name_len == 1 && name[0] == 0) {
                memcpy(name, ".", 2);
            } else if (name_len == 1 && name[0] == 1) {
                memcpy(name, "..", 3);
            }

            uint32_t rec_extent = rec->extent_loc_le;
            uint32_t rec_size = rec->data_length_le;

            callback(name, rec_extent, rec_size, rec->file_flags, ctx);
        }

        offset += rec->length;
    }

    memory_free(dir_data);
    return 0;
}

int iso9660_lookup(iso9660_fs_t* fs, uint32_t dir_extent, uint32_t dir_size,
                    const char* name, uint32_t* out_extent, uint32_t* out_size, uint8_t* out_flags)
{
    if (!fs || !name) return -1;

    uint8_t* dir_data = (uint8_t*)memory_alloc(dir_size + ISO9660_SECTOR_SIZE);
    if (!dir_data) return -2;

    uint32_t sectors = (dir_size + ISO9660_SECTOR_SIZE - 1) / ISO9660_SECTOR_SIZE;
    for (uint32_t i = 0; i < sectors; i++) {
        int result = iso9660_read_sector(fs, dir_extent + i, dir_data + i * ISO9660_SECTOR_SIZE);
        if (result != 0) {
            memory_free(dir_data);
            return result;
        }
    }

    uint32_t offset = 0;
    while (offset < dir_size) {
        const iso9660_dir_record_t* rec = (const iso9660_dir_record_t*)(dir_data + offset);

        if (rec->length == 0) {
            uint32_t next_sector = (offset / ISO9660_SECTOR_SIZE + 1) * ISO9660_SECTOR_SIZE;
            if (next_sector >= dir_size) break;
            offset = next_sector;
            continue;
        }

        if (rec->identifier_length > 0) {
            char entry_name[256];
            uint32_t name_len = rec->identifier_length;
            if (name_len > 255) name_len = 255;
            memcpy(entry_name, rec->identifier, name_len);
            entry_name[name_len] = '\0';

            if (name_len >= 2 && entry_name[name_len - 2] == ';') {
                name_len -= 2;
                entry_name[name_len] = '\0';
            }

            if (name_len == 1 && entry_name[0] == 0) {
                memcpy(entry_name, ".", 2);
            } else if (name_len == 1 && entry_name[0] == 1) {
                memcpy(entry_name, "..", 3);
            }

            if (strcmp(entry_name, name) == 0) {
                if (out_extent) *out_extent = rec->extent_loc_le;
                if (out_size) *out_size = rec->data_length_le;
                if (out_flags) *out_flags = rec->file_flags;
                memory_free(dir_data);
                return 0;
            }
        }

        offset += rec->length;
    }

    memory_free(dir_data);
    return -3;
}

int iso9660_read_file(iso9660_fs_t* fs, uint32_t extent, uint64_t offset,
                       void* buf, uint32_t size)
{
    if (!fs || !buf) return -1;

    uint8_t* dst = (uint8_t*)buf;
    uint32_t remaining = size;
    uint64_t file_offset = offset;

    while (remaining > 0) {
        uint32_t sector = extent + (uint32_t)(file_offset / ISO9660_SECTOR_SIZE);
        uint32_t sector_offset = (uint32_t)(file_offset % ISO9660_SECTOR_SIZE);
        uint32_t to_copy = ISO9660_SECTOR_SIZE - sector_offset;
        if (to_copy > remaining) to_copy = remaining;

        uint8_t sector_buf[ISO9660_SECTOR_SIZE];
        int result = iso9660_read_sector(fs, sector, sector_buf);
        if (result != 0) return result;

        memcpy(dst, sector_buf + sector_offset, to_copy);
        dst += to_copy;
        file_offset += to_copy;
        remaining -= to_copy;
    }

    return (int)size;
}

int iso9660_path_lookup(iso9660_fs_t* fs, const char* path,
                          uint32_t* out_extent, uint32_t* out_size, uint8_t* out_flags)
{
    if (!fs || !path) return -1;

    uint32_t current_extent = fs->root_extent;
    uint32_t current_size = fs->root_size;
    uint8_t current_flags = ISO9660_FILE_FLAG_DIRECTORY;

    if (path[0] == '/') path++;

    if (path[0] == '\0') {
        if (out_extent) *out_extent = current_extent;
        if (out_size) *out_size = current_size;
        if (out_flags) *out_flags = current_flags;
        return 0;
    }

    char path_copy[512];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* component = path_copy;
    char* next;

    while (component && *component) {
        next = strchr(component, '/');
        if (next) {
            *next = '\0';
            next++;
        }

        if (!(current_flags & ISO9660_FILE_FLAG_DIRECTORY)) return -2;

        uint32_t found_extent, found_size;
        uint8_t found_flags;
        int result = iso9660_lookup(fs, current_extent, current_size,
                                     component, &found_extent, &found_size, &found_flags);
        if (result != 0) return result;

        current_extent = found_extent;
        current_size = found_size;
        current_flags = found_flags;
        component = next;
    }

    if (out_extent) *out_extent = current_extent;
    if (out_size) *out_size = current_size;
    if (out_flags) *out_flags = current_flags;
    return 0;
}