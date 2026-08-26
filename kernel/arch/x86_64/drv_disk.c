/*
 * Disk / block device driver - freestanding kernel implementation.
 *
 * The driver maintains a fixed-size static pool of virtual disks.  Each
 * disk is backed by a private 64 KiB memory store (128 sectors of 512
 * bytes) so reads and writes are plain memory copies with no hardware
 * interaction.  Built-in "VDA0" and "VDB0" devices are registered at
 * init time and an MBR partition table is parsed (or synthesised for a
 * fresh, zero-filled disk) per device.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>
#include <stdio.h>

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);

/* ------------------------------------------------------------------ *
 * Constants
 * ------------------------------------------------------------------ */
#define DISK_MAX_DEVICES    8
#define DISK_SECTOR_SIZE    512
#define SIM_SECTORS         128
#define SIM_STORE_BYTES     (SIM_SECTORS * DISK_SECTOR_SIZE)

#define MBR_PART_OFFSET     0x1BE
#define MBR_PART_COUNT      4
#define MBR_PART_SIZE       16
#define MBR_SIG_OFFSET      0x1FE
#define MBR_SIG_VALUE       0xAA55u

#define MEDIA_FIXED         0
#define MEDIA_REMOVABLE     1

/* ------------------------------------------------------------------ *
 * Pools
 * ------------------------------------------------------------------ */

/* Per-device memory-backed store. 128 sectors of 512 bytes = 64 KiB. */
static uint8_t g_disk_storage[DISK_MAX_DEVICES][SIM_STORE_BYTES];

/* Device pool. */
static DISK_DEVICE g_disk_pool[DISK_MAX_DEVICES];
static int         g_disk_count;
static int         g_disk_initialized;

/* ------------------------------------------------------------------ *
 * Helpers
 * ------------------------------------------------------------------ */

static DISK_DEVICE* disk_lookup(uint32_t device_id) {
    if (device_id >= (uint32_t)DISK_MAX_DEVICES) return NULL;
    if (!g_disk_pool[device_id].is_online) return NULL;
    return &g_disk_pool[device_id];
}

static void disk_set_default_geometry(DISK_DEVICE* dev) {
    uint64_t spt = 32;          /* sectors per track */
    uint64_t hpc = 8;           /* heads (tracks per cylinder) */
    uint64_t cap = spt * hpc;
    dev->geometry.bytes_per_sector   = dev->sector_size;
    dev->geometry.sectors_per_track = (uint32_t)spt;
    dev->geometry.tracks_per_cylinder = (uint32_t)hpc;
    dev->geometry.cylinders          = (cap > 0) ? (dev->total_sectors / cap) : 0;
    if (dev->geometry.cylinders == 0) dev->geometry.cylinders = 1;
    dev->geometry.media_type          = MEDIA_FIXED;
}

/* Parse the MBR partition table from sector 0 of the device's store.
 * If the table is empty (fresh / zero-filled disk), a single synthetic
 * partition covering the whole disk is created. */
static void disk_parse_mbr(DISK_DEVICE* dev) {
    const uint8_t* sec0 = g_disk_storage[dev->device_id];
    int j;
    int found = 0;

    /* Cache sector 0 in the device's mbr field. */
    memcpy(dev->mbr, sec0, sizeof(dev->mbr));

    /* Parse the 4 16-byte entries at offset 0x1BE. */
    for (j = 0; j < MBR_PART_COUNT; j++) {
        const uint8_t* e = &sec0[MBR_PART_OFFSET + j * MBR_PART_SIZE];
        PARTITION_ENTRY* p = &dev->partitions[j];
        p->boot_flag        = e[0];
        p->start_head       = e[1];
        p->start_sector_cyl = (uint16_t)(e[2] | ((uint16_t)e[3] << 8));
        p->system_id        = e[4];
        p->end_head         = e[5];
        p->end_sector_cyl   = (uint16_t)(e[6] | ((uint16_t)e[7] << 8));
        p->lba_start        = (uint32_t)(e[8]  | ((uint32_t)e[9]  << 8) |
                                         ((uint32_t)e[10] << 16) |
                                         ((uint32_t)e[11] << 24));
        p->sector_count     = (uint32_t)(e[12] | ((uint32_t)e[13] << 8) |
                                         ((uint32_t)e[14] << 16) |
                                         ((uint32_t)e[15] << 24));
        if (p->system_id != 0 || p->lba_start != 0 || p->sector_count != 0) {
            found++;
        }
    }

    if (found == 0) {
        /* Fresh disk: synthesise a single whole-disk partition. */
        PARTITION_ENTRY* p = &dev->partitions[0];
        memset(&dev->partitions[0], 0, sizeof(PARTITION_ENTRY));
        p->boot_flag    = 0x00;
        p->system_id    = 0x83;    /* Linux filesystem */
        p->lba_start    = 1;       /* skip the MBR boot sector */
        p->sector_count = (dev->total_sectors > 1)
                          ? (uint32_t)(dev->total_sectors - 1) : 1u;
        /* Clear the remaining entries. */
        memset(&dev->partitions[1], 0,
               (MBR_PART_COUNT - 1) * sizeof(PARTITION_ENTRY));
        found = 1;
    }

    dev->num_partitions = found;
}

/* ------------------------------------------------------------------ *
 * Public API
 * ------------------------------------------------------------------ */

