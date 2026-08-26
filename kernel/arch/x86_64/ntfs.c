#include <arch/ntfs.h>
#include <arch/slab.h>
#include <arch/memory.h>
#include <string.h>

#define NTFS_MFT_RECORD_SIZE      1024
#define NTFS_MFT_MAX_RECORDS      1024
#define NTFS_RESIDENT_DATA_LIMIT  700
#define NTFS_INDEX_ROOT_SIZE      4096
#define NTFS_VOLUME_BUFFER_SIZE   (1024 * 1024)
#define NTFS_MFT_BITMAP_SIZE      ((NTFS_MFT_MAX_RECORDS + 7) / 8)
#define NTFS_NONRESIDENT_BLOCK    65536

#define MFT_RECORD_PTR(fs, idx)   ((uint8_t*)(fs)->mft_buffer + (uint64_t)(idx) * NTFS_MFT_RECORD_SIZE)

#define ATTR_FILENAME_BASE_SIZE   offsetof(NTFS_FILE_NAME, file_name)

static ntfs_fs_t* g_ntfs_fs = NULL;
static uint32_t g_next_mft_idx = 0;
static uint8_t g_mft_bitmap[NTFS_MFT_BITMAP_SIZE];

static void* ntfs_kmalloc(uint32_t size)
{
    return memory_alloc((uint64_t)size);
}

static void ntfs_kfree(void* ptr)
{
    if (ptr != NULL) {
        memory_free(ptr);
    }
}

static void ascii_to_utf16(const char* ascii, uint16_t* utf16, uint32_t max_len)
{
    uint32_t i;
    for (i = 0; i < max_len && ascii[i] != '\0'; i++) {
        utf16[i] = (uint16_t)(uint8_t)ascii[i];
    }
    if (i < max_len) {
        utf16[i] = 0;
    }
}

static int utf16_ascii_cmp(const uint16_t* u, const char* a, uint32_t u_len)
{
    uint32_t i;
    uint32_t a_len = (uint32_t)strlen(a);
    if (u_len != a_len) {
        return (u_len < a_len) ? -1 : 1;
    }
    for (i = 0; i < u_len; i++) {
        uint16_t uc = u[i];
        uint8_t ac = (uint8_t)a[i];
        if (uc != (uint16_t)ac) {
            return (uc < (uint16_t)ac) ? -1 : 1;
        }
    }
    return 0;
}

static void mft_bitmap_set(uint32_t idx, int used)
{
    if (idx >= NTFS_MFT_MAX_RECORDS) {
        return;
    }
    uint32_t byte = idx / 8U;
    uint32_t bit = idx % 8U;
    if (used) {
        g_mft_bitmap[byte] |= (uint8_t)(1U << bit);
    } else {
        g_mft_bitmap[byte] &= (uint8_t)~(1U << bit);
    }
}

static int mft_bitmap_test(uint32_t idx)
{
    if (idx >= NTFS_MFT_MAX_RECORDS) {
        return 0;
    }
    uint32_t byte = idx / 8U;
    uint32_t bit = idx % 8U;
    return (g_mft_bitmap[byte] & (uint8_t)(1U << bit)) != 0U ? 1 : 0;
}

static uint32_t mft_alloc_record(void)
{
    uint32_t i;
    for (i = 0; i < NTFS_MFT_MAX_RECORDS; i++) {
        if (!mft_bitmap_test(i)) {
            mft_bitmap_set(i, 1);
            if (i >= g_next_mft_idx) {
                g_next_mft_idx = i + 1U;
            }
            return i;
        }
    }
    return (uint32_t)-1;
}

static void init_file_record_header(NTFS_FILE_RECORD_HEADER* hdr, uint32_t mft_idx, int is_dir)
{
    memset(hdr, 0, NTFS_MFT_RECORD_SIZE);
    hdr->magic = NTFS_FILE_RECORD_MAGIC;
    hdr->update_sequence_offset = (uint16_t)sizeof(NTFS_FILE_RECORD_HEADER);
    hdr->update_sequence_size = 3;
    hdr->sequence_number = 1;
    hdr->hard_link_count = 1;
    hdr->attribute_offset = (uint16_t)sizeof(NTFS_FILE_RECORD_HEADER) + 8U;
    hdr->flags = (uint16_t)(NTFS_MFT_RECORD_IN_USE | (is_dir ? NTFS_MFT_RECORD_DIRECTORY : 0));
    hdr->bytes_in_use = hdr->attribute_offset;
    hdr->bytes_allocated = NTFS_MFT_RECORD_SIZE;
    hdr->next_attribute_id = 0;
    hdr->mft_record_number = mft_idx;
}

static uint8_t* append_attribute(NTFS_FILE_RECORD_HEADER* rec, uint32_t* bytes_in_use_out)
{
    uint32_t offset = rec->bytes_in_use;
    uint32_t remaining = NTFS_MFT_RECORD_SIZE - offset;
    if (remaining < sizeof(NTFS_ATTRIBUTE_HEADER)) {
        return NULL;
    }
    uint8_t* ptr = (uint8_t*)rec + offset;
    if (bytes_in_use_out != NULL) {
        *bytes_in_use_out = offset;
    }
    return ptr;
}

static void finalize_attribute(NTFS_FILE_RECORD_HEADER* rec, uint32_t attr_start, uint32_t total_size)
{
    uint32_t new_use = attr_start + total_size;
    if (new_use > rec->bytes_in_use) {
        rec->bytes_in_use = new_use;
    }
    rec->next_attribute_id++;
}

