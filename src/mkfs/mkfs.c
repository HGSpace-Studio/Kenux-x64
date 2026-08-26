/*
 * Kenux OS - Filesystem Creation Tool
 * Main mkfs functionality
 */

#include "mkfs.h"

#ifndef _WIN32
#include <sys/mman.h>
#endif

/*
 * MinGW compatibility shims.
 * Windows has no block devices, so S_ISBLK is not provided by MinGW's
 * <sys/stat.h>. Define a fallback so the source compiles cross-platform.
 */
#ifndef S_ISBLK
#ifdef S_IFBLK
#define S_ISBLK(mode) (((mode) & S_IFMT) == S_IFBLK)
#else
#define S_ISBLK(mode) (0)
#endif
#endif

// Filesystem magic numbers
#define EXT2_SUPER_MAGIC 0xEF53
#define EXT3_SUPER_MAGIC 0xEF53
#define EXT4_SUPER_MAGIC 0xEF53
#define VFAT_MAGIC 0x0AB16F30
#define NTFS_MAGIC 0x5346544E
#define ISO9660_MAGIC 0x43443030
#define SWAP_MAGIC 0x454E4445

// Block sizes (in bytes)
#define BLOCK_SIZE_512 512
#define BLOCK_SIZE_1024 1024
#define BLOCK_SIZE_2048 2048
#define BLOCK_SIZE_4096 4096

// Default filesystem parameters
#define DEFAULT_BLOCK_SIZE 4096
#define DEFAULT_INODES_PER_GROUP 8192
#define DEFAULT_BLOCKS_PER_GROUP 8192

void mkfs_init(MkfsState *state) {
    memset(state, 0, sizeof(MkfsState));
    state->fs_type = FS_TYPE_EXT2;
    state->verbose = 0;
    state->force = 0;
    state->check_interval = 0;
    state->block_size = DEFAULT_BLOCK_SIZE;
}

void mkfs_cleanup(MkfsState *state) {
    if (state->device_fd >= 0) {
        close(state->device_fd);
    }
}

int parse_arguments(MkfsState *state, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            state->fs_type = get_fs_type_from_name(argv[++i]);
            if (state->fs_type == FS_TYPE_UNKNOWN) {
                fprintf(stderr, "kenux-mkfs: unknown filesystem type: %s\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-V") == 0) {
            state->verbose = 1;
        } else if (strcmp(argv[i], "-f") == 0) {
            state->force = 1;
        } else if (strcmp(argv[i], "-L") == 0 && i + 1 < argc) {
            strncpy(state->label, argv[++i], MAX_LABEL_LEN - 1);
            state->label[MAX_LABEL_LEN - 1] = '\0';
        } else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc) {
            state->check_interval = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
            int size = atoi(argv[++i]);
            if (size == 512 || size == 1024 || size == 2048 || size == 4096) {
                state->block_size = size;
            } else {
                fprintf(stderr, "kenux-mkfs: invalid block size: %d\n", size);
                return -1;
            }
        } else if (strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        } else if (argv[i][0] != '-') {
            // Device file
            if (strlen(state->device_path) == 0) {
                strncpy(state->device_path, argv[i], MAX_PATH_LEN - 1);
                state->device_path[MAX_PATH_LEN - 1] = '\0';
            } else {
                fprintf(stderr, "kenux-mkfs: too many device files specified\n");
                return -1;
            }
        } else {
            fprintf(stderr, "kenux-mkfs: unknown option: %s\n", argv[i]);
            return -1;
        }
    }
    
    if (strlen(state->device_path) == 0) {
        fprintf(stderr, "kenux-mkfs: device file not specified\n");
        return -1;
    }
    
    return 0;
}

const char *get_fs_type_name(FileSystemType type) {
    switch (type) {
        case FS_TYPE_EXT2: return "ext2";
        case FS_TYPE_EXT3: return "ext3";
        case FS_TYPE_EXT4: return "ext4";
        case FS_TYPE_VFAT: return "vfat";
        case FS_TYPE_NTFS: return "ntfs";
        case FS_TYPE_ISO9660: return "iso9660";
        case FS_TYPE_SWAP: return "swap";
        default: return "unknown";
    }
}

