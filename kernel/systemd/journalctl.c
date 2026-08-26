#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>

#define JOURNALCTL_MAX_ARGS 64

static void serial_printf(const char* format, ...)
{
    (void)format;
}

static void journalctl_usage(const char* prog)
{
    serial_printf("Usage: %s [OPTIONS]\n\n", prog);
    serial_printf("Options:\n");
    serial_printf("  -n <num>              Show last n entries\n");
    serial_printf("  -f                    Follow journal output\n");
    serial_printf("  -u <unit>             Filter by unit\n");
    serial_printf("  -p <priority>         Filter by priority (0-7)\n");
    serial_printf("  --since <time>        Show entries since time\n");
    serial_printf("  --until <time>        Show entries until time\n");
    serial_printf("  -r                    Reverse order (newest first)\n");
    serial_printf("  -a                    Show all fields\n");
    serial_printf("  --list-boots          List boot timestamps\n");
    serial_printf("  --vacuum-size <size>  Reduce journal size\n");
    serial_printf("  --clear               Clear all journal entries\n");
    serial_printf("  -h, --help            Show this help\n");
    serial_printf("\n");
}

static const char* priority_to_string(int priority)
{
    switch (priority) {
        case LOG_EMERG:  return "emerg";
        case LOG_ALERT:  return "alert";
        case LOG_CRIT:   return "crit";
        case LOG_ERR:    return "err";
        case LOG_WARNING:return "warn";
        case LOG_NOTICE: return "notice";
        case LOG_INFO:   return "info";
        case LOG_DEBUG:  return "debug";
        default:         return "unknown";
    }
}

static void journalctl_print_entry(const journal_entry_t* entry, bool show_all)
{
    char time_str[64];
    snprintf(time_str, sizeof(time_str), "%llu", entry->timestamp);
    
    const char* prio_str = priority_to_string(entry->priority);
    
    if (show_all) {
        serial_printf("%s %s %s[%llu]: %s\n",
                      time_str,
                      prio_str,
                      entry->comm,
                      entry->pid,
                      entry->message);
    } else {
        serial_printf("%s %s: %s\n",
                      time_str,
                      entry->unit,
                      entry->message);
    }
}

static int parse_time(const char* time_str)
{
    return atoi(time_str);
}

int journalctl_main(int argc, char** argv)
{
    journal_query_t query;
    memset(&query, 0, sizeof(query));
    
    query.priority = -1;
    query.reverse = false;
    
    int lines = 10;
    bool follow = false;
    bool show_all = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 < argc) {
                lines = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-f") == 0) {
            follow = true;
        } else if (strcmp(argv[i], "-u") == 0) {
            if (i + 1 < argc) {
                strncpy(query.unit, argv[++i], SYSTEMD_MAX_NAME - 1);
            }
        } else if (strcmp(argv[i], "-p") == 0) {
            if (i + 1 < argc) {
                query.priority = atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "--since") == 0) {
            if (i + 1 < argc) {
                query.since = parse_time(argv[++i]);
            }
        } else if (strcmp(argv[i], "--until") == 0) {
            if (i + 1 < argc) {
                query.until = parse_time(argv[++i]);
            }
        } else if (strcmp(argv[i], "-r") == 0) {
            query.reverse = true;
        } else if (strcmp(argv[i], "-a") == 0) {
            show_all = true;
        } else if (strcmp(argv[i], "--list-boots") == 0) {
            serial_printf("Boot 0: %llu\n", time_get_timestamp());
            return 0;
        } else if (strcmp(argv[i], "--vacuum-size") == 0) {
            if (i + 1 < argc) {
                journal_clear();
                serial_printf("Journal cleared.\n");
            }
            return 0;
        } else if (strcmp(argv[i], "--clear") == 0) {
            journal_clear();
            serial_printf("Journal cleared.\n");
            return 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            journalctl_usage(argv[0]);
            return 0;
        }
    }
    
    journal_entry_t entries[256];
    int count = journal_query(&query, entries, lines);
    
    for (int i = 0; i < count; i++) {
        journalctl_print_entry(&entries[i], show_all);
    }
    
    if (follow) {
        uint64_t last_timestamp = time_get_timestamp();
        
        while (true) {
            memset(&query, 0, sizeof(query));
            query.since = last_timestamp;
            
            count = journal_query(&query, entries, 256);
            
            for (int i = 0; i < count; i++) {
                journalctl_print_entry(&entries[i], show_all);
                if (entries[i].timestamp > last_timestamp) {
                    last_timestamp = entries[i].timestamp;
                }
            }
            
            msleep(1000);
        }
    }
    
    return 0;
}