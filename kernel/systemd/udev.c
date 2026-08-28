#include <systemd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arch/fs.h>
#include <arch/pci.h>
#include <ctype.h>
#include <stdlib.h>

#define UDEV_MAX_RULES 128
#define UDEV_MAX_DEVICES 256
#define UDEV_MAX_SUBSYSTEMS 16

static long strtol(const char* str, char** endptr, int base)
{
    (void)endptr;
    long result = 0;
    int sign = 1;
    
    while (*str == ' ' || *str == '\t') str++;
    
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    while (*str) {
        char c = *str;
        int digit;
        
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (base == 16 && c >= 'a' && c <= 'f') {
            digit = 10 + c - 'a';
        } else if (base == 16 && c >= 'A' && c <= 'F') {
            digit = 10 + c - 'A';
        } else {
            break;
        }
        
        if (digit >= base) break;
        
        result = result * base + digit;
        str++;
    }
    
    return sign * result;
}

static int udev_vfs_mknod(const char* path, int type, uint64_t major, uint64_t minor)
{
    (void)path; (void)type; (void)major; (void)minor;
    journal_log("udev", "udev_vfs_mknod not implemented", LOG_WARNING);
    return -1;
}

static int udev_vfs_symlink(const char* target, const char* linkpath)
{
    (void)target; (void)linkpath;
    journal_log("udev", "udev_vfs_symlink not implemented", LOG_WARNING);
    return -1;
}

typedef enum {
    UDEV_ACTION_ADD,
    UDEV_ACTION_REMOVE,
    UDEV_ACTION_CHANGE,
} udev_action_t;

typedef struct {
    char subsystem[64];
    char vendor[32];
    char device[32];
    char action[16];
    char program[256];
    char env[128];
    char name[64];
    char symlink[128];
    char driver[64];
    char group[32];
    int mode;
} udev_rule_t;

typedef struct {
    char devpath[SYSTEMD_MAX_PATH];
    char subsystem[64];
    char sysname[64];
    char devname[64];
    char vendor[32];
    char device[32];
    int major;
    int minor;
    uint64_t pci_addr;
    uint16_t pci_vendor_id;
    uint16_t pci_device_id;
    uint8_t pci_class;
    uint8_t pci_subclass;
    char driver[64];
} udev_device_t;

static udev_rule_t udev_rules[UDEV_MAX_RULES];
static udev_device_t udev_devices[UDEV_MAX_DEVICES];
static int udev_rule_count = 0;
static int udev_device_count = 0;
static spinlock_t udev_lock;

static const char* udev_subsystems[UDEV_MAX_SUBSYSTEMS] = {
    "block", "char", "net", "usb", "pci", "input", "sound", "drm"
};

static int parse_udev_rule(const char* line)
{
    if (udev_rule_count >= UDEV_MAX_RULES) return -1;
    
    udev_rule_t* rule = &udev_rules[udev_rule_count];
    memset(rule, 0, sizeof(udev_rule_t));
    rule->mode = 0644;
    
    char copy[2048];
    strncpy(copy, line, sizeof(copy) - 1);
    
    char* token = strtok(copy, " \t");
    while (token) {
        if (strncmp(token, "SUBSYSTEM==", 11) == 0) {
            int len = strlen(token);
            if (len > 12) strncpy(rule->subsystem, token + 11, len - 13);
        } else if (strncmp(token, "SUBSYSTEMS==", 12) == 0) {
            int len = strlen(token);
            if (len > 13) strncpy(rule->subsystem, token + 12, len - 14);
        } else if (strncmp(token, "ATTR{vendor}==", 12) == 0) {
            int len = strlen(token);
            if (len > 13) strncpy(rule->vendor, token + 12, len - 14);
        } else if (strncmp(token, "ATTR{device}==", 12) == 0) {
            int len = strlen(token);
            if (len > 13) strncpy(rule->device, token + 12, len - 14);
        } else if (strncmp(token, "ACTION==", 8) == 0) {
            int len = strlen(token);
            if (len > 9) strncpy(rule->action, token + 8, len - 10);
        } else if (strncmp(token, "RUN+=", 5) == 0) {
            int len = strlen(token);
            if (len > 6) strncpy(rule->program, token + 5, len - 7);
        } else if (strncmp(token, "NAME=", 5) == 0) {
            int len = strlen(token);
            if (len > 6) strncpy(rule->name, token + 5, len - 7);
        } else if (strncmp(token, "SYMLINK+=", 10) == 0) {
            int len = strlen(token);
            if (len > 11) strncpy(rule->symlink, token + 10, len - 12);
        } else if (strncmp(token, "DRIVER==", 9) == 0) {
            int len = strlen(token);
            if (len > 10) strncpy(rule->driver, token + 9, len - 11);
        } else if (strncmp(token, "GROUP=", 7) == 0) {
            int len = strlen(token);
            if (len > 8) strncpy(rule->group, token + 7, len - 9);
        } else if (strncmp(token, "MODE=", 6) == 0) {
            int len = strlen(token);
            if (len > 7) rule->mode = strtol(token + 6, NULL, 8);
        }
        token = strtok(NULL, " \t");
    }
    
    udev_rule_count++;
    return 0;
}