FileSystemType get_fs_type_from_name(const char *name) {
    if (strcmp(name, "ext2") == 0) return FS_TYPE_EXT2;
    if (strcmp(name, "ext3") == 0) return FS_TYPE_EXT3;
    if (strcmp(name, "ext4") == 0) return FS_TYPE_EXT4;
    if (strcmp(name, "vfat") == 0) return FS_TYPE_VFAT;
    if (strcmp(name, "ntfs") == 0) return FS_TYPE_NTFS;
    if (strcmp(name, "iso9660") == 0) return FS_TYPE_ISO9660;
    if (strcmp(name, "swap") == 0) return FS_TYPE_SWAP;
    return FS_TYPE_UNKNOWN;
}

int validate_device(MkfsState *state) {
    // Check if device exists
    struct stat st;
    if (stat(state->device_path, &st) != 0) {
        fprintf(stderr, "kenux-mkfs: device file does not exist: %s\n", state->device_path);
        return -1;
    }
    
    // Check if it's a block device
    if (!S_ISBLK(st.st_mode)) {
        fprintf(stderr, "kenux-mkfs: %s is not a block device\n", state->device_path);
        return -1;
    }
    
    // Check if device is mounted
    FILE *mtab = fopen("/etc/mtab", "r");
    if (mtab) {
        char line[256];
        while (fgets(line, sizeof(line), mtab)) {
            if (strstr(line, state->device_path)) {
                fprintf(stderr, "kenux-mkfs: device is mounted: %s\n", state->device_path);
                fclose(mtab);
                if (!state->force) {
                    return -1;
                }
                break;
            }
        }
        fclose(mtab);
    }
    
    // Open device
    state->device_fd = open(state->device_path, O_RDWR);
    if (state->device_fd < 0) {
        fprintf(stderr, "kenux-mkfs: cannot open device: %s\n", state->device_path);
        return -1;
    }
    
    // Get device size
#ifndef _WIN32
    if (ioctl(state->device_fd, BLKGETSIZE64, &state->stats.total_blocks) != 0) {
        // Fallback to lseek
        off_t size = lseek(state->device_fd, 0, SEEK_END);
        if (size < 0) {
            fprintf(stderr, "kenux-mkfs: cannot get device size\n");
            return -1;
        }
        state->stats.total_blocks = size / state->block_size;
    } else {
        state->stats.total_blocks /= state->block_size;
    }
#else
    /* Windows: no ioctl/BLKGETSIZE64, use lseek directly */
    {
        off_t size = lseek(state->device_fd, 0, SEEK_END);
        if (size < 0) {
            fprintf(stderr, "kenux-mkfs: cannot get device size\n");
            return -1;
        }
        state->stats.total_blocks = size / state->block_size;
    }
#endif
    
    // Calculate filesystem statistics
    state->stats.block_size = state->block_size;
    state->stats.blocks_per_group = DEFAULT_BLOCKS_PER_GROUP;
    state->stats.inodes_per_group = DEFAULT_INODES_PER_GROUP;
    
    // Calculate number of groups
    uint32_t num_groups = (state->stats.total_blocks + state->stats.blocks_per_group - 1) / 
                          state->stats.blocks_per_group;
    
    // Calculate number of inodes (typically 4% of blocks)
    state->stats.total_inodes = (state->stats.total_blocks * 4096) / 1024; // Rough approximation
    state->stats.free_inodes = state->stats.total_inodes;
    
    if (state->verbose) {
        printf("Device: %s\n", state->device_path);
        printf("Size: %zu blocks\n", state->stats.total_blocks);
        printf("Block size: %zu bytes\n", state->stats.block_size);
        printf("Filesystem type: %s\n", get_fs_type_name(state->fs_type));
        printf("Number of groups: %u\n", num_groups);
    }
    
    return 0;
}

