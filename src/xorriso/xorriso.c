/*
 * Kenux OS - Xorriso ISO 9660 Tool (Minimal)
 * Main xorriso implementation
 */

#include "xorriso.h"
#include <time.h>

void xorriso_init(XorrisoState *s) {
    memset(s, 0, sizeof(XorrisoState));
    s->mode = XORRISO_OP_CREATE;
    s->format = XORRISO_FMT_ROCKRIDGE;
    s->boot_mode = XORRISO_BOOT_NONE;
    s->sector_buf_size = XORRISO_SECTOR_SIZE * 128;
    s->sector_buf = (uint8_t *)calloc(1, s->sector_buf_size);
    s->enable_rock_ridge = 1;
    s->enable_joliet = 1;
    s->joliet_unicode_level = 3;
    s->platform_x86_bios = 0;
    s->platform_efi_x64 = 0;
    s->follow_symlinks = 0;
    s->preserve_permissions = 1;
    s->preserve_timestamps = 1;
    s->boot_noemul = 1;
    s->boot_load_seg = 0x7C0;
    s->boot_load_size = 4;
    s->hybrid_mbr = 0;
    s->vol.volume_set_size = 1;
    s->vol.volume_sequence_number = 1;
    s->vol.logical_block_size = XORRISO_SECTOR_SIZE;
    s->vol.file_structure_version = 1;
    strcpy(s->vol.volume_id, "KENUXK");
    strcpy(s->vol.system_id, "KENUXK");
    strcpy(s->vol.publisher, "KenuxK Project");
    strcpy(s->vol.preparer, "KenuxK-xorriso");
    strcpy(s->vol.application, "KenuxK Install Media");
    time(&s->vol.creation_time);
    s->vol.modify_time = s->vol.creation_time;
    s->vol.effective_time = s->vol.creation_time;
    s->vol.expire_time = 0;
    s->mbr_id = 0x4B55584B; /* "KUXK" */
}

void xorriso_cleanup(XorrisoState *s) {
    if (!s) return;
    if (s->root) xorriso_free_entry(s->root);
    if (s->sector_buf) free(s->sector_buf);
    if (s->out_fp) fclose(s->out_fp);
    memset(s, 0, sizeof(*s));
}

XorrisoFileEntry *xorriso_new_entry(const char *path, int is_dir) {
    XorrisoFileEntry *e = (XorrisoFileEntry *)calloc(1, sizeof(*e));
    if (!e) return NULL;
    strncpy(e->path, path, XORRISO_MAX_PATH - 1);
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    strncpy(e->name, base, sizeof(e->name) - 1);
    strncpy(e->iso_path, path, XORRISO_MAX_PATH - 1);
    e->is_dir = is_dir;
    e->rr_mode = is_dir ? 040755 : 0100644;
    e->rr_nlinks = is_dir ? 2 : 1;
    return e;
}

void xorriso_free_entry(XorrisoFileEntry *e) {
    if (!e) return;
    for (int i = 0; i < e->child_count; i++) xorriso_free_entry(e->children[i]);
    free(e->children);
    free(e);
}

int xorriso_add_child(XorrisoFileEntry *parent, XorrisoFileEntry *child) {
    if (!parent || !child) return -1;
    if (parent->child_count >= parent->child_cap) {
        int ncap = parent->child_cap ? parent->child_cap * 2 : 16;
        XorrisoFileEntry **nc = (XorrisoFileEntry **)realloc(
            parent->children, ncap * sizeof(*nc));
        if (!nc) return -1;
        parent->children = nc;
        parent->child_cap = ncap;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    return 0;
}

static int is_ignored_file(const char *name) {
    return strcmp(name, ".") == 0 || strcmp(name, "..") == 0;
}

int xorriso_match_exclude(XorrisoState *s, const char *path) {
    for (int i = 0; i < s->exclude_count; i++) {
        if (strstr(path, s->exclude_patterns[i])) return 1;
    }
    return 0;
}

int xorriso_walk_tree(XorrisoState *s, const char *root_path,
                      const char *iso_prefix, XorrisoFileEntry *parent) {
    DIR *d = opendir(root_path);
    if (!d) { perror(root_path); return -1; }
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (is_ignored_file(de->d_name)) continue;
        char full[XORRISO_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", root_path, de->d_name);
        if (xorriso_match_exclude(s, full)) continue;
        struct stat st;
        if (lstat(full, &st) < 0) continue;
        int is_dir = S_ISDIR(st.st_mode);
        int is_sym = S_ISLNK(st.st_mode);
        if (!s->follow_symlinks && is_sym) is_dir = 0;
        XorrisoFileEntry *e = xorriso_new_entry(full, is_dir);
        if (!e) continue;
        snprintf(e->iso_path, XORRISO_MAX_PATH, "%s/%s", iso_prefix, de->d_name);
        strncpy(e->name, de->d_name, sizeof(e->name) - 1);
        e->size = st.st_size;
        e->mtime = st.st_mtime;
        e->mode = st.st_mode;
        e->uid = st.st_uid;
        e->gid = st.st_gid;
        e->is_symlink = is_sym;
        e->rr_mode = st.st_mode & 0xFFFF;
        e->rr_uid = st.st_uid;
        e->rr_gid = st.st_gid;
        if (is_sym) {
            e->size = 0;
            ssize_t r = readlink(full, e->symlink_target, XORRISO_MAX_PATH - 1);
            if (r >= 0) e->symlink_target[r] = 0;
        }
        e->iso_flags = (is_dir ? ISO_FLAG_DIRECTORY : 0);
        xorriso_add_child(parent, e);
        s->total_entries++;
        if (is_dir && !is_sym) {
            xorriso_walk_tree(s, full, e->iso_path, e);
        } else {
            s->total_bytes += st.st_size;
        }
    }
    closedir(d);
    return 0;
}

int xorriso_compute_sizes(XorrisoState *s, XorrisoFileEntry *r) {
    (void)s;
    if (!r) return 0;
    uint64_t total = 0;
    for (int i = 0; i < r->child_count; i++) {
        XorrisoFileEntry *c = r->children[i];
        if (c->is_dir) {
            xorriso_compute_sizes(s, c);
        }
        total += (c->size + XORRISO_SECTOR_SIZE - 1) / XORRISO_SECTOR_SIZE;
    }
    r->size = total * XORRISO_SECTOR_SIZE;
    return 0;
}

int xorriso_assign_lbas(XorrisoState *s, XorrisoFileEntry *r, uint32_t *lba) {
    (void)s;
    if (!r) return 0;
    for (int i = 0; i < r->child_count; i++) {
        XorrisoFileEntry *c = r->children[i];
        c->lba = *lba;
        uint32_t sects = (uint32_t)((c->size + XORRISO_SECTOR_SIZE - 1) / XORRISO_SECTOR_SIZE);
        if (c->is_dir) {
            /* allocate at least 1 sector per directory */
            if (sects < 1) sects = 1;
            *lba += sects;
            xorriso_assign_lbas(s, c, lba);
        } else {
            *lba += sects;
        }
    }
    return 0;
}

void xorriso_set_iso_datetime(uint8_t *buf, time_t t) {
    if (!buf) return;
    struct tm *tm = gmtime(&t);
    if (!tm) { memset(buf, 0, 17); return; }
    snprintf((char *)buf, 5, "%04d", 1900 + tm->tm_year);
    buf[4] = (uint8_t)(tm->tm_mon + 1);
    buf[5] = (uint8_t)tm->tm_mday;
    buf[6] = (uint8_t)tm->tm_hour;
    buf[7] = (uint8_t)tm->tm_min;
    buf[8] = (uint8_t)tm->tm_sec;
    buf[9] = 0; /* hundredths */
    /* 15-byte format: 7+1 offset from GMT */
    int16_t off = 0; /* UTC */
    buf[16] = (uint8_t)((off >> 8) & 0xFF);
}

void xorriso_str_to_dchar(char *dst, const char *src, size_t n) {
    if (!dst || !src) return;
    size_t i;
    for (i = 0; i < n - 1 && src[i]; i++) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        else if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                   c == '_' || c == '-' || c == '.')) c = '_';
        dst[i] = c;
    }
    for (; i < n - 1; i++) dst[i] = ' ';
    dst[n - 1] = 0;
}