static int udev_match_rule(udev_rule_t* rule, udev_device_t* device)
{
    if (strlen(rule->subsystem) > 0) {
        if (strcmp(rule->subsystem, device->subsystem) != 0) {
            return 0;
        }
    }
    if (strlen(rule->vendor) > 0) {
        char vendor_hex[16];
        sprintf(vendor_hex, "%04x", device->pci_vendor_id);
        if (strcmp(rule->vendor, vendor_hex) != 0) {
            return 0;
        }
    }
    if (strlen(rule->device) > 0) {
        char device_hex[16];
        sprintf(device_hex, "%04x", device->pci_device_id);
        if (strcmp(rule->device, device_hex) != 0) {
            return 0;
        }
    }
    if (strlen(rule->driver) > 0) {
        if (strcmp(rule->driver, device->driver) != 0) {
            return 0;
        }
    }
    return 1;
}

static int udev_create_device_node(udev_device_t* device, const char* name)
{
    char devpath[SYSTEMD_MAX_PATH];
    sprintf(devpath, "/dev/%s", name);
    
    char msg[256];
    sprintf(msg, "Creating device node %s", devpath);
    journal_log("udev", msg, LOG_INFO);
    
    udev_vfs_mknod(devpath, FS_TYPE_CHARDEV, device->major, device->minor);
    
    return 0;
}

static int udev_create_symlink(udev_device_t* device, const char* symlink)
{
    char target[SYSTEMD_MAX_PATH];
    sprintf(target, "/dev/%s", device->devname);
    
    char linkpath[SYSTEMD_MAX_PATH];
    sprintf(linkpath, "/dev/%s", symlink);
    
    udev_vfs_symlink(target, linkpath);
    
    char msg[256];
    sprintf(msg, "Creating symlink %s -> %s", linkpath, target);
    journal_log("udev", msg, LOG_INFO);
    
    return 0;
}

static void udev_create_sysfs_node(udev_device_t* device)
{
    char sysdir[SYSTEMD_MAX_PATH];
    sprintf(sysdir, "/sys/%s/%s", device->subsystem, device->sysname);
    vfs_mkdir(sysdir, 0755);
    
    char syspath[SYSTEMD_MAX_PATH];
    sprintf(syspath, "%s/vendor", sysdir);
    int fd = vfs_open(syspath, FS_O_WRONLY | FS_O_CREAT, 0644);
    if (fd >= 0) {
        char vendor[16];
        sprintf(vendor, "%04x\n", device->pci_vendor_id);
        vfs_write(fd, vendor, strlen(vendor));
        vfs_close(fd);
    }
    
    sprintf(syspath, "%s/device", sysdir);
    fd = vfs_open(syspath, FS_O_WRONLY | FS_O_CREAT, 0644);
    if (fd >= 0) {
        char dev_id[16];
        sprintf(dev_id, "%04x\n", device->pci_device_id);
        vfs_write(fd, dev_id, strlen(dev_id));
        vfs_close(fd);
    }
    
    sprintf(syspath, "%s/class", sysdir);
    fd = vfs_open(syspath, FS_O_WRONLY | FS_O_CREAT, 0644);
    if (fd >= 0) {
        char class[16];
        sprintf(class, "%02x\n", device->pci_class);
        vfs_write(fd, class, strlen(class));
        vfs_close(fd);
    }
}

static void udev_apply_rules(udev_device_t* device)
{
    for (int i = 0; i < udev_rule_count; i++) {
        udev_rule_t* rule = &udev_rules[i];
        if (udev_match_rule(rule, device)) {
            if (strlen(rule->name) > 0) {
                strncpy(device->devname, rule->name, 63);
                udev_create_device_node(device, rule->name);
            }
            if (strlen(rule->symlink) > 0) {
                udev_create_symlink(device, rule->symlink);
            }
            if (strlen(rule->program) > 0) {
                exec_command_t cmd;
                parse_exec_command(rule->program, &cmd);
                service_run_command(&cmd, "/");
            }
        }
    }
}

static const char* pci_class_to_subsystem(uint8_t class, uint8_t subclass)
{
    switch (class) {
        case 0x01: return "block";
        case 0x02: return "net";
        case 0x03: return "drm";
        case 0x04: return "sound";
        case 0x06: return "pci";
        case 0x07: return "char";
        case 0x09: return "usb";
        case 0x0C: return "usb";
        case 0x0D: return "char";
        case 0x0E: return "char";
        case 0x0F: return "char";
        case 0x10: return "char";
        case 0x11: return "char";
        case 0x12: return "char";
        default: return "misc";
    }
}

