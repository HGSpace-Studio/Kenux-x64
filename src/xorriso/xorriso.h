/*
 * Kenux OS - Xorriso ISO 9660 Image Tool (Minimal)
 * Header file for ISO 9660 / Rock Ridge / El Torito creation
 */

#ifndef _XORRISO_H
#define _XORRISO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>

#ifdef _WIN32
/* MinGW 兼容 */
typedef int uid_t;
typedef int gid_t;
#include <unistd.h>
#include <direct.h>
#include <io.h>
#include <windows.h>
#include <time.h>
#define mkdir(path,mode) _mkdir(path)
/* MinGW 没有 lstat，使用 stat 替代 */
#define lstat(path,st) stat(path,st)
/* MinGW 没有 S_ISLNK 宏，文件系统不支持符号链接 */
#define S_ISLNK(m) 0
/* MinGW 没有 readlink，提供 stub 返回 -1 */
static int readlink(const char *path, char *buf, size_t bufsiz) {
    (void)path; (void)buf; (void)bufsiz;
    return -1;
}
/* dirent 替代 */
typedef struct DIR DIR;
struct dirent { char d_name[260]; };
static DIR *opendir(const char *name) { (void)name; return NULL; }
static struct dirent *readdir(DIR *d) { (void)d; return NULL; }
static int closedir(DIR *d) { (void)d; return -1; }
#else
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#endif

#define XORRISO_VERSION "KenuxK-xorriso 1.5.6 (Minimal)"

#define XORRISO_MAX_PATH 1024
#define XORRISO_MAX_ENTRIES 4096
#define XORRISO_SECTOR_SIZE 2048
#define XORRISO_BLOCK_SIZE 2048
#define XORRISO_VOL_ID_LEN 32
#define XORRISO_SYSTEM_ID_LEN 32
#define XORRISO_PUBLISHER_LEN 128
#define XORRISO_PREPARER_LEN 128
#define XORRISO_APPLICATION_LEN 128
#define XORRISO_COPYRIGHT_LEN 256
#define XORRISO_ABSTRACT_LEN 256
#define XORRISO_BIBLIO_LEN 256

/* ISO 9660 volume descriptor types */
#define ISO_VD_BOOT 0
#define ISO_VD_PRIMARY 1
#define ISO_VD_SUPPLEMENTARY 2
#define ISO_VD_PARTITION 3
#define ISO_VD_SET_TERMINATOR 255

/* ISO 9660 file flags */
#define ISO_FLAG_HIDDEN (1 << 0)
#define ISO_FLAG_DIRECTORY (1 << 1)
#define ISO_FLAG_ASSOCIATED (1 << 2)
#define ISO_FLAG_RECORD (1 << 3)
#define ISO_FLAG_PROTECTION (1 << 4)
#define ISO_FLAG_MULTIEXTENT (1 << 7)

/* Rock Ridge extensions signatures */
#define RR_SUSP_SIG "SP"
#define RR_PX_SIG "PX"
#define RR_PN_SIG "PN"
#define RR_SL_SIG "SL"
#define RR_NM_SIG "NM"
#define RR_CL_SIG "CL"
#define RR_PL_SIG "PL"
#define RR_RE_SIG "RE"
#define RR_TF_SIG "TF"

/* El Torito */
#define ET_BOOT_CATALOG_SECTOR 0x11
#define ET_ENTRY_VALID 0x88
#define ET_BOOTABLE 0x88
#define ET_NOEMUL 0
#define ET_12FLOPPY 1
#define ET_288FLOPPY 2
#define ET_HARDDISK 3
#define ET_X86_BIOS 0x00
#define ET_EFI 0xEF

/* Output formats */
typedef enum {
    XORRISO_FMT_ISO,          /* Plain ISO 9660 */
    XORRISO_FMT_ROCKRIDGE,    /* ISO + Rock Ridge (POSIX) */
    XORRISO_FMT_JOLIET,       /* ISO + Joliet (Unicode) */
    XORRISO_FMT_HYBRID,       /* ISO + MBR hybrid (ISO/USB dual) */
    XORRISO_FMT_UDF,          /* UDF only */
    XORRISO_FMT_ISO_UDF,      /* ISO + UDF bridge */
} XorrisoFormat;