static uint32_t write_attr_standard_info(uint8_t* buf, uint32_t attr_id, uint32_t file_attrs)
{
    NTFS_ATTRIBUTE_HEADER* ah = (NTFS_ATTRIBUTE_HEADER*)buf;
    memset(ah, 0, sizeof(NTFS_ATTRIBUTE_HEADER));
    ah->type = NTFS_ATTRIBUTE_STANDARD_INFORMATION;
    ah->non_resident = 0;
    ah->attribute_id = (uint16_t)attr_id;

    NTFS_STANDARD_INFORMATION* si = (NTFS_STANDARD_INFORMATION*)(buf + sizeof(NTFS_ATTRIBUTE_HEADER));
    memset(si, 0, sizeof(NTFS_STANDARD_INFORMATION));
    si->file_attributes = file_attrs;

    ah->data.resident.value_length = (uint32_t)sizeof(NTFS_STANDARD_INFORMATION);
    ah->data.resident.value_offset = (uint16_t)sizeof(NTFS_ATTRIBUTE_HEADER);
    ah->length = (uint32_t)(sizeof(NTFS_ATTRIBUTE_HEADER) + sizeof(NTFS_STANDARD_INFORMATION));
    return ah->length;
}

static uint32_t write_attr_filename(uint8_t* buf, uint32_t attr_id,
                                     uint64_t parent_ref, const char* name,
                                     uint64_t data_size, uint32_t file_attrs)
{
    NTFS_ATTRIBUTE_HEADER* ah = (NTFS_ATTRIBUTE_HEADER*)buf;
    memset(ah, 0, sizeof(NTFS_ATTRIBUTE_HEADER));
    ah->type = NTFS_ATTRIBUTE_FILE_NAME;
    ah->non_resident = 0;
    ah->attribute_id = (uint16_t)attr_id;

    uint8_t* val_ptr = buf + sizeof(NTFS_ATTRIBUTE_HEADER);
    NTFS_FILE_NAME* fn = (NTFS_FILE_NAME*)val_ptr;
    memset(fn, 0, sizeof(NTFS_FILE_NAME));

    fn->parent_directory_reference = parent_ref;
    fn->allocated_size = data_size;
    fn->data_size = data_size;
    fn->file_attributes = file_attrs;

    uint32_t name_len = (uint32_t)strlen(name);
    if (name_len > NTFS_MAX_FILENAME_LEN) {
        name_len = NTFS_MAX_FILENAME_LEN;
    }
    ascii_to_utf16(name, fn->file_name, NTFS_MAX_FILENAME_LEN);
    fn->file_name_length = (uint8_t)name_len;
    fn->file_namespace = NTFS_NAMESPACE_WIN32;

    uint32_t val_size = ATTR_FILENAME_BASE_SIZE + (uint32_t)sizeof(uint16_t) * name_len;
    ah->data.resident.value_length = val_size;
    ah->data.resident.value_offset = (uint16_t)sizeof(NTFS_ATTRIBUTE_HEADER);
    ah->length = (uint32_t)(sizeof(NTFS_ATTRIBUTE_HEADER) + val_size);
    return ah->length;
}

static uint32_t write_attr_data_resident(uint8_t* buf, uint32_t attr_id,
                                          const uint8_t* data, uint32_t data_len)
{
    NTFS_ATTRIBUTE_HEADER* ah = (NTFS_ATTRIBUTE_HEADER*)buf;
    memset(ah, 0, sizeof(NTFS_ATTRIBUTE_HEADER));
    ah->type = NTFS_ATTRIBUTE_DATA;
    ah->non_resident = 0;
    ah->attribute_id = (uint16_t)attr_id;

    uint8_t* val_ptr = buf + sizeof(NTFS_ATTRIBUTE_HEADER);
    if (data_len > 0 && data != NULL) {
        memcpy(val_ptr, data, (size_t)data_len);
    }

    ah->data.resident.value_length = data_len;
    ah->data.resident.value_offset = (uint16_t)sizeof(NTFS_ATTRIBUTE_HEADER);
    ah->length = (uint32_t)(sizeof(NTFS_ATTRIBUTE_HEADER) + data_len);
    return ah->length;
}

static uint32_t write_attr_data_nonresident(uint8_t* buf, uint32_t attr_id,
                                             uint64_t data_size, uint64_t alloc_size)
{
    NTFS_ATTRIBUTE_HEADER* ah = (NTFS_ATTRIBUTE_HEADER*)buf;
    memset(ah, 0, sizeof(NTFS_ATTRIBUTE_HEADER));
    ah->type = NTFS_ATTRIBUTE_DATA;
    ah->non_resident = 1;
    ah->attribute_id = (uint16_t)attr_id;
    ah->data.non_resident.low_vcn = 0;
    ah->data.non_resident.high_vcn = 0;
    ah->data.non_resident.data_run_offset = (uint16_t)(sizeof(NTFS_ATTRIBUTE_HEADER));
    ah->data.non_resident.allocated_size = alloc_size;
    ah->data.non_resident.data_size = data_size;
    ah->data.non_resident.initialized_size = data_size;
    ah->data.non_resident.compressed_size = alloc_size;
    ah->length = (uint32_t)sizeof(NTFS_ATTRIBUTE_HEADER);
    return ah->length;
}