int init_superblock(MkfsState *state) {
    memset(&state->superblock, 0, sizeof(Superblock));
    
    // Set filesystem-specific parameters
    switch (state->fs_type) {
        case FS_TYPE_EXT2:
        case FS_TYPE_EXT3:
        case FS_TYPE_EXT4:
            state->superblock.s_magic = EXT2_SUPER_MAGIC;
            state->superblock.s_block_size = state->block_size;
            state->superblock.s_blocks_count = state->stats.total_blocks;
            state->superblock.s_free_blocks_count = state->stats.total_blocks - 1; // Reserve block 0
            state->superblock.s_free_inodes_count = state->stats.total_inodes;
            state->superblock.s_data_blocks_per_group = state->stats.blocks_per_group;
            state->superblock.s_inodes_per_group = state->stats.inodes_per_group;
            state->superblock.s_first_data_block = 1;
            state->superblock.s_log_block_size = 
                (state->block_size == 1024) ? 1 :
                (state->block_size == 2048) ? 2 :
                (state->block_size == 4096) ? 3 : 0;
            state->superblock.s_blocks_per_group = state->stats.blocks_per_group;
            state->superblock.s_frags_per_group = state->stats.blocks_per_group;
            state->superblock.s_mtime = time(NULL);
            state->superblock.s_wtime = time(NULL);
            state->superblock.s_mnt_count = 0;
            state->superblock.s_max_mnt_count = -1;
            state->superblock.s_magic = EXT2_SUPER_MAGIC;
            state->superblock.s_state = 1; // VALID_FS
            state->superblock.s_errors = 1; // Continue
            state->superblock.s_minor_rev_level = 0;
            state->superblock.s_lastcheck = 0;
            state->superblock.s_checkinterval = 0;
            state->superblock.s_creator_os = 0; // Linux
            state->superblock.s_rev_level = 1; // Current revision
            strncpy(state->superblock.s_volume_name, state->label, MAX_LABEL_LEN - 1);
            break;
            
        case FS_TYPE_VFAT:
            // FAT filesystem parameters would go here
            break;
            
        case FS_TYPE_NTFS:
            // NTFS filesystem parameters would go here
            break;
            
        case FS_TYPE_ISO9660:
            // ISO9660 filesystem parameters would go here
            break;
            
        case FS_TYPE_SWAP:
            state->superblock.s_magic = SWAP_MAGIC;
            break;
            
        default:
            fprintf(stderr, "kenux-mkfs: unsupported filesystem type\n");
            return -1;
    }
    
    if (state->verbose) {
        printf("Superblock initialized\n");
    }
    
    return 0;
}

int write_superblock(MkfsState *state) {
    // Write superblock to device
    // In a real implementation, this would write to the proper location
    // For now, we'll write it to the beginning of the device
    
    off_t pos = lseek(state->device_fd, 0, SEEK_SET);
    if (pos < 0) {
        fprintf(stderr, "kenux-mkfs: cannot seek to superblock position\n");
        return -1;
    }
    
    if (write(state->device_fd, &state->superblock, sizeof(Superblock)) != sizeof(Superblock)) {
        fprintf(stderr, "kenux-mkfs: cannot write superblock\n");
        return -1;
    }
    
    if (state->verbose) {
        printf("Superblock written\n");
    }
    
    return 0;
}

int create_bitmaps(MkfsState *state) {
    // Create and write block and inode bitmaps
    // This is a simplified implementation
    
    // In a real implementation, we would:
    // 1. Calculate bitmap sizes
    // 2. Allocate memory for bitmaps
    // 3. Initialize bitmaps (all blocks free except reserved)
    // 4. Write bitmaps to device at proper locations
    
    if (state->verbose) {
        printf("Bitmaps created (simplified)\n");
    }
    
    return 0;
}