/* Boot mode */
typedef enum {
    XORRISO_BOOT_NONE = 0,
    XORRISO_BOOT_ELTORITO_BIOS,   /* Legacy BIOS via El Torito no-emul */
    XORRISO_BOOT_ELTORITO_EFI,    /* EFI via El Torito FAT partition */
    XORRISO_BOOT_HYBRID_BOTH,     /* BIOS + EFI */
    XORRISO_BOOT_GRUB_ELTORITO,   /* Grub2 multi-boot */
} XorrisoBootMode;

/* File entry in source tree */
typedef struct XorrisoFileEntry {
    char path[XORRISO_MAX_PATH];
    char iso_path[XORRISO_MAX_PATH];   /* path inside ISO */
    char name[256];
    struct XorrisoFileEntry *parent;
    struct XorrisoFileEntry **children;
    int child_count;
    int child_cap;
    uint64_t size;
    uint32_t lba;              /* starting logical block */
    time_t mtime;
    mode_t mode;
    uid_t uid;
    gid_t gid;
    int is_dir;
    int is_symlink;
    char symlink_target[XORRISO_MAX_PATH];
    uint8_t iso_flags;
    /* Rock Ridge attributes */
    uint32_t rr_mode;
    uint32_t rr_nlinks;
    uint32_t rr_uid;
    uint32_t rr_gid;
    uint64_t rr_ino;
    char rr_name[256];
    struct XorrisoFileEntry *rr_cl;  /* relocated child */
    struct XorrisoFileEntry *rr_pl;  /* relocated parent */
} XorrisoFileEntry;

/* El Torito boot catalog */
typedef struct {
    uint8_t  header_id;          /* 0x01 */
    uint8_t  platform;           /* 0x00 x86, 0xEF EFI */
    uint16_t reserved1;
    char     id_string[24];
    uint16_t checksum;
    uint8_t  key_55;
    uint8_t  key_AA;
    /* Entry 1 (Initial/Default) */
    uint8_t  boot_indicator;     /* 0x88 = bootable */
    uint8_t  boot_media_type;    /* 0 = no emul, 1=1.2MB, 2=1.44MB, 3=2.88MB, 4=hd */
    uint16_t load_segment;
    uint8_t  system_type;
    uint8_t  unused1;
    uint16_t sector_count;
    uint32_t load_lba;           /* LBA of boot image */
    uint8_t  unused2[4];
} XorrisoBootCatalog;

/* Volume metadata */
typedef struct {
    char volume_id[XORRISO_VOL_ID_LEN + 1];
    char volume_set_id[128];
    char system_id[XORRISO_SYSTEM_ID_LEN + 1];
    char publisher[XORRISO_PUBLISHER_LEN + 1];
    char preparer[XORRISO_PREPARER_LEN + 1];
    char application[XORRISO_APPLICATION_LEN + 1];
    char copyright[XORRISO_COPYRIGHT_LEN + 1];
    char abstract_file[XORRISO_ABSTRACT_LEN + 1];
    char bibliographic[XORRISO_BIBLIO_LEN + 1];
    uint32_t volume_space_size;   /* total sectors */
    uint16_t volume_set_size;
    uint16_t volume_sequence_number;
    uint16_t logical_block_size;
    uint8_t  file_structure_version;
    time_t   creation_time;
    time_t   modify_time;
    time_t   expire_time;
    time_t   effective_time;
} XorrisoVolumeInfo;

/* Operation mode */
typedef enum {
    XORRISO_OP_CREATE,       /* -as mkisofs: create new ISO */
    XORRISO_OP_EXTRACT,      /* Extract files from ISO */
    XORRISO_OP_LIST,         /* List files in ISO */
    XORRISO_OP_VERIFY,       /* Verify ISO integrity */
    XORRISO_OP_BLIND_WRITE,  /* Write ISO to block device */
    XORRISO_OP_DELTA,        /* Compare two ISOs */
} XorrisoOpMode;