static uint32_t write_attr_index_root(uint8_t* buf, uint32_t attr_id,
                                       uint32_t entries_size, uint8_t leaf_flag)
{
    NTFS_ATTRIBUTE_HEADER* ah = (NTFS_ATTRIBUTE_HEADER*)buf;
    memset(ah, 0, sizeof(NTFS_ATTRIBUTE_HEADER));
    ah->type = NTFS_ATTRIBUTE_INDEX_ROOT;
    ah->non_resident = 0;
    ah->attribute_id = (uint16_t)attr_id;

    uint8_t* val_ptr = buf + sizeof(NTFS_ATTRIBUTE_HEADER);
    memset(val_ptr, 0, 4U + sizeof(NTFS_INDEX_HEADER) + entries_size);

    uint32_t* idx_type = (uint32_t*)val_ptr;
    *idx_type = NTFS_ATTRIBUTE_FILE_NAME;

    uint32_t* idx_block_size = (uint32_t*)(val_ptr + 4U);
    *idx_block_size = 4096;

    uint8_t* clusters_per_rec = val_ptr + 8U;
    *clusters_per_rec = 1;

    NTFS_INDEX_HEADER* ih = (NTFS_INDEX_HEADER*)(val_ptr + 16U);
    ih->entries_offset = (uint32_t)sizeof(NTFS_INDEX_HEADER);
    ih->index_entries_size = entries_size;
    ih->allocated_size = NTFS_INDEX_ROOT_SIZE - (uint32_t)sizeof(NTFS_INDEX_HEADER) - 16U;
    ih->leaf_flag = leaf_flag;

    uint32_t total = 16U + (uint32_t)sizeof(NTFS_INDEX_HEADER) + entries_size;
    ah->data.resident.value_length = total;
    ah->data.resident.value_offset = (uint16_t)sizeof(NTFS_ATTRIBUTE_HEADER);
    ah->length = (uint32_t)(sizeof(NTFS_ATTRIBUTE_HEADER) + total);
    return ah->length;
}

static uint32_t write_attr_end(uint8_t* buf)
{
    NTFS_ATTRIBUTE_HEADER* ah = (NTFS_ATTRIBUTE_HEADER*)buf;
    memset(ah, 0, sizeof(NTFS_ATTRIBUTE_HEADER));
    ah->type = NTFS_ATTRIBUTE_END;
    ah->length = (uint32_t)sizeof(NTFS_ATTRIBUTE_HEADER);
    return ah->length;
}

static NTFS_ATTRIBUTE_HEADER* find_attribute(NTFS_FILE_RECORD_HEADER* rec, uint32_t attr_type)
{
    if (rec == NULL) {
        return NULL;
    }
    uint8_t* base = (uint8_t*)rec;
    uint32_t offset = rec->attribute_offset;
    while (offset + sizeof(NTFS_ATTRIBUTE_HEADER) <= rec->bytes_in_use) {
        NTFS_ATTRIBUTE_HEADER* ah = (NTFS_ATTRIBUTE_HEADER*)(base + offset);
        if (ah->type == NTFS_ATTRIBUTE_END) {
            break;
        }
        if (ah->length == 0) {
            break;
        }
        if (ah->type == attr_type) {
            return ah;
        }
        if (offset + ah->length > NTFS_MFT_RECORD_SIZE) {
            break;
        }
        offset += ah->length;
    }
    return NULL;
}

static void* get_attribute_value(NTFS_ATTRIBUTE_HEADER* ah, uint32_t* out_len)
{
    if (ah == NULL) {
        if (out_len != NULL) *out_len = 0;
        return NULL;
    }
    if (ah->non_resident) {
        if (out_len != NULL) {
            *out_len = (uint32_t)ah->data.non_resident.data_size;
        }
        return NULL;
    }
    if (out_len != NULL) {
        *out_len = ah->data.resident.value_length;
    }
    return (uint8_t*)ah + ah->data.resident.value_offset;
}

static uint64_t make_mft_ref(uint32_t idx)
{
    return ((uint64_t)idx) | (1ULL << 48);
}

static uint32_t mft_ref_to_idx(uint64_t ref)
{
    return (uint32_t)(ref & 0xFFFFFFFFULL);
}

typedef struct {
    uint8_t* data_buf;
    uint64_t data_size;
    uint64_t alloc_size;
} ntfs_nonresident_store_t;

#define NTFS_MAX_NONRESIDENT_FILES 64
static ntfs_nonresident_store_t g_nonresident[NTFS_MAX_NONRESIDENT_FILES];
static uint32_t g_nonresident_count = 0;
static uint32_t g_nonresident_mft_map[NTFS_MAX_NONRESIDENT_FILES];

static ntfs_nonresident_store_t* alloc_nonresident(uint32_t mft_idx, uint64_t size)
{
    if (g_nonresident_count >= NTFS_MAX_NONRESIDENT_FILES) {
        return NULL;
    }
    uint32_t slot = g_nonresident_count;
    g_nonresident[slot].data_buf = (uint8_t*)ntfs_kmalloc((uint32_t)size);
    if (g_nonresident[slot].data_buf == NULL) {
        return NULL;
    }
    memset(g_nonresident[slot].data_buf, 0, (size_t)size);
    g_nonresident[slot].data_size = 0;
    g_nonresident[slot].alloc_size = size;
    g_nonresident_mft_map[slot] = mft_idx;
    g_nonresident_count++;
    return &g_nonresident[slot];
}

static ntfs_nonresident_store_t* get_nonresident(uint32_t mft_idx)
{
    uint32_t i;
    for (i = 0; i < g_nonresident_count; i++) {
        if (g_nonresident_mft_map[i] == mft_idx) {
            return &g_nonresident[i];
        }
    }
    return NULL;
}

static void create_standard_metadata_file(ntfs_fs_t* fs, uint32_t mft_idx,
                                           const char* name, int is_dir)
{
    uint8_t* rec_buf = MFT_RECORD_PTR(fs, mft_idx);
    NTFS_FILE_RECORD_HEADER* rec = (NTFS_FILE_RECORD_HEADER*)rec_buf;
    init_file_record_header(rec, mft_idx, is_dir);

    uint32_t attr_id = 0;
    uint32_t start, len;
    uint32_t file_attrs = NTFS_FILE_ATTR_SYSTEM | (is_dir ? NTFS_FILE_ATTR_DIRECTORY : 0);

    uint8_t* attr_buf = append_attribute(rec, &start);
    len = write_attr_standard_info(attr_buf, attr_id++, file_attrs);
    finalize_attribute(rec, start, len);

    uint64_t parent_ref = make_mft_ref(mft_idx);
    if (mft_idx != 5) {
        parent_ref = make_mft_ref(5);
    }

    attr_buf = append_attribute(rec, &start);
    len = write_attr_filename(attr_buf, attr_id++, parent_ref, name, 0, file_attrs);
    finalize_attribute(rec, start, len);

    if (is_dir) {
        attr_buf = append_attribute(rec, &start);
        len = write_attr_index_root(attr_buf, attr_id++, 0, 1);
        finalize_attribute(rec, start, len);
    }

    attr_buf = append_attribute(rec, &start);
    len = write_attr_end(attr_buf);
    finalize_attribute(rec, start, len);

    mft_bitmap_set(mft_idx, 1);
}