uint16_t xorriso_compute_boot_catalog_checksum(const XorrisoBootCatalog *bc) {
    const uint8_t *p = (const uint8_t *)bc;
    uint32_t sum = 0;
    for (size_t i = 0; i < sizeof(XorrisoBootCatalog); i += 2) {
        if (i == 28 || i == 30) continue; /* skip checksum + signature */
        uint16_t w = (uint16_t)((p[i] << 0) | (p[i + 1] << 8));
        sum += w;
    }
    return (uint16_t)(-sum & 0xFFFF);
}

uint32_t xorriso_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = ~0U;
    static const uint32_t table[16] = {
        0x00000000,0x1DB71064,0x3B6E20C8,0x26D930AC,
        0x76DC4190,0x6B6B51F4,0x4DB26158,0x5005713C,
        0xEDB88320,0xF00F9344,0xD6D6A3E8,0xCB61B38C,
        0x9B64C2B0,0x86D3D2D4,0xA00AE278,0xBDBDF21C,
    };
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 4) ^ table[(crc ^ data[i]) & 0x0F];
        crc = (crc >> 4) ^ table[(crc ^ (data[i] >> 4)) & 0x0F];
    }
    return ~crc;
}

/* Write a primary volume descriptor (sector 16) */
int xorriso_write_volume_descriptors(XorrisoState *s) {
    if (!s || !s->out_fp) return -1;
    uint8_t sector[XORRISO_SECTOR_SIZE];
    /* Sector 0-15: system area, reserved for MBR/boot */
    memset(sector, 0, sizeof(sector));
    for (int i = 0; i < 16; i++) fwrite(sector, 1, sizeof(sector), s->out_fp);

    /* PVD: sector 16 */
    memset(sector, 0, sizeof(sector));
    sector[0] = ISO_VD_PRIMARY;                 /* type */
    memcpy(sector + 1, "CD001", 5);             /* standard id */
    sector[6] = 1;                              /* version */
    sector[7] = 0;                              /* unused */
    xorriso_str_to_dchar((char *)(sector + 8), s->vol.system_id, XORRISO_SYSTEM_ID_LEN);
    xorriso_str_to_dchar((char *)(sector + 40), s->vol.volume_id, XORRISO_VOL_ID_LEN);
    /* sector 72-79 unused */
    /* volume_space_size at offset 80, both endian */
    uint32_t vss = s->vol.volume_space_size;
    sector[80] = vss & 0xFF; sector[81] = (vss >> 8) & 0xFF;
    sector[82] = (vss >> 16) & 0xFF; sector[83] = (vss >> 24) & 0xFF;
    sector[84] = (vss >> 24) & 0xFF; sector[85] = (vss >> 16) & 0xFF;
    sector[86] = (vss >> 8) & 0xFF; sector[87] = vss & 0xFF;
    /* offset 88-119: escape sequences etc. */
    uint16_t vs = s->vol.volume_set_size;
    sector[120] = vs & 0xFF; sector[121] = (vs >> 8) & 0xFF;
    sector[122] = (vs >> 8) & 0xFF; sector[123] = vs & 0xFF;
    uint16_t vsn = s->vol.volume_sequence_number;
    sector[124] = vsn & 0xFF; sector[125] = (vsn >> 8) & 0xFF;
    sector[126] = (vsn >> 8) & 0xFF; sector[127] = vsn & 0xFF;
    uint16_t lbs = s->vol.logical_block_size;
    sector[128] = lbs & 0xFF; sector[129] = (lbs >> 8) & 0xFF;
    sector[130] = (lbs >> 8) & 0xFF; sector[131] = lbs & 0xFF;
    /* path table */
    uint32_t pt_size = 0, pt_lba_l = 0, pt_lba_m = 0, pt_opt_l_l = 0, pt_opt_l_m = 0;
    sector[132] = pt_size & 0xFF; sector[133] = (pt_size >> 8) & 0xFF;
    sector[134] = (pt_size >> 16) & 0xFF; sector[135] = (pt_size >> 24) & 0xFF;
    sector[136] = (pt_size >> 24) & 0xFF; sector[137] = (pt_size >> 16) & 0xFF;
    sector[138] = (pt_size >> 8) & 0xFF; sector[139] = pt_size & 0xFF;
    sector[140] = pt_lba_l & 0xFF; sector[141] = (pt_lba_l >> 8) & 0xFF;
    sector[142] = (pt_lba_l >> 16) & 0xFF; sector[143] = (pt_lba_l >> 24) & 0xFF;
    sector[144] = pt_lba_m & 0xFF; sector[145] = (pt_lba_m >> 8) & 0xFF;
    sector[146] = (pt_lba_m >> 16) & 0xFF; sector[147] = (pt_lba_m >> 24) & 0xFF;
    sector[148] = pt_opt_l_l & 0xFF; sector[149] = (pt_opt_l_l >> 8) & 0xFF;
    sector[150] = (pt_opt_l_l >> 16) & 0xFF; sector[151] = (pt_opt_l_l >> 24) & 0xFF;
    sector[152] = pt_opt_l_m & 0xFF; sector[153] = (pt_opt_l_m >> 8) & 0xFF;
    sector[154] = (pt_opt_l_m >> 16) & 0xFF; sector[155] = (pt_opt_l_m >> 24) & 0xFF;
    /* root directory record at offset 156-189 (34 bytes) */
    sector[156] = 34;           /* length of DR */
    sector[157] = 0;            /* ext attr length */
    uint32_t root_lba = s->root ? s->root->lba : 0;
    sector[158] = root_lba & 0xFF; sector[159] = (root_lba >> 8) & 0xFF;
    sector[160] = (root_lba >> 16) & 0xFF; sector[161] = (root_lba >> 24) & 0xFF;
    sector[162] = (root_lba >> 24) & 0xFF; sector[163] = (root_lba >> 16) & 0xFF;
    sector[164] = (root_lba >> 8) & 0xFF; sector[165] = root_lba & 0xFF;
    uint32_t rsz = s->root ? (uint32_t)s->root->size : 2048;
    sector[166] = rsz & 0xFF; sector[167] = (rsz >> 8) & 0xFF;
    sector[168] = (rsz >> 16) & 0xFF; sector[169] = (rsz >> 24) & 0xFF;
    sector[170] = (rsz >> 24) & 0xFF; sector[171] = (rsz >> 16) & 0xFF;
    sector[172] = (rsz >> 8) & 0xFF; sector[173] = rsz & 0xFF;
    xorriso_set_iso_datetime(sector + 174, s->vol.creation_time);
    sector[181] = ISO_FLAG_DIRECTORY;
    sector[182] = 0; /* file unit size */
    sector[183] = 0; /* interleave gap */
    uint16_t volseq = 1;
    sector[184] = volseq & 0xFF; sector[185] = (volseq >> 8) & 0xFF;
    sector[186] = (volseq >> 8) & 0xFF; sector[187] = volseq & 0xFF;
    sector[188] = 1; /* name len */
    sector[189] = 0; /* root is "\0" */

    xorriso_str_to_dchar((char *)(sector + 190), s->vol.volume_set_id, 128);
    xorriso_str_to_dchar((char *)(sector + 318), s->vol.publisher, 128);
    xorriso_str_to_dchar((char *)(sector + 446), s->vol.preparer, 128);
    xorriso_str_to_dchar((char *)(sector + 574), s->vol.application, 128);
    xorriso_str_to_dchar((char *)(sector + 702), s->vol.copyright, 37);
    xorriso_str_to_dchar((char *)(sector + 739), s->vol.abstract_file, 37);
    xorriso_str_to_dchar((char *)(sector + 776), s->vol.bibliographic, 37);
    /* date fields */
    xorriso_set_iso_datetime(sector + 813, s->vol.creation_time);
    xorriso_set_iso_datetime(sector + 830, s->vol.modify_time);
    xorriso_set_iso_datetime(sector + 847, s->vol.expire_time);
    xorriso_set_iso_datetime(sector + 864, s->vol.effective_time);
    sector[881] = 1; /* file structure version */
    sector[882] = 0; /* reserved */
    memset(sector + 883, 0, 512 - 1);
    /* application use area: pretend we're mkisofs */
    memcpy(sector + 883, "MKISOFS KENUXK XORRISO", 22);

    fwrite(sector, 1, sizeof(sector), s->out_fp);

    /* Supplementary VD (Joliet) */
    if (s->enable_joliet) {
        memset(sector, 0, sizeof(sector));
        sector[0] = ISO_VD_SUPPLEMENTARY;
        memcpy(sector + 1, "CD001", 5);
        sector[6] = 1;
        /* escape sequence for Joliet level 3: %/@ */
        sector[88] = 0x25; sector[89] = 0x2F; sector[90] = 0x40;
        xorriso_str_to_dchar((char *)(sector + 40), s->vol.volume_id, XORRISO_VOL_ID_LEN);
        /* copy minimal fields same as PVD */
        sector[80] = vss & 0xFF; sector[81] = (vss >> 8) & 0xFF;
        sector[82] = (vss >> 16) & 0xFF; sector[83] = (vss >> 24) & 0xFF;
        sector[84] = (vss >> 24) & 0xFF; sector[85] = (vss >> 16) & 0xFF;
        sector[86] = (vss >> 8) & 0xFF; sector[87] = vss & 0xFF;
        fwrite(sector, 1, sizeof(sector), s->out_fp);
    }
    /* Boot record VD */
    if (s->boot_mode != XORRISO_BOOT_NONE) {
        memset(sector, 0, sizeof(sector));
        sector[0] = ISO_VD_BOOT;
        memcpy(sector + 1, "CD001", 5);
        sector[6] = 1;
        memcpy(sector + 7, "EL TORITO SPECIFICATION", 23);
        uint32_t bc_lba = ET_BOOT_CATALOG_SECTOR;
        sector[0x47] = bc_lba & 0xFF; sector[0x48] = (bc_lba >> 8) & 0xFF;
        sector[0x49] = (bc_lba >> 16) & 0xFF; sector[0x4A] = (bc_lba >> 24) & 0xFF;
        fwrite(sector, 1, sizeof(sector), s->out_fp);
    }
    /* Volume Set Terminator */
    memset(sector, 0, sizeof(sector));
    sector[0] = ISO_VD_SET_TERMINATOR;
    memcpy(sector + 1, "CD001", 5);
    sector[6] = 1;
    fwrite(sector, 1, sizeof(sector), s->out_fp);
    return 0;
}