/* Global state */
typedef struct {
    XorrisoOpMode mode;
    XorrisoFormat format;
    XorrisoBootMode boot_mode;

    char output_file[XORRISO_MAX_PATH];
    char source_dir[XORRISO_MAX_PATH];
    char mount_point[XORRISO_MAX_PATH];

    /* source tree root */
    XorrisoFileEntry *root;
    int total_entries;
    uint64_t total_bytes;

    /* Boot configuration */
    char boot_image[XORRISO_MAX_PATH];   /* e.g. boot/grub/i386-pc/eltorito.img */
    char efi_boot_image[XORRISO_MAX_PATH]; /* e.g. boot/grub/efi.img */
    char boot_catalog[XORRISO_MAX_PATH];
    uint32_t boot_load_size;       /* sectors for El Torito */
    uint32_t boot_load_seg;        /* 0x07C0 typical */
    int boot_noemul;
    int boot_info_table;           /* add boot info table (Grub) */
    int grub2_boot;                /* Enable grub2 --embedded-area emulation */
    int hybrid_mbr;                /* Create isohybrid MBR */
    uint32_t mbr_id;               /* MBR disk signature */

    /* Platform */
    int platform_x86_bios;
    int platform_efi_ia32;
    int platform_efi_x64;
    int platform_efi_arm64;

    /* Volume info */
    XorrisoVolumeInfo vol;

    /* Rock Ridge / Joliet */
    int enable_rock_ridge;
    int enable_joliet;
    int joliet_unicode_level;
    int enable_trans_tbl;      /* TRANS.TBL */

    /* HFS / UDF (stubs) */
    int enable_hfs;
    int enable_udf;
    char hfs_vol_icon[XORRISO_MAX_PATH];

    /* Exclusions */
    char exclude_patterns[32][256];
    int exclude_count;
    int follow_symlinks;
    int preserve_permissions;
    int preserve_timestamps;

    /* Graft points (source=target pairs) */
    char graft_sources[32][XORRISO_MAX_PATH];
    char graft_targets[32][XORRISO_MAX_PATH];
    int graft_count;

    /* Runtime status */
    int verbose;
    int quiet;
    int dry_run;
    int progress;
    int force;
    int overwrite;
    size_t pad_blocks;

    /* Output buffers */
    uint8_t *sector_buf;
    size_t  sector_buf_size;
    FILE *out_fp;
    off_t  out_offset;
    uint32_t next_lba;

} XorrisoState;

/* API prototypes */
void xorriso_init(XorrisoState *state);
void xorriso_cleanup(XorrisoState *state);
int xorriso_parse_arguments(XorrisoState *state, int argc, char **argv);

/* File tree walking */
XorrisoFileEntry *xorriso_new_entry(const char *path, int is_dir);
void xorriso_free_entry(XorrisoFileEntry *e);
int xorriso_add_child(XorrisoFileEntry *parent, XorrisoFileEntry *child);
int xorriso_walk_tree(XorrisoState *state, const char *root_path,
                      const char *iso_prefix, XorrisoFileEntry *parent);
int xorriso_apply_grafts(XorrisoState *state);
int xorriso_resolve_symlinks(XorrisoState *state);
int xorriso_compute_sizes(XorrisoState *state, XorrisoFileEntry *root);
int xorriso_assign_lbas(XorrisoState *state, XorrisoFileEntry *root, uint32_t *lba);

/* ISO 9660 writer */
int xorriso_write_volume_descriptors(XorrisoState *state);
int xorriso_write_path_table(XorrisoState *state);
int xorriso_write_directory_records(XorrisoState *state, XorrisoFileEntry *dir, uint32_t *lba);
int xorriso_write_file_data(XorrisoState *state, XorrisoFileEntry *f);
int xorriso_write_boot_catalog(XorrisoState *state);
int xorriso_write_hybrid_mbr(XorrisoState *state);
int xorriso_pad_output(XorrisoState *state);

/* ISO 9660 reader (for list/extract) */
int xorriso_read_volume_descriptors(XorrisoState *state, const char *iso_file);
int xorriso_list_files(XorrisoState *state, const char *iso_file);
int xorriso_extract_all(XorrisoState *state, const char *iso_file, const char *dest);
int xorriso_verify_checksum(XorrisoState *state, const char *iso_file);

/* Utilities */
uint16_t xorriso_compute_boot_catalog_checksum(const XorrisoBootCatalog *bc);
void xorriso_set_iso_datetime(uint8_t *buf, time_t t);
void xorriso_str_to_dchar(char *dst, const char *src, size_t dst_size);
void xorriso_str_to_joliet(uint16_t *dst, const char *src, size_t count);
int xorriso_match_exclude(XorrisoState *state, const char *path);
uint32_t xorriso_crc32(const uint8_t *data, size_t len);
void xorriso_print_help(void);
void xorriso_print_version(void);

#endif /* _XORRISO_H */