static uint32_t create_directory(ntfs_fs_t* fs, uint32_t parent_idx, const char* name)
{
    uint32_t idx = mft_alloc_record();
    if (idx == (uint32_t)-1) {
        return (uint32_t)-1;
    }

    uint8_t* rec_buf = MFT_RECORD_PTR(fs, idx);
    NTFS_FILE_RECORD_HEADER* rec = (NTFS_FILE_RECORD_HEADER*)rec_buf;
    init_file_record_header(rec, idx, 1);

    uint32_t attr_id = 0;
    uint32_t start, len;
    uint32_t file_attrs = 0;
    if (idx < 12) {
        file_attrs = NTFS_FILE_ATTR_SYSTEM;
    }

    uint8_t* attr_buf = append_attribute(rec, &start);
    len = write_attr_standard_info(attr_buf, attr_id++, file_attrs | NTFS_FILE_ATTR_DIRECTORY);
    finalize_attribute(rec, start, len);

    attr_buf = append_attribute(rec, &start);
    len = write_attr_filename(attr_buf, attr_id++, make_mft_ref(parent_idx), name, 0,
                               file_attrs | NTFS_FILE_ATTR_DIRECTORY);
    finalize_attribute(rec, start, len);

    attr_buf = append_attribute(rec, &start);
    len = write_attr_index_root(attr_buf, attr_id++, 0, 1);
    finalize_attribute(rec, start, len);

    attr_buf = append_attribute(rec, &start);
    len = write_attr_end(attr_buf);
    finalize_attribute(rec, start, len);

    return idx;
}

static uint32_t create_file(ntfs_fs_t* fs, uint32_t parent_idx, const char* name,
                             const uint8_t* content, uint32_t content_size)
{
    uint32_t idx = mft_alloc_record();
    if (idx == (uint32_t)-1) {
        return (uint32_t)-1;
    }

    uint8_t* rec_buf = MFT_RECORD_PTR(fs, idx);
    NTFS_FILE_RECORD_HEADER* rec = (NTFS_FILE_RECORD_HEADER*)rec_buf;
    init_file_record_header(rec, idx, 0);

    uint32_t attr_id = 0;
    uint32_t start, len;
    uint32_t file_attrs = NTFS_FILE_ATTR_ARCHIVE;

    uint8_t* attr_buf = append_attribute(rec, &start);
    len = write_attr_standard_info(attr_buf, attr_id++, file_attrs);
    finalize_attribute(rec, start, len);

    attr_buf = append_attribute(rec, &start);
    len = write_attr_filename(attr_buf, attr_id++, make_mft_ref(parent_idx), name,
                               (uint64_t)content_size, file_attrs);
    finalize_attribute(rec, start, len);

    if (content_size <= NTFS_RESIDENT_DATA_LIMIT) {
        attr_buf = append_attribute(rec, &start);
        len = write_attr_data_resident(attr_buf, attr_id++, content, content_size);
        finalize_attribute(rec, start, len);
    } else {
        uint64_t alloc_size = (uint64_t)content_size;
        if (alloc_size < NTFS_NONRESIDENT_BLOCK) {
            alloc_size = NTFS_NONRESIDENT_BLOCK;
        }
        ntfs_nonresident_store_t* store = alloc_nonresident(idx, alloc_size);
        if (store != NULL) {
            if (content != NULL && content_size > 0) {
                memcpy(store->data_buf, content, (size_t)content_size);
            }
            store->data_size = (uint64_t)content_size;
        }
        attr_buf = append_attribute(rec, &start);
        len = write_attr_data_nonresident(attr_buf, attr_id++,
                                           (uint64_t)content_size, alloc_size);
        finalize_attribute(rec, start, len);
    }

    attr_buf = append_attribute(rec, &start);
    len = write_attr_end(attr_buf);
    finalize_attribute(rec, start, len);

    return idx;
}