static const char* pci_vendor_to_name(uint16_t vendor_id)
{
    switch (vendor_id) {
        case 0x8086: return "intel";
        case 0x10DE: return "nvidia";
        case 0x1002: return "amd";
        case 0x14E4: return "broadcom";
        case 0x10EC: return "realtek";
        case 0x8087: return "intel";
        case 0x0E11: return "hewlett-packard";
        case 0x0001: return "ibm";
        default: return "unknown";
    }
}

static void udev_scan_pci_devices(void)
{
    journal_log("udev", "Scanning PCI devices", LOG_INFO);
    
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint16_t slot = 0; slot < 32; slot++) {
            for (uint16_t func = 0; func < 8; func++) {
                uint32_t addr = (bus << 16) | (slot << 11) | (func << 8);
                uint32_t vendor_device = pci_read_config_dword(bus, slot, func, 0);
                
                if (vendor_device == 0xFFFFFFFF) continue;
                
                uint16_t vendor_id = vendor_device & 0xFFFF;
                uint16_t device_id = (vendor_device >> 16) & 0xFFFF;
                
                if (vendor_id == 0xFFFF) continue;
                
                uint32_t class_rev = pci_read_config_dword(bus, slot, func, 8);
                uint8_t class = (class_rev >> 24) & 0xFF;
                uint8_t subclass = (class_rev >> 16) & 0xFF;
                
                if (udev_device_count >= UDEV_MAX_DEVICES) break;
                
                udev_device_t* dev = &udev_devices[udev_device_count];
                memset(dev, 0, sizeof(udev_device_t));
                
                sprintf(dev->devpath, "/devices/pci%02x:%02x:%02x.%x", bus, slot, func / 8, func % 8);
                sprintf(dev->sysname, "pci%02x:%02x:%02x.%x", bus, slot, func / 8, func % 8);
                sprintf(dev->devname, "pci%02x%02x%02x", bus, slot, func);
                
                strncpy(dev->subsystem, pci_class_to_subsystem(class, subclass), 63);
                strncpy(dev->vendor, pci_vendor_to_name(vendor_id), 31);
                sprintf(dev->device, "%04x", device_id);
                
                dev->pci_addr = addr;
                dev->pci_vendor_id = vendor_id;
                dev->pci_device_id = device_id;
                dev->pci_class = class;
                dev->pci_subclass = subclass;
                
                dev->major = 248 + bus;
                dev->minor = (slot << 3) | func;
                
                char msg[256];
                sprintf(msg, "Found PCI device: %s %04x:%04x", dev->subsystem, vendor_id, device_id);
                journal_log("udev", msg, LOG_INFO);
                
                udev_create_sysfs_node(dev);
                udev_apply_rules(dev);
                
                udev_device_count++;
            }
        }
    }
}

static void udev_create_default_devices(void)
{
    journal_log("udev", "Creating default devices", LOG_INFO);
    
    udev_vfs_mknod("/dev/null", FS_TYPE_CHARDEV, 1, 3);
    udev_vfs_mknod("/dev/zero", FS_TYPE_CHARDEV, 1, 5);
    udev_vfs_mknod("/dev/tty", FS_TYPE_CHARDEV, 5, 0);
    udev_vfs_mknod("/dev/console", FS_TYPE_CHARDEV, 5, 1);
    udev_vfs_mknod("/dev/random", FS_TYPE_CHARDEV, 1, 8);
    udev_vfs_mknod("/dev/urandom", FS_TYPE_CHARDEV, 1, 9);
    
    udev_vfs_mknod("/dev/fb0", FS_TYPE_CHARDEV, 29, 0);
    udev_vfs_mknod("/dev/tty0", FS_TYPE_CHARDEV, 4, 0);
    udev_vfs_mknod("/dev/tty1", FS_TYPE_CHARDEV, 4, 1);
    udev_vfs_mknod("/dev/tty2", FS_TYPE_CHARDEV, 4, 2);
    udev_vfs_mknod("/dev/tty3", FS_TYPE_CHARDEV, 4, 3);
    
    udev_vfs_mknod("/dev/sda", FS_TYPE_BLOCKDEV, 8, 0);
    udev_vfs_mknod("/dev/sda1", FS_TYPE_BLOCKDEV, 8, 1);
    udev_vfs_mknod("/dev/sda2", FS_TYPE_BLOCKDEV, 8, 2);
    udev_vfs_mknod("/dev/sdb", FS_TYPE_BLOCKDEV, 8, 16);
    
    udev_vfs_mknod("/dev/hda", FS_TYPE_BLOCKDEV, 3, 0);
    udev_vfs_mknod("/dev/hda1", FS_TYPE_BLOCKDEV, 3, 1);
    
    udev_vfs_symlink("/dev/tty0", "/dev/tty");
    
    journal_log("udev", "Default devices created", LOG_INFO);
}