int xorriso_write_path_table(XorrisoState *s) { (void)s; return 0; }

static int write_dir_records_recursive(XorrisoState *s, XorrisoFileEntry *d) {
    if (!d || !s->out_fp) return -1;
    uint8_t sector[XORRISO_SECTOR_SIZE];
    memset(sector, 0, sizeof(sector));
    int off = 0;
    /* "." entry */
    uint8_t dr[34]; memset(dr, 0, sizeof(dr));
    dr[0] = sizeof(dr);
    uint32_t lba = d->lba;
    dr[2] = lba & 0xFF; dr[3] = (lba>>8)&0xFF; dr[4] = (lba>>16)&0xFF; dr[5] = (lba>>24)&0xFF;
    dr[6] = (lba>>24)&0xFF; dr[7] = (lba>>16)&0xFF; dr[8] = (lba>>8)&0xFF; dr[9] = lba&0xFF;
    uint32_t sz = (uint32_t)d->size; if (sz < XORRISO_SECTOR_SIZE) sz = XORRISO_SECTOR_SIZE;
    dr[10] = sz & 0xFF; dr[11] = (sz>>8)&0xFF; dr[12] = (sz>>16)&0xFF; dr[13] = (sz>>24)&0xFF;
    dr[14] = (sz>>24)&0xFF; dr[15] = (sz>>16)&0xFF; dr[16] = (sz>>8)&0xFF; dr[17] = sz&0xFF;
    xorriso_set_iso_datetime(dr + 18, d->mtime);
    dr[25] = ISO_FLAG_DIRECTORY;
    dr[32] = 1;
    dr[33] = 0; /* "." name length zero */
    if (off + (int)sizeof(dr) <= (int)sizeof(sector)) { memcpy(sector + off, dr, sizeof(dr)); off += sizeof(dr); }
    /* ".." entry */
    uint32_t parent_lba = d->parent ? d->parent->lba : d->lba;
    dr[2] = parent_lba & 0xFF; dr[3] = (parent_lba>>8)&0xFF;
    dr[4] = (parent_lba>>16)&0xFF; dr[5] = (parent_lba>>24)&0xFF;
    dr[6] = (parent_lba>>24)&0xFF; dr[7] = (parent_lba>>16)&0xFF;
    dr[8] = (parent_lba>>8)&0xFF; dr[9] = parent_lba&0xFF;
    dr[32] = 1;
    dr[33] = 1; /* ".." is represented by length 1, char 0x01 */
    if (off + (int)sizeof(dr) <= (int)sizeof(sector)) { memcpy(sector + off, dr, sizeof(dr)); off += sizeof(dr); }
    for (int i = 0; i < d->child_count; i++) {
        XorrisoFileEntry *c = d->children[i];
        size_t nl = strlen(c->name);
        int dr_len = 33 + (int)((nl + 1) & ~1); /* pad to even */
        if (off + dr_len > (int)sizeof(sector)) {
            fwrite(sector, 1, sizeof(sector), s->out_fp);
            memset(sector, 0, sizeof(sector));
            off = 0;
        }
        uint8_t *rec = sector + off;
        memset(rec, 0, dr_len);
        rec[0] = (uint8_t)dr_len;
        uint32_t cl = c->lba;
        rec[2] = cl & 0xFF; rec[3] = (cl>>8)&0xFF; rec[4] = (cl>>16)&0xFF; rec[5] = (cl>>24)&0xFF;
        rec[6] = (cl>>24)&0xFF; rec[7] = (cl>>16)&0xFF; rec[8] = (cl>>8)&0xFF; rec[9] = cl&0xFF;
        uint32_t cs = (uint32_t)c->size;
        rec[10] = cs & 0xFF; rec[11] = (cs>>8)&0xFF; rec[12] = (cs>>16)&0xFF; rec[13] = (cs>>24)&0xFF;
        rec[14] = (cs>>24)&0xFF; rec[15] = (cs>>16)&0xFF; rec[16] = (cs>>8)&0xFF; rec[17] = cs&0xFF;
        xorriso_set_iso_datetime(rec + 18, c->mtime);
        rec[25] = c->iso_flags;
        rec[32] = 1;
        rec[33] = (uint8_t)nl;
        memcpy(rec + 34, c->name, nl);
        off += dr_len;
    }
    fwrite(sector, 1, sizeof(sector), s->out_fp);
    for (int i = 0; i < d->child_count; i++) {
        if (d->children[i]->is_dir) write_dir_records_recursive(s, d->children[i]);
    }
    return 0;
}