static void add_dir_entry(ntfs_fs_t* fs, uint32_t dir_idx, uint32_t child_idx, const char* name)
{
    uint8_t* dir_rec_buf = MFT_RECORD_PTR(fs, dir_idx);
    NTFS_FILE_RECORD_HEADER* dir_rec = (NTFS_FILE_RECORD_HEADER*)dir_rec_buf;

    NTFS_ATTRIBUTE_HEADER* idx_root_ah = find_attribute(dir_rec, NTFS_ATTRIBUTE_INDEX_ROOT);
    if (idx_root_ah == NULL) {
        return;
    }

    uint32_t ir_val_len;
    uint8_t* ir_val = (uint8_t*)get_attribute_value(idx_root_ah, &ir_val_len);
    if (ir_val == NULL || ir_val_len < 16U + sizeof(NTFS_INDEX_HEADER)) {
        return;
    }

    NTFS_INDEX_HEADER* ih = (NTFS_INDEX_HEADER*)(ir_val + 16U);
    uint8_t* entries_base = (uint8_t*)ih + ih->entries_offset;
    uint8_t* entries_end = entries_base + ih->index_entries_size;

    uint32_t name_len = (uint32_t)strlen(name);
    if (name_len > NTFS_MAX_FILENAME_LEN) {
        name_len = NTFS_MAX_FILENAME_LEN;
    }
    uint32_t fn_bytes = (uint32_t)sizeof(uint16_t) * name_len;
    uint32_t entry_size = (uint32_t)sizeof(NTFS_INDEX_ENTRY) + fn_bytes;
    entry_size = (entry_size + 7U) & ~7U;

    uint8_t* prev_last_entry = NULL;
    if (ih->index_entries_size > 0) {
        uint8_t* scan = entries_base;
        while (scan < entries_end) {
            NTFS_INDEX_ENTRY* ie = (NTFS_INDEX_ENTRY*)scan;
            if (ie->flags & NTFS_INDEX_ENTRY_LAST) {
                prev_last_entry = scan;
                break;
            }
            if (ie->index_entry_length == 0) {
                break;
            }
            scan += ie->index_entry_length;
        }
    }

    uint32_t old_last_size = 0;
    if (prev_last_entry != NULL) {
        NTFS_INDEX_ENTRY* ie = (NTFS_INDEX_ENTRY*)prev_last_entry;
        old_last_size = ie->index_entry_length;
        ie->flags = (uint16_t)(ie->flags & ~NTFS_INDEX_ENTRY_LAST);
    }

    uint32_t new_entries_size = ih->index_entries_size - old_last_size + entry_size + 16U;
    if (new_entries_size + 16U + sizeof(NTFS_INDEX_HEADER) > NTFS_INDEX_ROOT_SIZE) {
        return;
    }

    uint8_t* new_entry_ptr;
    if (prev_last_entry != NULL) {
        new_entry_ptr = prev_last_entry;
    } else {
        new_entry_ptr = entries_base;
    }

    NTFS_INDEX_ENTRY* new_ie = (NTFS_INDEX_ENTRY*)new_entry_ptr;
    memset(new_ie, 0, entry_size);
    new_ie->mft_record_reference = make_mft_ref(child_idx);
    new_ie->index_entry_length = (uint16_t)entry_size;
    new_ie->filename_length = (uint16_t)name_len;
    new_ie->flags = 0;

    uint16_t* fn_dest = (uint16_t*)new_ie->data;
    ascii_to_utf16(name, fn_dest, name_len);

    uint8_t* last_ptr = new_entry_ptr + entry_size;
    NTFS_INDEX_ENTRY* last_ie = (NTFS_INDEX_ENTRY*)last_ptr;
    memset(last_ie, 0, 16U);
    last_ie->mft_record_reference = 0;
    last_ie->index_entry_length = 16;
    last_ie->filename_length = 0;
    last_ie->flags = NTFS_INDEX_ENTRY_LAST;

    ih->index_entries_size = (uint32_t)((last_ptr + 16U) - entries_base);
    ih->allocated_size = ih->index_entries_size + 1024U;
}

static NTFS_FILE_RECORD_HEADER* get_mft_record(ntfs_fs_t* fs, uint32_t idx)
{
    if (fs == NULL || idx >= NTFS_MFT_MAX_RECORDS) {
        return NULL;
    }
    return (NTFS_FILE_RECORD_HEADER*)MFT_RECORD_PTR(fs, idx);
}

static NTFS_FILE_NAME* get_filename_attr(NTFS_FILE_RECORD_HEADER* rec, uint32_t* out_len)
{
    NTFS_ATTRIBUTE_HEADER* ah = find_attribute(rec, NTFS_ATTRIBUTE_FILE_NAME);
    if (ah == NULL) {
        if (out_len != NULL) *out_len = 0;
        return NULL;
    }
    uint32_t val_len;
    void* val = get_attribute_value(ah, &val_len);
    if (val == NULL || val_len < ATTR_FILENAME_BASE_SIZE) {
        if (out_len != NULL) *out_len = 0;
        return NULL;
    }
    if (out_len != NULL) *out_len = val_len;
    return (NTFS_FILE_NAME*)val;
}

static void copy_filename_ascii(NTFS_FILE_NAME* fn, char* out, uint32_t max_out)
{
    if (fn == NULL || out == NULL || max_out == 0) {
        return;
    }
    uint32_t len = (uint32_t)fn->file_name_length;
    uint32_t i;
    if (len >= max_out) {
        len = max_out - 1U;
    }
    for (i = 0; i < len; i++) {
        uint16_t c = fn->file_name[i];
        out[i] = (c < 0x80U) ? (char)(uint8_t)c : '?';
    }
    out[len] = '\0';
}

static int file_record_is_dir(NTFS_FILE_RECORD_HEADER* rec)
{
    if (rec == NULL) return 0;
    NTFS_FILE_NAME* fn = get_filename_attr(rec, NULL);
    if (fn != NULL) {
        if ((fn->file_attributes & NTFS_FILE_ATTR_DIRECTORY) != 0U) {
            return 1;
        }
    }
    if ((rec->flags & NTFS_MFT_RECORD_DIRECTORY) != 0U) {
        return 1;
    }
    return 0;
}

static vfs_node_t* mft_idx_to_vfs_node(ntfs_fs_t* fs, uint32_t mft_idx)
{
    NTFS_FILE_RECORD_HEADER* rec = get_mft_record(fs, mft_idx);
    if (rec == NULL) {
        return NULL;
    }

    NTFS_FILE_NAME* fn = get_filename_attr(rec, NULL);
    if (fn == NULL) {
        return NULL;
    }

    uint64_t type = file_record_is_dir(rec) ? FS_TYPE_DIRECTORY : FS_TYPE_REGULAR;

    vfs_node_t* node = vfs_create_node("", type);
    if (node == NULL) {
        return NULL;
    }

    copy_filename_ascii(fn, node->name, FS_MAX_NAME);
    node->inode = (uint64_t)mft_idx;
    node->size = fn->data_size;
    node->impl_data = (void*)(uint64_t)mft_idx;
    node->blksize = 4096;
    node->blocks = (node->size + 4095ULL) / 4096ULL;
    node->mode = type == FS_TYPE_DIRECTORY ? 0755ULL : 0644ULL;
    node->nlink = 1;
    node->uid = 0;
    node->gid = 0;
    node->atime = 0;
    node->mtime = 0;
    node->ctime = 0;

    return node;
}