int udev_load_rules(const char* path)
{
    udev_rule_count = 0;
    
    char content[8192];
    int ret = fs_read_file_content(path, content, sizeof(content));
    if (ret > 0) {
        char* line = content;
        char* next;
        
        while ((next = strchr(line, '\n')) != NULL) {
            *next = '\0';
            trim(line);
            
            if (*line == '\0' || *line == '#') {
                line = next + 1;
                continue;
            }
            
            parse_udev_rule(line);
            
            line = next + 1;
        }
    } else {
        journal_log("udev", "No rules file found, using defaults", LOG_NOTICE);
        
        udev_rule_t* rule = &udev_rules[udev_rule_count++];
        memset(rule, 0, sizeof(udev_rule_t));
        strcpy(rule->subsystem, "block");
        strcpy(rule->name, "sd%n");
        
        rule = &udev_rules[udev_rule_count++];
        memset(rule, 0, sizeof(udev_rule_t));
        strcpy(rule->subsystem, "char");
        strcpy(rule->name, "tty%n");
        
        rule = &udev_rules[udev_rule_count++];
        memset(rule, 0, sizeof(udev_rule_t));
        strcpy(rule->subsystem, "drm");
        strcpy(rule->name, "card%n");
        strcpy(rule->symlink, "card");
        
        rule = &udev_rules[udev_rule_count++];
        memset(rule, 0, sizeof(udev_rule_t));
        strcpy(rule->subsystem, "net");
        strcpy(rule->name, "eth%n");
        
        rule = &udev_rules[udev_rule_count++];
        memset(rule, 0, sizeof(udev_rule_t));
        strcpy(rule->subsystem, "usb");
        strcpy(rule->name, "usb%n");
    }
    
    char msg[256];
    sprintf(msg, "Loaded %d udev rules", udev_rule_count);
    journal_log("udev", msg, LOG_INFO);
    
    return 0;
}

int udev_scan_devices(void)
{
    journal_log("udev", "Scanning devices", LOG_INFO);
    
    spinlock_lock(&udev_lock);
    udev_device_count = 0;
    spinlock_unlock(&udev_lock);
    
    vfs_mkdir("/dev", 0755);
    vfs_mkdir("/sys", 0755);
    
    udev_create_default_devices();
    
    udev_scan_pci_devices();
    
    char msg[256];
    sprintf(msg, "Found %d devices", udev_device_count);
    journal_log("udev", msg, LOG_INFO);
    
    return 0;
}

int udev_add_device(const char* subsystem, const char* sysname, 
                    const char* vendor, const char* device,
                    int major, int minor, uint64_t pci_addr)
{
    spinlock_lock(&udev_lock);
    
    if (udev_device_count >= UDEV_MAX_DEVICES) {
        spinlock_unlock(&udev_lock);
        return -1;
    }
    
    udev_device_t* dev = &udev_devices[udev_device_count++];
    memset(dev, 0, sizeof(udev_device_t));
    strncpy(dev->subsystem, subsystem, 63);
    strncpy(dev->sysname, sysname, 63);
    strncpy(dev->devname, sysname, 63);
    strncpy(dev->vendor, vendor, 31);
    strncpy(dev->device, device, 31);
    dev->major = major;
    dev->minor = minor;
    dev->pci_addr = pci_addr;
    
    spinlock_unlock(&udev_lock);
    
    udev_apply_rules(dev);
    
    char msg[256];
    sprintf(msg, "Device added: %s/%s", subsystem, sysname);
    journal_log("udev", msg, LOG_INFO);
    
    return 0;
}

int udev_remove_device(const char* sysname)
{
    spinlock_lock(&udev_lock);
    
    for (int i = 0; i < udev_device_count; i++) {
        if (strcmp(udev_devices[i].sysname, sysname) == 0) {
            char devpath[SYSTEMD_MAX_PATH];
            sprintf(devpath, "/dev/%s", udev_devices[i].devname);
            vfs_unlink(devpath);
            
            char sysdir[SYSTEMD_MAX_PATH];
            sprintf(sysdir, "/sys/%s/%s", udev_devices[i].subsystem, sysname);
            vfs_rmdir(sysdir);
            
            for (int j = i; j < udev_device_count - 1; j++) {
                udev_devices[j] = udev_devices[j + 1];
            }
            udev_device_count--;
            
            char msg[256];
            sprintf(msg, "Device removed: %s", sysname);
            journal_log("udev", msg, LOG_INFO);
            
            spinlock_unlock(&udev_lock);
            return 0;
        }
    }
    
    spinlock_unlock(&udev_lock);
    return -1;
}