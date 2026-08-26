#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>

static uint32_t crc32_table[256];

static void crc32_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
}

static uint32_t crc32(const void* data, uint32_t length)
{
    static bool initialized = false;
    if (!initialized) {
        crc32_init();
        initialized = true;
    }
    
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = (const uint8_t*)data;
    
    for (uint32_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

#define JOURNAL_MAGIC 0x6A6F75726E616C21LL
#define JOURNAL_HEADER_SIZE 64
#define JOURNAL_FILE_PATH "/var/log/journal/systemd.journal"
#define JOURNAL_MAX_FILE_SIZE (1024 * 1024 * 10)
#define JOURNAL_MAX_ENTRIES 4096
#define SYSTEMD_MAX_MESSAGE 2048
#define SEEK_SET 0
#define SEEK_END 2

static void serial_printf(const char* format, ...)
{
    (void)format;
}

static int vfs_sync(int fd)
{
    (void)fd;
    return 0;
}

typedef struct {
    uint64_t magic;
    uint64_t version;
    uint64_t header_size;
    uint64_t entry_count;
    uint64_t file_size;
    uint64_t created_time;
    uint64_t modified_time;
    uint32_t crc32;
} journal_file_header_t;

typedef struct {
    uint64_t offset;
    uint64_t timestamp;
    uint32_t priority;
    uint32_t unit_len;
} journal_index_entry_t;

typedef struct {
    journal_index_entry_t entries[JOURNAL_MAX_ENTRIES];
    int count;
    uint64_t last_timestamp;
} journal_index_t;

static bool journal_initialized = false;
static bool journal_persist = true;
static uint64_t journal_file_size = 0;
static journal_index_t journal_index;
static spinlock_t journal_lock;
static int journal_fd = -1;
static char journal_current_file[SYSTEMD_MAX_PATH_LEN];
static int journal_file_number = 0;

static void journal_init_index(void)
{
    memset(&journal_index, 0, sizeof(journal_index));
    journal_index.last_timestamp = 0;
}

static uint32_t journal_crc32(const void* data, size_t len)
{
    return crc32(data, len);
}

static void journal_rotate(void)
{
    if (journal_fd >= 0) {
        vfs_close(journal_fd);
        journal_fd = -1;
    }
    
    char rotated_file[SYSTEMD_MAX_PATH_LEN];
    sprintf(rotated_file, "%s.%d", JOURNAL_FILE_PATH, journal_file_number++);
    vfs_rename(JOURNAL_FILE_PATH, rotated_file);
    
    journal_file_size = 0;
    journal_init_index();
    
    journal_fd = vfs_open(JOURNAL_FILE_PATH, FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC, 0644);
    if (journal_fd >= 0) {
        journal_file_header_t header = {
            .magic = JOURNAL_MAGIC,
            .version = 1,
            .header_size = JOURNAL_HEADER_SIZE,
            .entry_count = 0,
            .file_size = JOURNAL_HEADER_SIZE,
            .created_time = time_get_timestamp(),
            .modified_time = time_get_timestamp(),
            .crc32 = 0
        };
        
        header.crc32 = journal_crc32(&header, JOURNAL_HEADER_SIZE - sizeof(uint32_t));
        
        vfs_write(journal_fd, &header, JOURNAL_HEADER_SIZE);
        journal_file_size = JOURNAL_HEADER_SIZE;
    }
    
    char msg[256];
    sprintf(msg, "Rotated journal to %s", rotated_file);
    serial_printf("[journal] %s\n", msg);
}

static void journal_update_header(void)
{
    if (journal_fd < 0) return;
    
    journal_file_header_t header = {
        .magic = JOURNAL_MAGIC,
        .version = 1,
        .header_size = JOURNAL_HEADER_SIZE,
        .entry_count = journal_index.count,
        .file_size = journal_file_size,
        .created_time = time_get_timestamp(),
        .modified_time = time_get_timestamp(),
        .crc32 = 0
    };
    
    header.crc32 = journal_crc32(&header, JOURNAL_HEADER_SIZE - sizeof(uint32_t));
    
    vfs_lseek(journal_fd, 0, SEEK_SET);
    vfs_write(journal_fd, &header, JOURNAL_HEADER_SIZE);
}

static int journal_write_binary(const journal_entry_t* entry)
{
    if (!journal_persist) return 0;
    
    spinlock_lock(&journal_lock);
    
    if (journal_fd < 0) {
        journal_fd = vfs_open(JOURNAL_FILE_PATH, FS_O_WRONLY | FS_O_CREAT | FS_O_APPEND, 0644);
        if (journal_fd < 0) {
            spinlock_unlock(&journal_lock);
            return -1;
        }
        
        if (vfs_lseek(journal_fd, 0, SEEK_END) == 0) {
            journal_file_header_t header = {
                .magic = JOURNAL_MAGIC,
                .version = 1,
                .header_size = JOURNAL_HEADER_SIZE,
                .entry_count = 0,
                .file_size = JOURNAL_HEADER_SIZE,
                .created_time = time_get_timestamp(),
                .modified_time = time_get_timestamp(),
                .crc32 = 0
            };
            
            header.crc32 = journal_crc32(&header, JOURNAL_HEADER_SIZE - sizeof(uint32_t));
            
            vfs_write(journal_fd, &header, JOURNAL_HEADER_SIZE);
            journal_file_size = JOURNAL_HEADER_SIZE;
        } else {
            journal_file_size = vfs_lseek(journal_fd, 0, SEEK_END);
        }
    }
    
    if (journal_file_size >= JOURNAL_MAX_FILE_SIZE) {
        journal_rotate();
    }
    
    uint64_t offset = journal_file_size;
    
    uint32_t unit_len = strlen(entry->unit);
    uint32_t msg_len = strlen(entry->message);
    uint32_t comm_len = strlen(entry->comm);
    
    size_t data_len = sizeof(uint64_t) + sizeof(uint32_t) * 4 + sizeof(uint64_t) + 
                      unit_len + 1 + msg_len + 1 + comm_len + 1;
    
    char* buffer = (char*)malloc(data_len);
    if (!buffer) {
        spinlock_unlock(&journal_lock);
        return -1;
    }
    
    char* ptr = buffer;
    *(uint64_t*)ptr = entry->timestamp; ptr += sizeof(uint64_t);
    *(uint32_t*)ptr = entry->priority; ptr += sizeof(uint32_t);
    *(uint32_t*)ptr = unit_len; ptr += sizeof(uint32_t);
    *(uint32_t*)ptr = msg_len; ptr += sizeof(uint32_t);
    *(uint32_t*)ptr = comm_len; ptr += sizeof(uint32_t);
    *(uint64_t*)ptr = entry->pid; ptr += sizeof(uint64_t);
    memcpy(ptr, entry->unit, unit_len + 1); ptr += unit_len + 1;
    memcpy(ptr, entry->message, msg_len + 1); ptr += msg_len + 1;
    memcpy(ptr, entry->comm, comm_len + 1);
    
    vfs_lseek(journal_fd, offset, SEEK_SET);
    vfs_write(journal_fd, buffer, data_len);
    
    free(buffer);
    
    if (journal_index.count < JOURNAL_MAX_ENTRIES) {
        journal_index.entries[journal_index.count].offset = offset;
        journal_index.entries[journal_index.count].timestamp = entry->timestamp;
        journal_index.entries[journal_index.count].priority = entry->priority;
        journal_index.entries[journal_index.count].unit_len = unit_len;
        journal_index.count++;
        journal_index.last_timestamp = entry->timestamp;
    }
    
    journal_file_size += data_len;
    journal_update_header();
    
    spinlock_unlock(&journal_lock);
    
    return 0;
}

static int journal_read_binary(uint64_t offset, journal_entry_t* entry)
{
    if (journal_fd < 0) {
        journal_fd = vfs_open(JOURNAL_FILE_PATH, FS_O_RDONLY, 0);
        if (journal_fd < 0) return -1;
    }
    
    vfs_lseek(journal_fd, offset, SEEK_SET);
    
    vfs_read(journal_fd, &entry->timestamp, sizeof(uint64_t));
    vfs_read(journal_fd, &entry->priority, sizeof(uint32_t));
    
    uint32_t unit_len, msg_len, comm_len;
    vfs_read(journal_fd, &unit_len, sizeof(uint32_t));
    vfs_read(journal_fd, &msg_len, sizeof(uint32_t));
    vfs_read(journal_fd, &comm_len, sizeof(uint32_t));
    vfs_read(journal_fd, &entry->pid, sizeof(uint64_t));
    
    vfs_read(journal_fd, entry->unit, unit_len + 1);
    vfs_read(journal_fd, entry->message, msg_len + 1);
    vfs_read(journal_fd, entry->comm, comm_len + 1);
    
    return 0;
}

void journal_log(const char* unit, const char* message, int priority)
{
    if (!journal_initialized) return;
    
    journal_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    
    entry.timestamp = time_get_timestamp();
    entry.priority = priority;
    entry.pid = 0;
    
    strncpy(entry.unit, unit, SYSTEMD_MAX_NAME - 1);
    strncpy(entry.message, message, SYSTEMD_MAX_MESSAGE - 1);
    strncpy(entry.comm, "systemd", SYSTEMD_MAX_NAME - 1);
    
    journal_write_binary(&entry);
    
    const char* prefix;
    switch (priority) {
        case LOG_EMERG:  prefix = "[EMERG]";  break;
        case LOG_ALERT:  prefix = "[ALERT]";  break;
        case LOG_CRIT:   prefix = "[CRIT]";   break;
        case LOG_ERR:    prefix = "[ERR]";    break;
        case LOG_WARNING:prefix = "[WARN]";   break;
        case LOG_NOTICE: prefix = "[NOTICE]"; break;
        case LOG_INFO:   prefix = "[INFO]";   break;
        case LOG_DEBUG:  prefix = "[DEBUG]";  break;
        default:         prefix = "[UNKNOWN]";
    }
    
    serial_printf("[journal] %s %s: %s\n", prefix, unit, message);
}

void journal_init(bool persist)
{
    spinlock_init(&journal_lock);
    journal_init_index();
    journal_persist = persist;
    journal_initialized = true;
    
    vfs_mkdir("/var/log", 0755);
    vfs_mkdir("/var/log/journal", 0755);
    vfs_mkdir("/run/systemd/journal", 0755);
    
    journal_fd = vfs_open(JOURNAL_FILE_PATH, FS_O_RDWR | FS_O_CREAT, 0644);
    if (journal_fd >= 0) {
        journal_file_header_t header;
        if (vfs_read(journal_fd, &header, JOURNAL_HEADER_SIZE) == JOURNAL_HEADER_SIZE) {
            if (header.magic == JOURNAL_MAGIC) {
                journal_file_size = header.file_size;
            }
        }
        vfs_close(journal_fd);
        journal_fd = -1;
    }
    
    journal_log("systemd", "Journal initialized", LOG_INFO);
}

void journal_flush(void)
{
    if (journal_fd >= 0) {
        vfs_sync(journal_fd);
    }
}

void journal_close(void)
{
    journal_flush();
    
    if (journal_fd >= 0) {
        vfs_close(journal_fd);
        journal_fd = -1;
    }
    
    journal_initialized = false;
}

int journal_query(journal_query_t* query, journal_entry_t* entries, int max_entries)
{
    if (!query || !entries || max_entries <= 0) return -1;
    
    int count = 0;
    int start = (query->reverse) ? journal_index.count - 1 : 0;
    int end = (query->reverse) ? -1 : journal_index.count;
    int step = (query->reverse) ? -1 : 1;
    
    spinlock_lock(&journal_lock);
    
    for (int i = start; (step > 0 ? i < end : i > end); i += step) {
        if (count >= max_entries) break;
        
        journal_index_entry_t* idx_entry = &journal_index.entries[i];
        
        if (query->priority >= 0 && idx_entry->priority > query->priority) {
            continue;
        }
        
        if (query->unit && strlen(query->unit) > 0) {
            journal_entry_t temp;
            if (journal_read_binary(idx_entry->offset, &temp) == 0) {
                if (strcmp(temp.unit, query->unit) != 0) {
                    continue;
                }
            }
        }
        
        if (query->since > 0 && idx_entry->timestamp < query->since) {
            if (query->reverse) break;
            continue;
        }
        
        if (query->until > 0 && idx_entry->timestamp > query->until) {
            if (!query->reverse) break;
            continue;
        }
        
        if (journal_read_binary(idx_entry->offset, &entries[count]) == 0) {
            count++;
        }
    }
    
    spinlock_unlock(&journal_lock);
    
    return count;
}

int journal_count(journal_query_t* query)
{
    if (!query) return -1;
    
    int count = 0;
    
    spinlock_lock(&journal_lock);
    
    for (int i = 0; i < journal_index.count; i++) {
        journal_index_entry_t* idx_entry = &journal_index.entries[i];
        
        if (query->priority >= 0 && idx_entry->priority > query->priority) {
            continue;
        }
        
        if (query->unit && strlen(query->unit) > 0) {
            journal_entry_t temp;
            if (journal_read_binary(idx_entry->offset, &temp) == 0) {
                if (strcmp(temp.unit, query->unit) != 0) {
                    continue;
                }
            }
        }
        
        if (query->since > 0 && idx_entry->timestamp < query->since) {
            continue;
        }
        
        if (query->until > 0 && idx_entry->timestamp > query->until) {
            continue;
        }
        
        count++;
    }
    
    spinlock_unlock(&journal_lock);
    
    return count;
}

void journal_clear(void)
{
    spinlock_lock(&journal_lock);
    
    if (journal_fd >= 0) {
        vfs_close(journal_fd);
        journal_fd = -1;
    }
    
    vfs_unlink(JOURNAL_FILE_PATH);
    
    journal_file_size = 0;
    journal_init_index();
    
    spinlock_unlock(&journal_lock);
    
    journal_log("systemd", "Journal cleared", LOG_INFO);
}