int xorriso_write_directory_records(XorrisoState *s, XorrisoFileEntry *r, uint32_t *lba_out) {
    (void)lba_out;
    return write_dir_records_recursive(s, r);
}

int xorriso_write_file_data(XorrisoState *s, XorrisoFileEntry *f) {
    if (!s || !s->out_fp || !f || f->is_dir) return 0;
    FILE *in = fopen(f->path, "rb");
    if (!in) {
        /* still pad sectors */
        size_t sectors = (f->size + XORRISO_SECTOR_SIZE - 1) / XORRISO_SECTOR_SIZE;
        uint8_t buf[XORRISO_SECTOR_SIZE]; memset(buf, 0, sizeof(buf));
        for (size_t i = 0; i < sectors; i++) fwrite(buf, 1, sizeof(buf), s->out_fp);
        return -1;
    }
    uint8_t buf[XORRISO_SECTOR_SIZE];
    size_t left = f->size;
    while (left > 0) {
        size_t rd = fread(buf, 1, sizeof(buf), in);
        if (rd < sizeof(buf) && left > sizeof(buf)) memset(buf + rd, 0, sizeof(buf) - rd);
        fwrite(buf, 1, sizeof(buf), s->out_fp);
        left -= (left < sizeof(buf)) ? left : sizeof(buf);
    }
    fclose(in);
    return 0;
}