int ntfs_init(void)
{
    memset(g_mft_bitmap, 0, sizeof(g_mft_bitmap));
    g_next_mft_idx = 0;
    g_nonresident_count = 0;
    memset(g_nonresident, 0, sizeof(g_nonresident));
    memset(g_nonresident_mft_map, 0xFF, sizeof(g_nonresident_mft_map));
    g_ntfs_fs = NULL;
    return 0;
}

int ntfs_mount(const char* device)
{
    if (g_ntfs_fs != NULL) {
        return -1;
    }

    ntfs_fs_t* fs = (ntfs_fs_t*)ntfs_kmalloc((uint32_t)sizeof(ntfs_fs_t));
    if (fs == NULL) {
        return -1;
    }
    memset(fs, 0, sizeof(ntfs_fs_t));

    fs->boot_sector = (NTFS_BOOT_SECTOR*)ntfs_kmalloc(NTFS_BOOT_SECTOR_SIZE);
    if (fs->boot_sector == NULL) {
        ntfs_kfree(fs);
        return -1;
    }
    memset(fs->boot_sector, 0, NTFS_BOOT_SECTOR_SIZE);
    memcpy(fs->boot_sector->oem_id, NTFS_OEM_ID, 8);
    fs->boot_sector->bytes_per_sector = 512;
    fs->boot_sector->sectors_per_cluster = 8;
    fs->boot_sector->media_descriptor = 0xF8;
    fs->boot_sector->sectors_per_track = 63;
    fs->boot_sector->number_of_heads = 255;
    fs->boot_sector->total_sectors = (uint64_t)NTFS_VOLUME_BUFFER_SIZE / 512ULL;
    fs->boot_sector->mft_cluster = 4;
    fs->boot_sector->mft_mirror_cluster = (uint64_t)(fs->boot_sector->total_sectors / 2ULL / 8ULL);
    fs->boot_sector->bytes_per_file_record = 10;
    fs->boot_sector->clusters_per_index_buffer = 1;
    fs->boot_sector->volume_serial_number = 0x123456789ABCDEF0ULL;
    fs->boot_sector->signature = 0xAA55;

    fs->cluster_size = (uint64_t)fs->boot_sector->bytes_per_sector *
                       (uint64_t)fs->boot_sector->sectors_per_cluster;
    fs->mft_record_size = 1024ULL;
    fs->index_buffer_size = 4096ULL;
    fs->block_size = 512U;
    fs->total_clusters = fs->boot_sector->total_sectors /
                         (uint64_t)fs->boot_sector->sectors_per_cluster;
    fs->mft_offset = fs->boot_sector->mft_cluster * fs->cluster_size;
    fs->mounted = 0;
    if (device != NULL) {
        size_t dev_len = strlen(device);
        if (dev_len > sizeof(fs->device_name) - 1U) {
            dev_len = sizeof(fs->device_name) - 1U;
        }
        memcpy(fs->device_name, device, dev_len);
        fs->device_name[dev_len] = '\0';
    }

    uint64_t mft_total = (uint64_t)NTFS_MFT_MAX_RECORDS * NTFS_MFT_RECORD_SIZE;
    fs->mft_buffer = (uint8_t*)ntfs_kmalloc((uint32_t)mft_total);
    if (fs->mft_buffer == NULL) {
        ntfs_kfree(fs->boot_sector);
        ntfs_kfree(fs);
        return -1;
    }
    memset(fs->mft_buffer, 0, (size_t)mft_total);

    memset(g_mft_bitmap, 0, sizeof(g_mft_bitmap));
    g_next_mft_idx = 12;
    g_nonresident_count = 0;
    memset(g_nonresident_mft_map, 0xFF, sizeof(g_nonresident_mft_map));

    create_standard_metadata_file(fs, 0, "$MFT", 0);
    create_standard_metadata_file(fs, 1, "$MFTMirr", 0);
    create_standard_metadata_file(fs, 2, "$LogFile", 0);
    create_standard_metadata_file(fs, 3, "$Volume", 0);
    create_standard_metadata_file(fs, 4, "$AttrDef", 0);
    create_standard_metadata_file(fs, 5, ".", 1);
    create_standard_metadata_file(fs, 6, "$Bitmap", 0);
    create_standard_metadata_file(fs, 7, "$Boot", 0);
    create_standard_metadata_file(fs, 8, "$BadClus", 0);
    create_standard_metadata_file(fs, 9, "$Secure", 0);
    create_standard_metadata_file(fs, 10, "$UpCase", 0);
    create_standard_metadata_file(fs, 11, "$Extend", 1);

    uint32_t sysvol_idx = create_directory(fs, 5, "System Volume Information");
    uint32_t progfiles_idx = create_directory(fs, 5, "Program Files");
    uint32_t windows_idx = create_directory(fs, 5, "Windows");
    uint32_t users_idx = create_directory(fs, 5, "Users");
    uint32_t system32_idx = create_directory(fs, windows_idx, "System32");

    if (sysvol_idx != (uint32_t)-1) {
        add_dir_entry(fs, 5, sysvol_idx, "System Volume Information");
    }
    if (progfiles_idx != (uint32_t)-1) {
        add_dir_entry(fs, 5, progfiles_idx, "Program Files");
    }
    if (windows_idx != (uint32_t)-1) {
        add_dir_entry(fs, 5, windows_idx, "Windows");
    }
    if (users_idx != (uint32_t)-1) {
        add_dir_entry(fs, 5, users_idx, "Users");
    }
    if (system32_idx != (uint32_t)-1) {
        add_dir_entry(fs, windows_idx, system32_idx, "System32");
    }

    const char* readme_txt = "KenuxK NTFS Volume\n==================\n"
                              "This is an in-memory NTFS implementation.\n"
                              "Built for the KenuxK operating system.\n";
    uint32_t readme_len = (uint32_t)strlen(readme_txt);

    const char* autoexec_bat = "@echo off\n"
                                "echo KenuxK NTFS Autoexec\n";
    uint32_t autoexec_len = (uint32_t)strlen(autoexec_bat);

    const char* config_sys = "FILES=40\nBUFFERS=20\n";
    uint32_t config_len = (uint32_t)strlen(config_sys);

    char big_buf[2048];
    memset(big_buf, 'A', sizeof(big_buf));
    big_buf[0] = 'B';
    big_buf[sizeof(big_buf) - 1U] = 'Z';

    uint32_t readme_idx = create_file(fs, 5, "readme.txt",
                                       (const uint8_t*)readme_txt, readme_len);
    uint32_t autoexec_idx = create_file(fs, 5, "autoexec.bat",
                                         (const uint8_t*)autoexec_bat, autoexec_len);
    uint32_t config_idx = create_file(fs, 5, "config.sys",
                                       (const uint8_t*)config_sys, config_len);
    uint32_t bigfile_idx = create_file(fs, users_idx, "bigfile.dat",
                                        (const uint8_t*)big_buf, (uint32_t)sizeof(big_buf));

    if (readme_idx != (uint32_t)-1) {
        add_dir_entry(fs, 5, readme_idx, "readme.txt");
    }
    if (autoexec_idx != (uint32_t)-1) {
        add_dir_entry(fs, 5, autoexec_idx, "autoexec.bat");
    }
    if (config_idx != (uint32_t)-1) {
        add_dir_entry(fs, 5, config_idx, "config.sys");
    }
    if (bigfile_idx != (uint32_t)-1) {
        add_dir_entry(fs, users_idx, bigfile_idx, "bigfile.dat");
    }

    const char* ntoskrnl = "NTOSKRNL.EXE placeholder for KenuxK\n";
    uint32_t ntoskrnl_len = (uint32_t)strlen(ntoskrnl);
    const char* hal_dll = "HAL.DLL placeholder\n";
    uint32_t hal_len = (uint32_t)strlen(hal_dll);
    const char* kernel32 = "KERNEL32.DLL placeholder\n";
    uint32_t kernel32_len = (uint32_t)strlen(kernel32);

    uint32_t idx_ntoskrnl = create_file(fs, system32_idx, "ntoskrnl.exe",
                                         (const uint8_t*)ntoskrnl, ntoskrnl_len);
    uint32_t idx_hal = create_file(fs, system32_idx, "hal.dll",
                                    (const uint8_t*)hal_dll, hal_len);
    uint32_t idx_kernel32 = create_file(fs, system32_idx, "kernel32.dll",
                                         (const uint8_t*)kernel32, kernel32_len);

    if (idx_ntoskrnl != (uint32_t)-1) {
        add_dir_entry(fs, system32_idx, idx_ntoskrnl, "ntoskrnl.exe");
    }
    if (idx_hal != (uint32_t)-1) {
        add_dir_entry(fs, system32_idx, idx_hal, "hal.dll");
    }
    if (idx_kernel32 != (uint32_t)-1) {
        add_dir_entry(fs, system32_idx, idx_kernel32, "kernel32.dll");
    }

    const char* pf_txt = "Program Files directory\n";
    uint32_t pf_len = (uint32_t)strlen(pf_txt);
    uint32_t idx_pftxt = create_file(fs, progfiles_idx, "programs.txt",
                                      (const uint8_t*)pf_txt, pf_len);
    if (idx_pftxt != (uint32_t)-1) {
        add_dir_entry(fs, progfiles_idx, idx_pftxt, "programs.txt");
    }

    fs->root_node = mft_idx_to_vfs_node(fs, 5);
    if (fs->root_node == NULL) {
        ntfs_kfree(fs->mft_buffer);
        ntfs_kfree(fs->boot_sector);
        ntfs_kfree(fs);
        return -1;
    }
    if (fs->root_node->name[0] == '\0' ||
        (fs->root_node->name[0] == '.' && fs->root_node->name[1] == '\0')) {
        memcpy(fs->root_node->name, "ntfs_root", 9);
    }

    fs->mounted = 1;
    g_ntfs_fs = fs;
    return 0;
}