int disk_driver_init(void) {
    int id;

    /* Reset the pool and count.  Storage is zero-initialised (BSS). */
    memset(g_disk_pool, 0, sizeof(g_disk_pool));
    memset(g_disk_storage, 0, sizeof(g_disk_storage));
    g_disk_count = 0;
    g_disk_initialized = 0;

    /* VDA0: 2048 sectors, 512 B/sector, non-removable. */
    id = disk_register_device("VDA0", 2048, DISK_SECTOR_SIZE);
    if (id < 0) return -1;
    g_disk_pool[id].is_removable = 0;
    g_disk_pool[id].geometry.media_type = MEDIA_FIXED;
    disk_parse_mbr(&g_disk_pool[id]);

    /* VDB0: 1024 sectors, 512 B/sector, removable. */
    id = disk_register_device("VDB0", 1024, DISK_SECTOR_SIZE);
    if (id < 0) return -1;
    g_disk_pool[id].is_removable = 1;
    g_disk_pool[id].geometry.media_type = MEDIA_REMOVABLE;
    disk_parse_mbr(&g_disk_pool[id]);

    g_disk_initialized = 1;
    return 0;
}

int disk_register_device(const char* name, uint64_t total_sectors,
                         uint32_t sector_size) {
    int slot;
    DISK_DEVICE* dev;
    size_t nlen;

    if (name == NULL) return -1;
    if (sector_size == 0) sector_size = DISK_SECTOR_SIZE;

    /* Cap to the simulated store size. */
    if (total_sectors > (uint64_t)SIM_SECTORS) total_sectors = SIM_SECTORS;
    if (total_sectors == 0) total_sectors = 1;

    for (slot = 0; slot < DISK_MAX_DEVICES; slot++) {
        if (!g_disk_pool[slot].is_online) break;
    }
    if (slot >= DISK_MAX_DEVICES) return -1;

    dev = &g_disk_pool[slot];
    memset(dev, 0, sizeof(*dev));

    nlen = strlen(name);
    if (nlen >= sizeof(dev->name)) nlen = sizeof(dev->name) - 1u;
    memcpy(dev->name, name, nlen);
    dev->name[nlen] = 0;

    dev->device_id     = (uint32_t)slot;
    dev->total_sectors = total_sectors;
    dev->sector_size   = sector_size;
    dev->is_removable  = 0;
    dev->is_online     = 1;

    disk_set_default_geometry(dev);

    /* Zero this device's memory-backed store. */
    memset(g_disk_storage[slot], 0, SIM_STORE_BYTES);

    g_disk_count++;
    return slot;
}

int disk_read(uint32_t device_id, uint64_t lba, void* buf, uint32_t count) {
    DISK_DEVICE* dev;
    uint64_t end;
    uint32_t bytes;

    if (buf == NULL || count == 0) return -1;
    dev = disk_lookup(device_id);
    if (dev == NULL) return -1;

    /* Validate LBA range against the simulated capacity. */
    end = lba + (uint64_t)count;
    if (lba >= dev->total_sectors || end > dev->total_sectors) return -1;
    if (end > (uint64_t)SIM_SECTORS) return -1;

    bytes = count * dev->sector_size;
    memcpy(buf, &g_disk_storage[device_id][lba * dev->sector_size], bytes);
    return (int)bytes;
}

int disk_write(uint32_t device_id, uint64_t lba, const void* buf, uint32_t count) {
    DISK_DEVICE* dev;
    uint64_t end;
    uint32_t bytes;

    if (buf == NULL || count == 0) return -1;
    dev = disk_lookup(device_id);
    if (dev == NULL) return -1;

    end = lba + (uint64_t)count;
    if (lba >= dev->total_sectors || end > dev->total_sectors) return -1;
    if (end > (uint64_t)SIM_SECTORS) return -1;

    bytes = count * dev->sector_size;
    memcpy(&g_disk_storage[device_id][lba * dev->sector_size], buf, bytes);

    /* If sector 0 was written, refresh the cached MBR and partition table. */
    if (lba == 0) {
        disk_parse_mbr(dev);
    }
    return (int)bytes;
}

int disk_get_geometry(uint32_t device_id, DISK_GEOMETRY* geo) {
    DISK_DEVICE* dev;
    if (geo == NULL) return -1;
    dev = disk_lookup(device_id);
    if (dev == NULL) return -1;
    *geo = dev->geometry;
    return 0;
}

int disk_get_partition_info(uint32_t device_id, int part_idx,
                            PARTITION_ENTRY* entry) {
    DISK_DEVICE* dev;
    if (entry == NULL) return -1;
    dev = disk_lookup(device_id);
    if (dev == NULL) return -1;
    if (part_idx < 0 || part_idx >= MBR_PART_COUNT) return -1;
    if (part_idx >= dev->num_partitions) return -1;
    *entry = dev->partitions[part_idx];
    return 0;
}

int disk_enumerate(DISK_DEVICE* devices, int max_count) {
    int i;
    int copied = 0;
    if (devices == NULL || max_count <= 0) return 0;
    for (i = 0; i < DISK_MAX_DEVICES && copied < max_count; i++) {
        if (!g_disk_pool[i].is_online) continue;
        devices[copied] = g_disk_pool[i];
        copied++;
    }
    return copied;
}