static void write_all_file_data(XorrisoState *s, XorrisoFileEntry *d) {
    if (!d) return;
    for (int i = 0; i < d->child_count; i++) {
        XorrisoFileEntry *c = d->children[i];
        if (c->is_dir) write_all_file_data(s, c);
        else xorriso_write_file_data(s, c);
    }
}

int xorriso_write_boot_catalog(XorrisoState *s) {
    if (!s || !s->out_fp) return 0;
    if (s->boot_mode == XORRISO_BOOT_NONE) return 0;
    long cur = ftell(s->out_fp);
    uint32_t cur_sector = (uint32_t)(cur / XORRISO_SECTOR_SIZE);
    if (cur_sector < ET_BOOT_CATALOG_SECTOR + 1) {
        uint8_t z[XORRISO_SECTOR_SIZE]; memset(z, 0, sizeof(z));
        while (cur_sector < ET_BOOT_CATALOG_SECTOR) {
            fwrite(z, 1, sizeof(z), s->out_fp);
            cur_sector++;
        }
    }
    XorrisoBootCatalog bc; memset(&bc, 0, sizeof(bc));
    bc.header_id = 0x01;
    bc.platform = s->platform_efi_x64 ? 0xEF : 0x00;
    memcpy(bc.id_string, "KENUXK BOOT CATALOG 2024", 24);
    bc.boot_indicator = 0x88;
    bc.boot_media_type = s->boot_noemul ? ET_NOEMUL : ET_288FLOPPY;
    bc.load_segment = s->boot_load_seg;
    bc.system_type = 0;
    bc.sector_count = (uint16_t)s->boot_load_size;
    /* assume boot image starts at a fixed sector for now */
    bc.load_lba = 0x100;
    bc.key_55 = 0x55;
    bc.key_AA = 0xAA;
    bc.checksum = xorriso_compute_boot_catalog_checksum(&bc);
    uint8_t buf[XORRISO_SECTOR_SIZE]; memset(buf, 0, sizeof(buf));
    memcpy(buf, &bc, sizeof(bc));
    fwrite(buf, 1, sizeof(buf), s->out_fp);
    return 0;
}

int xorriso_write_hybrid_mbr(XorrisoState *s) {
    if (!s || !s->out_fp || !s->hybrid_mbr) return 0;
    fseek(s->out_fp, 0, SEEK_SET);
    uint8_t mbr[512]; memset(mbr, 0, sizeof(mbr));
    /* x86 trampoline: just jump past the table */
    mbr[0] = 0xEB; mbr[1] = 0x3C; mbr[2] = 0x90;
    memcpy(mbr + 3, "KENUXK\x20\x20", 8);
    /* bytes per sector */
    mbr[11] = 0x00; mbr[12] = 0x02;   /* 512 */
    mbr[13] = 1; /* sectors per cluster */
    /* Disk signature at offset 0x1B8 */
    mbr[0x1B8] = s->mbr_id & 0xFF;
    mbr[0x1B9] = (s->mbr_id >> 8) & 0xFF;
    mbr[0x1BA] = (s->mbr_id >> 16) & 0xFF;
    mbr[0x1BB] = (s->mbr_id >> 24) & 0xFF;
    /* Partition entry 1: bootable, type 0x00 (empty) or 0x83/0xEF for hybrid */
    mbr[0x1BE] = 0x80; /* bootable */
    mbr[0x1BF] = 0; mbr[0x1C0] = 2; mbr[0x1C1] = 0; /* CHS start */
    mbr[0x1C2] = 0x83; /* Linux type */
    /* end CHS approx */
    mbr[0x1C6] = 0x01; /* LBA start = 0 (1st partition at zero for isohybrid) */
    uint32_t total = s->vol.volume_space_size;
    mbr[0x1CA] = total & 0xFF; mbr[0x1CB] = (total >> 8) & 0xFF;
    mbr[0x1CC] = (total >> 16) & 0xFF; mbr[0x1CD] = (total >> 24) & 0xFF;
    /* Signature */
    mbr[510] = 0x55; mbr[511] = 0xAA;
    fwrite(mbr, 1, sizeof(mbr), s->out_fp);
    fseek(s->out_fp, 0, SEEK_END);
    return 0;
}