vfs_node_t* ntfs_lookup(vfs_node_t* parent, const char* name)
{
    if (g_ntfs_fs == NULL || parent == NULL || name == NULL) {
        return NULL;
    }

    uint32_t parent_idx = (uint32_t)(uint64_t)parent->impl_data;
    NTFS_FILE_RECORD_HEADER* parent_rec = get_mft_record(g_ntfs_fs, parent_idx);
    if (parent_rec == NULL) {
        return NULL;
    }

    NTFS_ATTRIBUTE_HEADER* idx_root_ah = find_attribute(parent_rec, NTFS_ATTRIBUTE_INDEX_ROOT);
    if (idx_root_ah == NULL) {
        return NULL;
    }

    uint32_t ir_val_len;
    uint8_t* ir_val = (uint8_t*)get_attribute_value(idx_root_ah, &ir_val_len);
    if (ir_val == NULL || ir_val_len < 16U + sizeof(NTFS_INDEX_HEADER)) {
        return NULL;
    }

    NTFS_INDEX_HEADER* ih = (NTFS_INDEX_HEADER*)(ir_val + 16U);
    uint8_t* entries_base = (uint8_t*)ih + ih->entries_offset;
    uint8_t* entries_end = entries_base + ih->index_entries_size;
    uint8_t* scan = entries_base;

    while (scan + sizeof(NTFS_INDEX_ENTRY) <= entries_end) {
        NTFS_INDEX_ENTRY* ie = (NTFS_INDEX_ENTRY*)scan;
        if (ie->index_entry_length == 0) {
            break;
        }
        if (ie->flags & NTFS_INDEX_ENTRY_LAST) {
            break;
        }
        if (ie->filename_length > 0) {
            uint16_t* fn16 = (uint16_t*)ie->data;
            if (utf16_ascii_cmp(fn16, name, (uint32_t)ie->filename_length) == 0) {
                uint32_t child_idx = mft_ref_to_idx(ie->mft_record_reference);
                return mft_idx_to_vfs_node(g_ntfs_fs, child_idx);
            }
        }
        scan += ie->index_entry_length;
    }

    return NULL;
}