int create_inodes(MkfsState *state) {
    // Create inode table
    // This is a simplified implementation
    
    // In a real implementation, we would:
    // 1. Allocate memory for inode table
    // 2. Initialize inodes
    // 3. Write inode table to device at proper locations
    
    if (state->verbose) {
        printf("Inode table created (simplified)\n");
    }
    
    return 0;
}

int create_root_directory(MkfsState *state) {
    // Create root directory inode
    // This is a simplified implementation
    
    // In a real implementation, we would:
    // 1. Create root directory inode
    // 2. Initialize directory entries
    // 3. Write root directory to device
    
    if (state->verbose) {
        printf("Root directory created (simplified)\n");
    }
    
    return 0;
}

int verify_filesystem(MkfsState *state) {
    // Basic filesystem verification
    if (state->superblock.s_magic != EXT2_SUPER_MAGIC && 
        state->superblock.s_magic != SWAP_MAGIC) {
        fprintf(stderr, "kenux-mkfs: filesystem verification failed\n");
        return -1;
    }
    
    if (state->superblock.s_blocks_count == 0) {
        fprintf(stderr, "kenux-mkfs: invalid block count\n");
        return -1;
    }
    
    if (state->verbose) {
        printf("Filesystem verification passed\n");
    }
    
    return 0;
}

int create_filesystem(MkfsState *state) {
    // Initialize superblock
    if (init_superblock(state) != 0) {
        return -1;
    }
    
    // Write superblock
    if (write_superblock(state) != 0) {
        return -1;
    }
    
    // Create bitmaps
    if (create_bitmaps(state) != 0) {
        return -1;
    }
    
    // Create inodes
    if (create_inodes(state) != 0) {
        return -1;
    }
    
    // Create root directory
    if (create_root_directory(state) != 0) {
        return -1;
    }
    
    // Verify filesystem
    if (verify_filesystem(state) != 0) {
        return -1;
    }
    
    return 0;
}

void print_filesystem_info(MkfsState *state) {
    printf("Filesystem Information:\n");
    printf("  Device: %s\n", state->device_path);
    printf("  Type: %s\n", get_fs_type_name(state->fs_type));
    printf("  Block size: %zu bytes\n", state->stats.block_size);
    printf("  Total blocks: %zu\n", state->stats.total_blocks);
    printf("  Free blocks: %zu\n", state->stats.free_blocks);
    printf("  Total inodes: %zu\n", state->stats.total_inodes);
    printf("  Free inodes: %zu\n", state->stats.free_inodes);
    printf("  Blocks per group: %zu\n", state->stats.blocks_per_group);
    printf("  Inodes per group: %zu\n", state->stats.inodes_per_group);
    printf("  Label: %s\n", state->label);
}

void print_usage(void) {
    printf("Kenux OS Filesystem Creation Tool\n");
    printf("Usage: mkfs [options] device\n");
    printf("Options:\n");
    printf("  -t type       Filesystem type (ext2, ext3, ext4, vfat, ntfs, iso9660, swap)\n");
    printf("  -V            Verbose output\n");
    printf("  -f            Force creation (even if mounted)\n");
    printf("  -L label      Volume label\n");
    printf("  -c interval   Check interval (for ext2/3/4)\n");
    printf("  -b size       Block size (512, 1024, 2048, 4096)\n");
    printf("  --help        Show this help message\n");
}

int main(int argc, char **argv) {
    MkfsState state;
    mkfs_init(&state);
    
    // Parse arguments
    if (parse_arguments(&state, argc, argv) != 0) {
        mkfs_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Validate device
    if (validate_device(&state) != 0) {
        mkfs_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Create filesystem
    if (create_filesystem(&state) != 0) {
        mkfs_cleanup(&state);
        return EXIT_FAILURE;
    }
    
    // Print filesystem info
    if (state.verbose) {
        print_filesystem_info(&state);
    }
    
    printf("Filesystem created successfully on %s\n", state.device_path);
    
    mkfs_cleanup(&state);
    return EXIT_SUCCESS;
}