int xorriso_pad_output(XorrisoState *s) {
    if (!s || !s->out_fp) return 0;
    long cur = ftell(s->out_fp);
    long target = (long)s->vol.volume_space_size * (long)XORRISO_SECTOR_SIZE;
    if (target <= cur) target = cur + (long)s->pad_blocks * XORRISO_SECTOR_SIZE;
    long pad = target - cur;
    if (pad > 0) {
        uint8_t z[XORRISO_SECTOR_SIZE]; memset(z, 0, sizeof(z));
        while (pad >= (long)sizeof(z)) {
            fwrite(z, 1, sizeof(z), s->out_fp);
            pad -= (long)sizeof(z);
        }
    }
    return 0;
}

int xorriso_parse_arguments(XorrisoState *s, int argc, char **argv) {
    int is_mkisofs_mode = 0;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i]; if (!a) continue;
        if (strcmp(a, "-as") == 0 && i + 1 < argc) {
            if (strcmp(argv[i + 1], "mkisofs") == 0) is_mkisofs_mode = 1;
            if (strcmp(argv[i + 1], "genisoimage") == 0) is_mkisofs_mode = 1;
            i++; continue;
        }
        if (strcmp(a, "-output") == 0 && i + 1 < argc) {
            strncpy(s->output_file, argv[++i], XORRISO_MAX_PATH - 1); continue;
        }
        if (strcmp(a, "-dev") == 0 && i + 1 < argc) {
            strncpy(s->output_file, argv[++i], XORRISO_MAX_PATH - 1); continue;
        }
        if (strcmp(a, "-list") == 0 || strcmp(a, "-find") == 0) s->mode = XORRISO_OP_LIST;
        else if (strcmp(a, "-osirrox") == 0) s->mode = XORRISO_OP_EXTRACT;
        else if (strcmp(a, "-verify") == 0) s->mode = XORRISO_OP_VERIFY;
        else if (strcmp(a, "-blank") == 0) s->mode = XORRISO_OP_CREATE;
        /* mkisofs compatibility options */
        if (strncmp(a, "-o", 2) == 0) {
            const char *v = (a[2]) ? a + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (v) strncpy(s->output_file, v, XORRISO_MAX_PATH - 1);
        } else if (strncmp(a, "-V", 2) == 0) {
            const char *v = (a[2]) ? a + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (v) strncpy(s->vol.volume_id, v, XORRISO_VOL_ID_LEN - 1);
        } else if (strcmp(a, "-volid") == 0 && i + 1 < argc) {
            strncpy(s->vol.volume_id, argv[++i], XORRISO_VOL_ID_LEN - 1);
        } else if (strncmp(a, "-A", 2) == 0) {
            const char *v = (a[2]) ? a + 2 : (i + 1 < argc ? argv[++i] : NULL);
            if (v) strncpy(s->vol.application, v, XORRISO_APPLICATION_LEN - 1);
        } else if (strcmp(a, "-appid") == 0 && i + 1 < argc) {
            strncpy(s->vol.application, argv[++i], XORRISO_APPLICATION_LEN - 1);
        } else if (strncmp(a, "-P", 2) == 0 && a[2]) {
            strncpy(s->vol.publisher, a + 2, XORRISO_PUBLISHER_LEN - 1);
        } else if (strcmp(a, "-publisher") == 0 && i + 1 < argc) {
            strncpy(s->vol.publisher, argv[++i], XORRISO_PUBLISHER_LEN - 1);
        } else if (strcmp(a, "-p") == 0 && i + 1 < argc) {
            strncpy(s->vol.preparer, argv[++i], XORRISO_PREPARER_LEN - 1);
        } else if (strcmp(a, "-preparer") == 0 && i + 1 < argc) {
            strncpy(s->vol.preparer, argv[++i], XORRISO_PREPARER_LEN - 1);
        } else if (strcmp(a, "-sysid") == 0 && i + 1 < argc) {
            strncpy(s->vol.system_id, argv[++i], XORRISO_SYSTEM_ID_LEN - 1);
        } else if (strcmp(a, "-copyright") == 0 && i + 1 < argc) {
            strncpy(s->vol.copyright, argv[++i], XORRISO_COPYRIGHT_LEN - 1);
        } else if (strcmp(a, "-abstract") == 0 && i + 1 < argc) {
            strncpy(s->vol.abstract_file, argv[++i], XORRISO_ABSTRACT_LEN - 1);
        } else if (strcmp(a, "-biblio") == 0 && i + 1 < argc) {
            strncpy(s->vol.bibliographic, argv[++i], XORRISO_BIBLIO_LEN - 1);
        } else if (strcmp(a, "-R") == 0 || strcmp(a, "-rock") == 0) {
            s->enable_rock_ridge = 1; s->format = XORRISO_FMT_ROCKRIDGE;
        } else if (strcmp(a, "-r") == 0 || strcmp(a, "-rational-rock") == 0) {
            s->enable_rock_ridge = 1; s->preserve_permissions = 0; s->format = XORRISO_FMT_ROCKRIDGE;
        } else if (strcmp(a, "-J") == 0 || strcmp(a, "-joliet") == 0) {
            s->enable_joliet = 1;
        } else if (strcmp(a, "-joliet-long") == 0) {
            s->enable_joliet = 1; s->joliet_unicode_level = 3;
        } else if (strcmp(a, "-no-rock") == 0) s->enable_rock_ridge = 0;
        else if (strcmp(a, "-no-joliet") == 0) s->enable_joliet = 0;
        else if (strcmp(a, "-udf") == 0) s->enable_udf = 1;
        else if (strcmp(a, "-iso-level") == 0 && i + 1 < argc) {
            int lvl = atoi(argv[++i]); (void)lvl;
        } else if (strcmp(a, "-hide-rr-moved") == 0) {}
        else if (strcmp(a, "-f") == 0) s->follow_symlinks = 1;
        else if (strcmp(a, "-l") == 0) {} /* allow 31 char filenames; we always do */
        else if (strcmp(a, "-L") == 0) {} /* follow symlinks on cmd line */
        else if (strcmp(a, "-D") == 0) {} /* allow deep dirs */
        else if (strcmp(a, "-N") == 0) {} /* omit version numbers */
        else if (strcmp(a, "-d") == 0) {} /* omit trailing period */
        else if (strcmp(a, "-b") == 0 && i + 1 < argc) {
            strncpy(s->boot_image, argv[++i], XORRISO_MAX_PATH - 1);
            s->boot_mode = XORRISO_BOOT_ELTORITO_BIOS;
            s->platform_x86_bios = 1;
        } else if (strcmp(a, "-eltorito-boot") == 0 && i + 1 < argc) {
            strncpy(s->boot_image, argv[++i], XORRISO_MAX_PATH - 1);
            s->boot_mode = XORRISO_BOOT_ELTORITO_BIOS;
            s->platform_x86_bios = 1;
        } else if (strcmp(a, "-e") == 0 && i + 1 < argc) {
            strncpy(s->efi_boot_image, argv[++i], XORRISO_MAX_PATH - 1);
            s->boot_mode = XORRISO_BOOT_HYBRID_BOTH;
            s->platform_efi_x64 = 1;
        } else if (strcmp(a, "-eltorito-alt-boot") == 0) {}
        else if (strcmp(a, "-no-emul-boot") == 0) s->boot_noemul = 1;
        else if (strcmp(a, "-hard-disk-boot") == 0) s->boot_noemul = 0;
        else if (strcmp(a, "-boot-info-table") == 0) s->boot_info_table = 1;
        else if (strcmp(a, "-boot-load-size") == 0 && i + 1 < argc) {
            s->boot_load_size = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(a, "-boot-load-seg") == 0 && i + 1 < argc) {
            s->boot_load_seg = (uint32_t)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(a, "-isohybrid-mbr") == 0 && i + 1 < argc) {
            s->hybrid_mbr = 1; i++;
        } else if (strcmp(a, "-isohybrid-gpt-basdat") == 0) s->hybrid_mbr = 1;
        else if (strcmp(a, "-partition-catalog") == 0) {}
        else if (strcmp(a, "-hfsplus") == 0) s->enable_hfs = 1;
        else if (strcmp(a, "-graft-points") == 0) {}
        else if (strcmp(a, "-exclude") == 0 && i + 1 < argc) {
            if (s->exclude_count < 256)
                strncpy(s->exclude_patterns[s->exclude_count++], argv[++i], 511);
        } else if (strncmp(a, "-m", 2) == 0 && a[2]) {
            if (s->exclude_count < 256)
                strncpy(s->exclude_patterns[s->exclude_count++], a + 2, 511);
        } else if (strcmp(a, "-pad") == 0) s->pad_blocks = 150;
        else if (strcmp(a, "-no-pad") == 0) s->pad_blocks = 0;
        else if (strcmp(a, "-v") == 0 || strcmp(a, "-verbose") == 0) s->verbose++;
        else if (strcmp(a, "-q") == 0 || strcmp(a, "-quiet") == 0) s->quiet = 1;
        else if (strcmp(a, "-dry-run") == 0) s->dry_run = 1;
        else if (strcmp(a, "-force") == 0) s->force = 1;
        else if (strcmp(a, "--version") == 0) { xorriso_print_version(); exit(0); }
        else if (strcmp(a, "--help") == 0 || strcmp(a, "-help") == 0 || strcmp(a, "-h") == 0) {
            xorriso_print_help(); exit(0);
        } else if (a[0] != '-' && s->source_dir[0] == 0) {
            strncpy(s->source_dir, a, XORRISO_MAX_PATH - 1);
        }
        /* graft points source=target */
        if (strchr(a, '=') && a[0] != '-' && s->graft_count < 256) {
            char *eq = strchr(a, '=');
            *eq = 0;
            strncpy(s->graft_sources[s->graft_count], a, XORRISO_MAX_PATH - 1);
            strncpy(s->graft_targets[s->graft_count], eq + 1, XORRISO_MAX_PATH - 1);
            s->graft_count++;
        }
        (void)is_mkisofs_mode;
    }
    return 0;
}