int ntfs_readdir(vfs_node_t* dir, vfs_dirent_t* out, uint32_t count)
{
    if (g_ntfs_fs == NULL || dir == NULL || out == NULL || count == 0) {
        return 0;
    }

    uint32_t dir_idx = (uint32_t)(uint64_t)dir->impl_data;
    NTFS_FILE_RECORD_HEADER* dir_rec = get_mft_record(g_ntfs_fs, dir_idx);
    if (dir_rec == NULL) {
        return 0;
    }

    NTFS_ATTRIBUTE_HEADER* idx_root_ah = find_attribute(dir_rec, NTFS_ATTRIBUTE_INDEX_ROOT);
    if (idx_root_ah == NULL) {
        return 0;
    }

    uint32_t ir_val_len;
    uint8_t* ir_val = (uint8_t*)get_attribute_value(idx_root_ah, &ir_val_len);
    if (ir_val == NULL || ir_val_len < 16U + sizeof(NTFS_INDEX_HEADER)) {
        return 0;
    }

    NTFS_INDEX_HEADER* ih = (NTFS_INDEX_HEADER*)(ir_val + 16U);
    uint8_t* entries_base = (uint8_t*)ih + ih->entries_offset;
    uint8_t* entries_end = entries_base + ih->index_entries_size;
    uint8_t* scan = entries_base;

    uint32_t filled = 0;

    while (filled < count && scan + sizeof(NTFS_INDEX_ENTRY) <= entries_end) {
        NTFS_INDEX_ENTRY* ie = (NTFS_INDEX_ENTRY*)scan;
        if (ie->index_entry_length == 0) {
            break;
        }
        if (ie->flags & NTFS_INDEX_ENTRY_LAST) {
            break;
        }

        if (ie->filename_length > 0) {
            uint32_t child_idx = mft_ref_to_idx(ie->mft_record_reference);
            NTFS_FILE_RECORD_HEADER* child_rec = get_mft_record(g_ntfs_fs, child_idx);
            NTFS_FILE_NAME* child_fn = NULL;
            uint64_t child_size = 0;
            int is_dir = 0;

            if (child_rec != NULL) {
                child_fn = get_filename_attr(child_rec, NULL);
                if (child_fn != NULL) {
                    child_size = child_fn->data_size;
                    if ((child_fn->file_attributes & NTFS_FILE_ATTR_DIRECTORY) != 0U) {
                        is_dir = 1;
                    }
                }
                if ((child_rec->flags & NTFS_MFT_RECORD_DIRECTORY) != 0U) {
                    is_dir = 1;
                }
            }

            memset(&out[filled], 0, sizeof(vfs_dirent_t));
            out[filled].inode = (uint64_t)child_idx;
            out[filled].type = is_dir ? (uint64_t)FS_TYPE_DIRECTORY : (uint64_t)FS_TYPE_REGULAR;
            out[filled].size = child_size;

            uint32_t i;
            uint16_t* fn16 = (uint16_t*)ie->data;
            uint32_t fn_len = (uint32_t)ie->filename_length;
            if (fn_len >= FS_MAX_NAME) {
                fn_len = (uint32_t)FS_MAX_NAME - 1U;
            }
            for (i = 0; i < fn_len; i++) {
                uint16_t c = fn16[i];
                out[filled].name[i] = (c < 0x80U) ? (char)(uint8_t)c : '?';
            }
            out[filled].name[i] = '\0';

            filled++;
        }

        scan += ie->index_entry_length;
    }

    return (int)filled;
}

int ntfs_read(vfs_node_t* node, uint64_t offset, uint32_t size, uint8_t* buf)
{
    if (g_ntfs_fs == NULL || node == NULL || buf == NULL || size == 0) {
        return 0;
    }
    if (offset >= node->size) {
        return 0;
    }

    uint32_t mft_idx = (uint32_t)(uint64_t)node->impl_data;
    NTFS_FILE_RECORD_HEADER* rec = get_mft_record(g_ntfs_fs, mft_idx);
    if (rec == NULL) {
        return 0;
    }

    NTFS_ATTRIBUTE_HEADER* data_ah = find_attribute(rec, NTFS_ATTRIBUTE_DATA);
    if (data_ah == NULL) {
        return 0;
    }

    uint64_t remaining = node->size - offset;
    uint64_t to_copy_u64 = (uint64_t)size;
    if (to_copy_u64 > remaining) {
        to_copy_u64 = remaining;
    }
    uint32_t to_copy = (uint32_t)to_copy_u64;

    if (!data_ah->non_resident) {
        uint32_t val_len;
        uint8_t* val = (uint8_t*)get_attribute_value(data_ah, &val_len);
        if (val == NULL) {
            return 0;
        }
        uint64_t val_remaining = (uint64_t)val_len;
        if (offset >= val_remaining) {
            return 0;
        }
        uint64_t val_avail = val_remaining - offset;
        if ((uint64_t)to_copy > val_avail) {
            to_copy = (uint32_t)val_avail;
        }
        memcpy(buf, val + offset, (size_t)to_copy);
        return (int)to_copy;
    } else {
        ntfs_nonresident_store_t* store = get_nonresident(mft_idx);
        if (store == NULL || store->data_buf == NULL) {
            return 0;
        }
        if (offset >= store->data_size) {
            return 0;
        }
        uint64_t store_avail = store->data_size - offset;
        if ((uint64_t)to_copy > store_avail) {
            to_copy = (uint32_t)store_avail;
        }
        memcpy(buf, store->data_buf + offset, (size_t)to_copy);
        return (int)to_copy;
    }
}