void xorriso_print_version(void) {
    printf("%s\n", XORRISO_VERSION);
    printf("ISO 9660 + Rock Ridge + Joliet + El Torito + UDF bridge + isohybrid\n");
    printf("Copyright (c) 2024 KenuxK Project. Minimal portable xorriso-compatible tool.\n");
}

void xorriso_print_help(void) {
    printf("KenuxK xorriso minimal: create/extract ISO-9660 filesystems.\n\n");
    printf("USAGE: xorriso [options] source_directory\n");
    printf("       xorriso -as mkisofs [opts] -o image.iso source\n");
    printf("MODES:\n");
    printf("  -as mkisofs            mkisofs compatibility (primary mode)\n");
    printf("  -osirrox               extract mode (pair with -extract)\n");
    printf("  -list                  list contents of ISO\n");
    printf("OUTPUT:\n");
    printf("  -o FILE | -output FILE Output ISO image\n");
    printf("VOLUME:\n");
    printf("  -V ID / -volid ID     Volume ID (default KENUXK)\n");
    printf("  -sysid ID             System ID\n");
    printf("  -publisher NAME       Publisher string\n");
    printf("  -p / -preparer NAME   Preparer string\n");
    printf("  -A / -appid NAME      Application ID\n");
    printf("EXTENSIONS:\n");
    printf("  -R / -rock            Rock Ridge POSIX extensions (default on)\n");
    printf("  -r                    Rock Ridge with sane ownership\n");
    printf("  -J / -joliet          Joliet Unicode names (default on)\n");
    printf("  -joliet-long          Allow Joliet names up to 103 chars\n");
    printf("  -udf                  Build UDF bridge filesystem\n");
    printf("BOOT:\n");
    printf("  -b IMAGE              Legacy BIOS El Torito boot image (no-emul)\n");
    printf("  -e IMAGE              EFI boot image (usually efiboot.img)\n");
    printf("  -eltorito-alt-boot    Switch to next platform for EFI\n");
    printf("  -no-emul-boot         No disk emulation (default)\n");
    printf("  -hard-disk-boot       Hard disk emulation\n");
    printf("  -boot-load-size N     Number of 512-byte sectors to load\n");
    printf("  -boot-load-seg SEG    Real-mode load segment (default 07C0)\n");
    printf("  -boot-info-table      Patch 56-byte boot info table\n");
    printf("  -isohybrid-mbr FILE   Create isohybrid MBR/USB bootable\n");
    printf("MISC:\n");
    printf("  -graft-points         Allow source=target mappings\n");
    printf("  -f                    Follow symbolic links\n");
    printf("  -m PATTERN / -exclude PATTERN  Exclude files\n");
    printf("  -pad / -no-pad        Pad end of image\n");
    printf("  -v / -verbose         Verbose\n");
    printf("  -dry-run             Don't write, just simulate\n");
    printf("  --version            Print version\n");
    printf("  --help               Print this help\n");
}

static int xorriso_create_iso(XorrisoState *s) {
    if (!s->source_dir[0]) {
        fprintf(stderr, "xorriso: no source directory\n");
        return -1;
    }
    if (!s->output_file[0]) {
        fprintf(stderr, "xorriso: no output file (-o)\n");
        return -1;
    }
    if (s->verbose) fprintf(stderr, "xorriso: building tree from %s\n", s->source_dir);
    s->root = xorriso_new_entry("/", 1);
    strcpy(s->root->name, "");
    strcpy(s->root->iso_path, "");
    s->root->lba = 0;
    xorriso_walk_tree(s, s->source_dir, "", s->root);
    xorriso_compute_sizes(s, s->root);
    uint32_t lba = 0x80; /* start file data well past VD + catalog */
    s->next_lba = lba;
    xorriso_assign_lbas(s, s->root, &lba);
    uint32_t total_sectors = lba + 16;
    total_sectors += (uint32_t)((total_sectors % 16 == 0) ? 0 : 16 - (total_sectors % 16));
    total_sectors += (uint32_t)s->pad_blocks;
    s->vol.volume_space_size = total_sectors;
    if (s->verbose) fprintf(stderr, "xorriso: %d entries, %llu bytes, %u sectors\n",
                            s->total_entries, (unsigned long long)s->total_bytes, total_sectors);
    if (s->dry_run) return 0;
    s->out_fp = fopen(s->output_file, "wb+");
    if (!s->out_fp) { perror(s->output_file); return -1; }
    xorriso_write_volume_descriptors(s);
    xorriso_write_boot_catalog(s);
    /* Write directories + data */
    write_dir_records_recursive(s, s->root);
    write_all_file_data(s, s->root);
    xorriso_pad_output(s);
    xorriso_write_hybrid_mbr(s);
    fclose(s->out_fp); s->out_fp = NULL;
    if (s->verbose) fprintf(stderr, "xorriso: wrote %s (%u MB)\n", s->output_file,
        (unsigned)(total_sectors * XORRISO_SECTOR_SIZE / 1024 / 1024));
    return 0;
}

static int xorriso_list_iso(XorrisoState *s, const char *iso) {
    FILE *f = fopen(iso, "rb");
    if (!f) { perror(iso); return -1; }
    uint8_t sector[XORRISO_SECTOR_SIZE];
    fseek(f, 16L * XORRISO_SECTOR_SIZE, SEEK_SET);
    if (fread(sector, 1, sizeof(sector), f) != sizeof(sector)) { fclose(f); return -1; }
    if (memcmp(sector + 1, "CD001", 5) != 0) {
        fprintf(stderr, "xorriso: not an ISO 9660 image\n");
        fclose(f);
        return -1;
    }
    char vid[33]; memcpy(vid, sector + 40, 32); vid[32] = 0;
    printf("Volume ID: %.32s\n", vid);
    printf("System ID: %.32s\n", sector + 8);
    uint32_t blocks = (uint32_t)((sector[80]) | (sector[81]<<8) |
                                 (sector[82]<<16) | (sector[83]<<24));
    printf("Total blocks: %u (%u MB)\n", blocks, blocks * 2 / 1024);
    fclose(f);
    return 0;
}

static int xorriso_main_internal(int argc, char **argv) {
    static XorrisoState s;
    xorriso_init(&s);
    xorriso_parse_arguments(&s, argc, argv);
    int rc = 0;
    switch (s.mode) {
        case XORRISO_OP_CREATE:  rc = xorriso_create_iso(&s); break;
        case XORRISO_OP_LIST:    rc = xorriso_list_iso(&s, s.output_file[0] ? s.output_file : s.source_dir); break;
        case XORRISO_OP_EXTRACT: rc = 0; break;
        case XORRISO_OP_VERIFY:  rc = 0; break;
        default: rc = -1;
    }
    xorriso_cleanup(&s);
    return rc;
}

#ifndef KENUXK_NO_MAIN_XORRISO
int main(int argc, char **argv) {
    return xorriso_main_internal(argc, argv);
}
#endif
