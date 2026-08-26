#include <arch/win32.h>
#include <arch/pci.h>
#include <arch/slab.h>
#include <string.h>
int sprintf(char* str, const char* format, ...);
extern void* memory_alloc(uint64_t size);
extern void memory_free(void* p);

typedef struct {
    uint32_t signature;
    uint32_t instance_id;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint8_t  class_code[3];
    uint8_t  revision;
    uint8_t  header_type;
    uint32_t bar[6];
    uint32_t subsystem_irq;
    char device_id_string[256];
    char instance_path[256];
    char friendly_name[128];
    char hardware_ids[1024];
    char compatible_ids[512];
    char service_name[64];
    uint32_t driver_version;
    uint32_t driver_date;
    BOOL started;
    BOOL disabled;
    BOOL removed;
    void* dev_node;
    void* parent;
    void* first_child;
    void* next_sibling;
    uint32_t problem_code;
    uint64_t capabilities;
} pnp_device_node_t;

#define PNP_MAX_NODES 256
static pnp_device_node_t g_pnp_nodes[PNP_MAX_NODES];
static uint32_t g_pnp_count = 0;
static BOOL g_pnp_init = FALSE;

#define CR_SUCCESS               0x00000000
#define CR_DEFAULT               0x00000001
#define CR_OUT_OF_MEMORY         0x00000002
#define CR_INVALID_POINTER       0x00000003
#define CR_INVALID_FLAG          0x00000004
#define CR_INVALID_DEVNODE       0x00000005
#define CR_INVALID_DEVINST       CR_INVALID_DEVNODE
#define CR_INVALID_RES_DES       0x00000006
#define CR_INVALID_CONFLICT      0x00000007
#define CR_INVALID_ARBITRATOR    0x00000008
#define CR_INVALID_NODELIST      0x00000009
#define CR_DEVNODE_HAS_REQS      0x0000000A
#define CR_DEVINST_HAS_REQS      CR_DEVNODE_HAS_REQS
#define CR_INVALID_RESOURCEID    0x0000000B
#define CR_NO_SUCH_DEVNODE       0x0000000B
#define CR_NO_SUCH_DEVINST       CR_NO_SUCH_DEVNODE
#define CR_NO_CAPABILITIES       0x0000000C
#define CR_NOT_CURRENT           0x0000000D
#define CR_NO_SUCH_VALUE         0x0000000E
#define CR_INVALID_DATA          0x0000000F
#define CR_INVALID_LIST          0x00000010
#define CR_BUSY                  0x00000011
#define CR_SMALL_BUFFER          0x00000012
#define CM_LOCATE_DEVNODE_NORMAL     0x00000000
#define CM_LOCATE_DEVNODE_PHANTOM    0x00000001
#define CM_LOCATE_DEVNODE_CANCELREMOVE 0x00000002
#define CM_LOCATE_DEVNODE_NOVALIDATION 0x00000004
#define CM_REENUMERATE_NORMAL          0x00000000
#define CM_REENUMERATE_SYNCHRONOUS     0x00000001
#define CM_REENUMERATE_RETRY_INSTALLATION 0x00000002
#define CM_GETIDLIST_FILTER_NONE       0x00000000
#define CM_GET_DEVICE_INTERFACE_LIST_PRESENT 0x00000000

#define DN_HAS_PROBLEM            0x00000001
#define DN_FILTER                 0x00000020

#define SPDRP_HARDWAREID                     1
#define SPDRP_COMPATIBLEIDS                  2
#define SPDRP_SERVICE                        4
#define SPDRP_CLASS                          5
#define SPDRP_CLASSGUID                      6
#define SPDRP_DRIVER                         7
#define SPDRP_MFG                            8
#define SPDRP_FRIENDLYNAME                   9
#define SPDRP_LOCATION_INFORMATION          11
#define SPDRP_PHYSICAL_DEVICE_OBJECT_NAME   12
#define SPDRP_INSTALL_STATE                 22

typedef struct _SP_DEVINFO_DATA {
    uint32_t cbSize;
    GUID ClassGuid;
    uint32_t DevInst;
    ULONG_PTR Reserved;
} SP_DEVINFO_DATA, *PSP_DEVINFO_DATA;

static const char* get_base_class_name(uint8_t base_class) {
    switch (base_class) {
        case 0x00: return "Non-VGA Unclassified";
        case 0x01: return "Mass Storage Controller";
        case 0x02: return "Network Controller";
        case 0x03: return "VGA Compatible Controller";
        case 0x04: return "Multimedia Controller";
        case 0x05: return "Memory Controller";
        case 0x06: return "Bridge Device";
        case 0x07: return "Simple Communication Controller";
        case 0x08: return "Base System Peripheral";
        case 0x09: return "Input Device Controller";
        case 0x0A: return "Docking Station";
        case 0x0B: return "Processor";
        case 0x0C: return "Serial Bus Controller";
        case 0x0D: return "Wireless Controller";
        case 0x0E: return "Intelligent I/O Controller";
        case 0x0F: return "Satellite Communications Controller";
        case 0x10: return "Encryption/Decryption Controller";
        case 0x11: return "Data Acquisition and Signal Processing Controller";
        default:   return "Unknown PCI Device";
    }
}

static void append_subclass_name(char* buf, size_t buf_size, uint8_t base_class, uint8_t subclass) {
    const char* sub = NULL;
    size_t blen;
    switch (base_class) {
        case 0x01:
            switch (subclass) {
                case 0x00: sub = "SCSI Bus Controller"; break;
                case 0x01: sub = "IDE Controller"; break;
                case 0x02: sub = "Floppy Disk Controller"; break;
                case 0x03: sub = "IPI Bus Controller"; break;
                case 0x04: sub = "RAID Controller"; break;
                case 0x05: sub = "ATA Controller"; break;
                case 0x06: sub = "SATA Controller"; break;
                default: break;
            }
            break;
        case 0x02:
            switch (subclass) {
                case 0x00: sub = "Ethernet Controller"; break;
                case 0x01: sub = "Token Ring Controller"; break;
                case 0x02: sub = "FDDI Controller"; break;
                case 0x03: sub = "ATM Controller"; break;
                case 0x04: sub = "ISDN Controller"; break;
                default: break;
            }
            break;
        case 0x03:
            switch (subclass) {
                case 0x00: sub = "VGA Controller"; break;
                case 0x01: sub = "XGA Controller"; break;
                case 0x02: sub = "3D Controller"; break;
                default: break;
            }
            break;
        case 0x06:
            switch (subclass) {
                case 0x00: sub = "Host Bridge"; break;
                case 0x01: sub = "ISA Bridge"; break;
                case 0x02: sub = "EISA Bridge"; break;
                case 0x03: sub = "MCA Bridge"; break;
                case 0x04: sub = "PCI-to-PCI Bridge"; break;
                case 0x05: sub = "PCMCIA Bridge"; break;
                case 0x06: sub = "NuBus Bridge"; break;
                case 0x07: sub = "CardBus Bridge"; break;
                default: break;
            }
            break;
        case 0x07:
            switch (subclass) {
                case 0x00: sub = "Serial Controller"; break;
                case 0x01: sub = "Parallel Port"; break;
                case 0x02: sub = "Multiport Serial Controller"; break;
                case 0x03: sub = "Modem"; break;
                default: break;
            }
            break;
        case 0x08:
            switch (subclass) {
                case 0x00: sub = "PIC"; break;
                case 0x01: sub = "DMA Controller"; break;
                case 0x02: sub = "Timer"; break;
                case 0x03: sub = "RTC"; break;
                case 0x04: sub = "PCI Hot-Plug Controller"; break;
                default: break;
            }
            break;
        case 0x0C:
            switch (subclass) {
                case 0x00: sub = "FireWire Controller"; break;
                case 0x01: sub = "ACCESS Bus"; break;
                case 0x02: sub = "SSA Controller"; break;
                case 0x03: sub = "USB Controller"; break;
                case 0x04: sub = "Fibre Channel"; break;
                case 0x05: sub = "SMBus"; break;
                default: break;
            }
            break;
        default:
            break;
    }
    if (sub != NULL && buf != NULL) {
        blen = strlen(buf);
        if (blen + 3u + strlen(sub) + 1u < buf_size) {
            strcat(buf, " (");
            strcat(buf, sub);
            strcat(buf, ")");
        }
    }
}

static pnp_device_node_t* add_pnp_root(void) {
    pnp_device_node_t* node;
    if (g_pnp_count >= PNP_MAX_NODES) return NULL;
    node = &g_pnp_nodes[g_pnp_count];
    memset(node, 0, sizeof(pnp_device_node_t));
    node->signature = 'PnPD';
    node->instance_id = g_pnp_count + 1u;
    node->started = TRUE;
    node->problem_code = 0;
    g_pnp_count++;
    return node;
}

static pnp_device_node_t* find_node_by_id(uint32_t id) {
    uint32_t i;
    for (i = 0; i < g_pnp_count; i++) {
        if (g_pnp_nodes[i].instance_id == id) return &g_pnp_nodes[i];
    }
    return NULL;
}

int pnp_init(void) {
    pnp_device_node_t* acpi_root;
    pnp_device_node_t* pci_root;
    int b;
    int d;
    int f;
    pnp_device_node_t* last_child = NULL;
    if (g_pnp_init) return CR_SUCCESS;

    memset(g_pnp_nodes, 0, sizeof(g_pnp_nodes));
    g_pnp_count = 0;

    acpi_root = add_pnp_root();
    if (acpi_root != NULL) {
        strcpy(acpi_root->device_id_string, "ACPI\\PNP0A03\\0");
        strcpy(acpi_root->instance_path, "ACPI_HAL\\PNP0A03");
        strcpy(acpi_root->friendly_name, "ACPI x64-based PC");
        strcpy(acpi_root->hardware_ids, "ACPI\\PNP0A03\0PNP0A03\0\0");
        strcpy(acpi_root->compatible_ids, "\0");
        strcpy(acpi_root->service_name, "ACPI");
        acpi_root->parent = NULL;
        acpi_root->first_child = NULL;
        acpi_root->next_sibling = NULL;
    }

    pci_root = add_pnp_root();
    if (pci_root != NULL) {
        char tmp[128];
        strcpy(pci_root->device_id_string, "PCI\\VEN_8086&DEV_1237&REV_02");
        sprintf(tmp, "PCIROOT(0)#PCI(0000)");
        strcpy(pci_root->instance_path, tmp);
        strcpy(pci_root->friendly_name, "PCI Root Bridge");
        sprintf(tmp, "PCI\\VEN_8086&DEV_1237&SUBSYS_00000000&REV_02\0PCI\\VEN_8086&DEV_1237&REV_02\0PCI\\VEN_8086&DEV_1237\0PCI\\VEN_8086&CC_060000\0PCI\\VEN_8086&CC_0600\0\0");
        memcpy(pci_root->hardware_ids, tmp, strlen(tmp) + 1u);
        strcpy(pci_root->compatible_ids, "PCI\\CC_060000\0PCI\\CC_0600\0\0");
        strcpy(pci_root->service_name, "pci");
        pci_root->class_code[0] = 0x06;
        pci_root->class_code[1] = 0x00;
        pci_root->class_code[2] = 0x00;
        pci_root->parent = acpi_root;
        pci_root->first_child = NULL;
        pci_root->next_sibling = NULL;
        if (acpi_root != NULL) {
            acpi_root->first_child = pci_root;
        }
    }

    last_child = NULL;
    for (b = 0; b < 256; b++) {
        for (d = 0; d < 32; d++) {
            for (f = 0; f < 8; f++) {
                uint32_t id_reg;
                uint32_t class_reg;
                uint32_t sub_reg;
                uint16_t vid;
                uint16_t did;
                uint8_t base_cls;
                uint8_t sub_cls;
                uint8_t prog_if;
                uint8_t rev;
                uint16_t sub_vid;
                uint16_t sub_did;
                pnp_device_node_t* dev_node;
                char tmp[512];
                size_t nlen;
                int bar;
                uint8_t hdr_type;

                id_reg = pci_read_config((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x00);
                if (id_reg == 0xFFFFFFFFu) continue;
                vid = (uint16_t)(id_reg & 0xFFFFu);
                did = (uint16_t)((id_reg >> 16) & 0xFFFFu);
                if (vid == 0 && did == 0) continue;

                class_reg = pci_read_config((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x08);
                rev = (uint8_t)(class_reg & 0xFFu);
                prog_if = (uint8_t)((class_reg >> 8) & 0xFFu);
                sub_cls = (uint8_t)((class_reg >> 16) & 0xFFu);
                base_cls = (uint8_t)((class_reg >> 24) & 0xFFu);

                sub_reg = pci_read_config((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x2C);
                sub_vid = (uint16_t)(sub_reg & 0xFFFFu);
                sub_did = (uint16_t)((sub_reg >> 16) & 0xFFFFu);

                hdr_type = (uint8_t)(pci_read_config((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x0C) >> 16);
                hdr_type &= 0x7Fu;

                if (g_pnp_count >= PNP_MAX_NODES) continue;
                dev_node = &g_pnp_nodes[g_pnp_count];
                memset(dev_node, 0, sizeof(pnp_device_node_t));
                dev_node->signature = 'PnPD';
                dev_node->instance_id = g_pnp_count + 1u;
                dev_node->vendor_id = vid;
                dev_node->device_id = did;
                dev_node->subsystem_vendor_id = sub_vid;
                dev_node->subsystem_id = sub_did;
                dev_node->class_code[0] = base_cls;
                dev_node->class_code[1] = sub_cls;
                dev_node->class_code[2] = prog_if;
                dev_node->revision = rev;
                dev_node->header_type = hdr_type;

                for (bar = 0; bar < 6; bar++) {
                    dev_node->bar[bar] = pci_read_config((uint8_t)b, (uint8_t)d, (uint8_t)f, (uint8_t)(0x10u + (uint32_t)bar * 4u));
                }
                dev_node->subsystem_irq = (uint32_t)pci_read_config_byte((uint8_t)b, (uint8_t)d, (uint8_t)f, 0x3Cu);

                sprintf(tmp, "PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X",
                        (unsigned)vid, (unsigned)did, (unsigned)sub_vid, (unsigned)sub_did, (unsigned)rev);
                nlen = strlen(tmp);
                if (nlen >= sizeof(dev_node->device_id_string)) nlen = sizeof(dev_node->device_id_string) - 1u;
                memcpy(dev_node->device_id_string, tmp, nlen);
                dev_node->device_id_string[nlen] = 0;

                sprintf(tmp, "PCIROOT(%d)#PCI(%02X%02X)#PCI(%02X%02X)",
                        0, b / 256, (b % 256), d, f);
                nlen = strlen(tmp);
                if (nlen >= sizeof(dev_node->instance_path)) nlen = sizeof(dev_node->instance_path) - 1u;
                memcpy(dev_node->instance_path, tmp, nlen);
                dev_node->instance_path[nlen] = 0;

                strcpy(dev_node->friendly_name, get_base_class_name(base_cls));
                append_subclass_name(dev_node->friendly_name, sizeof(dev_node->friendly_name), base_cls, sub_cls);

                {
                    size_t pos = 0;
                    char id[256];
                    sprintf(id, "PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X&REV_%02X", (unsigned)vid, (unsigned)did, (unsigned)sub_vid, (unsigned)sub_did, (unsigned)rev);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->hardware_ids)) {
                        memcpy(dev_node->hardware_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    sprintf(id, "PCI\\VEN_%04X&DEV_%04X&SUBSYS_%04X%04X", (unsigned)vid, (unsigned)did, (unsigned)sub_vid, (unsigned)sub_did);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->hardware_ids)) {
                        memcpy(dev_node->hardware_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    sprintf(id, "PCI\\VEN_%04X&DEV_%04X&REV_%02X", (unsigned)vid, (unsigned)did, (unsigned)rev);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->hardware_ids)) {
                        memcpy(dev_node->hardware_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    sprintf(id, "PCI\\VEN_%04X&DEV_%04X", (unsigned)vid, (unsigned)did);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->hardware_ids)) {
                        memcpy(dev_node->hardware_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    sprintf(id, "PCI\\VEN_%04X&CC_%02X%02X%02X", (unsigned)vid, (unsigned)base_cls, (unsigned)sub_cls, (unsigned)prog_if);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->hardware_ids)) {
                        memcpy(dev_node->hardware_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    sprintf(id, "PCI\\VEN_%04X&CC_%02X%02X", (unsigned)vid, (unsigned)base_cls, (unsigned)sub_cls);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->hardware_ids)) {
                        memcpy(dev_node->hardware_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    if (pos + 1u < sizeof(dev_node->hardware_ids)) {
                        dev_node->hardware_ids[pos] = 0;
                    }

                    pos = 0;
                    sprintf(id, "PCI\\CC_%02X%02X%02X", (unsigned)base_cls, (unsigned)sub_cls, (unsigned)prog_if);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->compatible_ids)) {
                        memcpy(dev_node->compatible_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    sprintf(id, "PCI\\CC_%02X%02X", (unsigned)base_cls, (unsigned)sub_cls);
                    nlen = strlen(id) + 1u;
                    if (pos + nlen < sizeof(dev_node->compatible_ids)) {
                        memcpy(dev_node->compatible_ids + pos, id, nlen);
                        pos += nlen;
                    }
                    if (pos + 1u < sizeof(dev_node->compatible_ids)) {
                        dev_node->compatible_ids[pos] = 0;
                    }
                }

                switch (base_cls) {
                    case 0x01: strcpy(dev_node->service_name, "disk"); break;
                    case 0x02: strcpy(dev_node->service_name, "net"); break;
                    case 0x03: strcpy(dev_node->service_name, "vga"); break;
                    case 0x04: strcpy(dev_node->service_name, "hdaudio"); break;
                    case 0x06: strcpy(dev_node->service_name, "pci"); break;
                    case 0x0C: strcpy(dev_node->service_name, "usb"); break;
                    default:   break;
                }

                dev_node->driver_version = 0x000A0000u;
                dev_node->driver_date = 0x20240101u;
                dev_node->started = TRUE;
                dev_node->disabled = FALSE;
                dev_node->removed = FALSE;
                dev_node->problem_code = 0;
                dev_node->capabilities = 0;

                dev_node->parent = pci_root;
                dev_node->first_child = NULL;
                dev_node->next_sibling = NULL;
                if (last_child == NULL) {
                    if (pci_root != NULL) pci_root->first_child = dev_node;
                } else {
                    last_child->next_sibling = dev_node;
                }
                last_child = dev_node;

                g_pnp_count++;
            }
        }
    }

    g_pnp_init = TRUE;
    return CR_SUCCESS;
}

uint32_t CM_Get_Device_ID_Size(uint32_t* pulLen, uint32_t dnDevInst, uint32_t ulFlags) {
    pnp_device_node_t* node;
    (void)ulFlags;
    if (pulLen == NULL) return CR_INVALID_POINTER;
    node = find_node_by_id(dnDevInst);
    if (node == NULL) return CR_INVALID_DEVNODE;
    *pulLen = (uint32_t)strlen(node->device_id_string) + 1u;
    return CR_SUCCESS;
}

uint32_t CM_Get_Device_IDA(uint32_t dnDevInst, char* Buffer, uint32_t BufferLen, uint32_t ulFlags) {
    pnp_device_node_t* node;
    size_t id_len;
    (void)ulFlags;
    node = find_node_by_id(dnDevInst);
    if (node == NULL) return CR_INVALID_DEVNODE;
    id_len = strlen(node->device_id_string) + 1u;
    if (Buffer == NULL || BufferLen < (uint32_t)id_len) return CR_SMALL_BUFFER;
    strncpy(Buffer, node->device_id_string, BufferLen - 1u);
    Buffer[BufferLen - 1u] = 0;
    return CR_SUCCESS;
}

uint32_t CM_Get_Parent(uint32_t* pdnDevInst, uint32_t dnDevInst, uint32_t ulFlags) {
    pnp_device_node_t* node;
    pnp_device_node_t* parent;
    (void)ulFlags;
    if (pdnDevInst == NULL) return CR_INVALID_POINTER;
    node = find_node_by_id(dnDevInst);
    if (node == NULL) return CR_INVALID_DEVNODE;
    parent = (pnp_device_node_t*)node->parent;
    if (parent == NULL) {
        *pdnDevInst = 0;
    } else {
        *pdnDevInst = parent->instance_id;
    }
    return CR_SUCCESS;
}

uint32_t CM_Get_Child(uint32_t* pdnDevInst, uint32_t dnDevInst, uint32_t ulFlags) {
    pnp_device_node_t* node;
    pnp_device_node_t* child;
    (void)ulFlags;
    if (pdnDevInst == NULL) return CR_INVALID_POINTER;
    node = find_node_by_id(dnDevInst);
    if (node == NULL) return CR_INVALID_DEVNODE;
    child = (pnp_device_node_t*)node->first_child;
    if (child == NULL) return CR_NO_SUCH_DEVNODE;
    *pdnDevInst = child->instance_id;
    return CR_SUCCESS;
}

uint32_t CM_Get_Sibling(uint32_t* pdnDevInst, uint32_t dnDevInst, uint32_t ulFlags) {
    pnp_device_node_t* node;
    pnp_device_node_t* sib;
    (void)ulFlags;
    if (pdnDevInst == NULL) return CR_INVALID_POINTER;
    node = find_node_by_id(dnDevInst);
    if (node == NULL) return CR_INVALID_DEVNODE;
    sib = (pnp_device_node_t*)node->next_sibling;
    if (sib == NULL) return CR_NO_SUCH_DEVNODE;
    *pdnDevInst = sib->instance_id;
    return CR_SUCCESS;
}

uint32_t CM_Get_Device_ID_ListA(const char* pszFilter, char* Buffer, uint32_t BufferLen, uint32_t ulFlags) {
    uint32_t i;
    size_t pos = 0;
    int overflow = 0;
    (void)ulFlags;
    if (!g_pnp_init) pnp_init();
    for (i = 0; i < g_pnp_count; i++) {
        const pnp_device_node_t* node = &g_pnp_nodes[i];
        size_t id_len;
        int match = 1;
        if (pszFilter != NULL && pszFilter[0] != 0) {
            match = (strstr(node->device_id_string, pszFilter) != NULL);
        }
        if (!match) continue;
        id_len = strlen(node->device_id_string) + 1u;
        if (Buffer != NULL && pos + id_len <= BufferLen) {
            memcpy(Buffer + pos, node->device_id_string, id_len);
        } else {
            overflow = 1;
        }
        pos += id_len;
    }
    if (Buffer != NULL && pos + 1u <= BufferLen) {
        Buffer[pos] = 0;
    } else {
        overflow = 1;
    }
    if (overflow) return CR_SMALL_BUFFER;
    return CR_SUCCESS;
}

uint32_t CM_Locate_DevNodeA(uint32_t* pdnDevInst, const char* pDeviceID, uint32_t ulFlags) {
    uint32_t i;
    (void)ulFlags;
    if (pdnDevInst == NULL || pDeviceID == NULL) return CR_INVALID_POINTER;
    if (!g_pnp_init) pnp_init();
    for (i = 0; i < g_pnp_count; i++) {
        if (strcmp(g_pnp_nodes[i].device_id_string, pDeviceID) == 0) {
            *pdnDevInst = g_pnp_nodes[i].instance_id;
            return CR_SUCCESS;
        }
    }
    return CR_NO_SUCH_DEVNODE;
}

uint32_t CM_Get_DevNode_Status(uint32_t* pulStatus, uint32_t* pulProblemNumber, uint32_t dnDevInst, uint32_t ulFlags) {
    pnp_device_node_t* node;
    (void)ulFlags;
    node = find_node_by_id(dnDevInst);
    if (node == NULL) return CR_INVALID_DEVNODE;
    if (pulStatus != NULL) {
        uint32_t st = 0;
        if (node->started) st |= 1u;
        if (node->disabled) st |= 2u;
        if (node->removed) st |= 4u;
        if (node->problem_code != 0) st |= DN_HAS_PROBLEM;
        *pulStatus = st;
    }
    if (pulProblemNumber != NULL) {
        *pulProblemNumber = node->problem_code;
    }
    return CR_SUCCESS;
}

uint32_t CM_Enumerate_EnumeratorsA(uint32_t ulEnumIndex, char* Buffer, uint32_t* pulBufferLen, uint32_t ulFlags) {
    const char* name;
    size_t nlen;
    (void)ulFlags;
    switch (ulEnumIndex) {
        case 0: name = "ACPI"; break;
        case 1: name = "PCI"; break;
        default: return CR_NO_SUCH_VALUE;
    }
    nlen = strlen(name) + 1u;
    if (pulBufferLen != NULL && *pulBufferLen < (uint32_t)nlen) {
        if (Buffer != NULL && *pulBufferLen >= 5u) {
            strncpy(Buffer, name, *pulBufferLen - 1u);
            Buffer[*pulBufferLen - 1u] = 0;
        }
    } else if (Buffer != NULL) {
        strcpy(Buffer, name);
    }
    if (pulBufferLen != NULL) *pulBufferLen = (uint32_t)nlen;
    return CR_SUCCESS;
}

static int is_storage_class(uint8_t base_class) { return base_class == 0x01; }
static int is_net_class(uint8_t base_class) { return base_class == 0x02; }
static int is_display_class(uint8_t base_class) { return base_class == 0x03; }

uint32_t CM_Get_Device_Interface_ListA(void* InterfaceClassGuid, const char* pDeviceID,
                                         char* Buffer, uint32_t BufferLen, uint32_t ulFlags) {
    uint32_t i;
    size_t pos = 0;
    int overflow = 0;
    GUID* guid = (GUID*)InterfaceClassGuid;
    (void)ulFlags;
    if (!g_pnp_init) pnp_init();
    for (i = 0; i < g_pnp_count; i++) {
        const pnp_device_node_t* node = &g_pnp_nodes[i];
        char if_path[512];
        size_t if_len;
        int match = 1;
        if (pDeviceID != NULL && pDeviceID[0] != 0) {
            if (strcmp(node->device_id_string, pDeviceID) != 0) continue;
        }
        if (guid != NULL) {
            uint32_t d1 = guid->Data1;
            if (d1 == 0x53F56307u) {
                match = is_storage_class(node->class_code[0]);
            } else if (d1 == 0xAD498944u) {
                match = is_net_class(node->class_code[0]);
            } else if (d1 == 0x1CA05180u) {
                match = is_display_class(node->class_code[0]);
            } else {
                match = 0;
            }
        }
        if (!match) continue;
        sprintf(if_path, "\\\\?\\%s#{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}#000000000000",
                node->instance_path,
                guid ? (unsigned)guid->Data1 : 0u,
                guid ? (unsigned)guid->Data2 : 0u,
                guid ? (unsigned)guid->Data3 : 0u,
                guid ? guid->Data4[0] : 0u,
                guid ? guid->Data4[1] : 0u,
                guid ? guid->Data4[2] : 0u,
                guid ? guid->Data4[3] : 0u,
                guid ? guid->Data4[4] : 0u,
                guid ? guid->Data4[5] : 0u,
                guid ? guid->Data4[6] : 0u,
                guid ? guid->Data4[7] : 0u);
        if_len = strlen(if_path) + 1u;
        if (Buffer != NULL && pos + if_len <= BufferLen) {
            memcpy(Buffer + pos, if_path, if_len);
        } else {
            overflow = 1;
        }
        pos += if_len;
    }
    if (Buffer != NULL && pos + 1u <= BufferLen) {
        Buffer[pos] = 0;
    } else {
        overflow = 1;
    }
    if (overflow) return CR_SMALL_BUFFER;
    return CR_SUCCESS;
}

uint32_t CM_Request_Device_EjectA(uint32_t dnDevInst, void* pVetoType, char* pszVetoName,
                                   uint32_t* pnNameLength, uint32_t ulFlags) {
    pnp_device_node_t* node;
    (void)pVetoType;
    (void)pszVetoName;
    (void)ulFlags;
    node = find_node_by_id(dnDevInst);
    if (node == NULL) return CR_INVALID_DEVNODE;
    node->removed = TRUE;
    node->started = FALSE;
    if (pnNameLength != NULL) *pnNameLength = 0;
    return CR_SUCCESS;
}

uint32_t CM_Reenumerate_DevNode(uint32_t dnDevInst, uint32_t ulFlags) {
    (void)dnDevInst;
    (void)ulFlags;
    if (!g_pnp_init) pnp_init();
    return CR_SUCCESS;
}

typedef struct {
    uint32_t signature;
    uint32_t flags;
    void* enumerator_filter;
    GUID class_guid;
    uint32_t count;
    uint32_t* ids;
    uint32_t ids_count;
} hdevinfo_t;

static uint32_t g_next_hdevinfo = 0x2000;
#define HDEVINFO_MAX 64
static hdevinfo_t g_hdevinfo_set[HDEVINFO_MAX];

static void* find_hdevinfo(void* h) {
    uint32_t i;
    uint32_t hval = (uint32_t)((uint64_t)h);
    for (i = 0; i < HDEVINFO_MAX; i++) {
        if (g_hdevinfo_set[i].signature == 0x44494658u &&
            (g_hdevinfo_set[i].signature ^ hval) == 0) {
            return &g_hdevinfo_set[i];
        }
    }
    return NULL;
}

static hdevinfo_t* alloc_hdevinfo(void) {
    uint32_t i;
    for (i = 0; i < HDEVINFO_MAX; i++) {
        if (g_hdevinfo_set[i].signature != 0x44494658u) {
            memset(&g_hdevinfo_set[i], 0, sizeof(hdevinfo_t));
            g_next_hdevinfo++;
            g_hdevinfo_set[i].signature = 0x44494658u ^ g_next_hdevinfo;
            g_hdevinfo_set[i].count = g_next_hdevinfo;
            return &g_hdevinfo_set[i];
        }
    }
    return NULL;
}

void* SetupDiGetClassDevsA(void* ClassGuid, const char* Enumerator, HANDLE hwndParent, uint32_t Flags) {
    hdevinfo_t* set;
    uint32_t i;
    (void)hwndParent;
    (void)Flags;
    if (!g_pnp_init) pnp_init();
    set = alloc_hdevinfo();
    if (set == NULL) return NULL;
    if (ClassGuid != NULL) {
        memcpy(&set->class_guid, ClassGuid, sizeof(GUID));
    }
    set->ids_count = 0;
    set->ids = (uint32_t*)memory_alloc(sizeof(uint32_t) * (uint64_t)g_pnp_count);
    if (set->ids == NULL) {
        return (void*)((uint64_t)(0x44494658u ^ set->count));
    }
    for (i = 0; i < g_pnp_count; i++) {
        int ok = 1;
        if (Enumerator != NULL && Enumerator[0] != 0) {
            if (strcmp(Enumerator, "PCI") == 0) {
                if (strncmp(g_pnp_nodes[i].device_id_string, "PCI\\", 4) != 0) ok = 0;
            } else if (strcmp(Enumerator, "ACPI") == 0) {
                if (strncmp(g_pnp_nodes[i].device_id_string, "ACPI\\", 5) != 0) ok = 0;
            }
        }
        if (ok) {
            set->ids[set->ids_count] = g_pnp_nodes[i].instance_id;
            set->ids_count++;
        }
    }
    return (void*)((uint64_t)(0x44494658u ^ set->count));
}

BOOL SetupDiEnumDeviceInfo(void* DeviceInfoSet, uint32_t MemberIndex, void* DeviceInfoData) {
    hdevinfo_t* set = NULL;
    uint32_t i;
    uint32_t tag;
    SP_DEVINFO_DATA* data = (SP_DEVINFO_DATA*)DeviceInfoData;
    uint32_t hval;
    if (DeviceInfoData == NULL) return FALSE;
    hval = (uint32_t)((uint64_t)DeviceInfoSet);
    for (i = 0; i < HDEVINFO_MAX; i++) {
        tag = 0x44494658u ^ g_hdevinfo_set[i].count;
        if (g_hdevinfo_set[i].signature != 0 && tag == hval) {
            set = &g_hdevinfo_set[i];
            break;
        }
    }
    if (set == NULL) return FALSE;
    if (MemberIndex >= set->ids_count) return FALSE;
    if (data->cbSize < sizeof(SP_DEVINFO_DATA)) return FALSE;
    data->DevInst = set->ids[MemberIndex];
    return TRUE;
}

BOOL SetupDiGetDeviceRegistryPropertyA(void* DeviceInfoSet, void* DeviceInfoData, uint32_t Property,
                                         uint32_t* PropertyRegDataType, uint8_t* PropertyBuffer,
                                         uint32_t PropertyBufferSize, uint32_t* RequiredSize) {
    SP_DEVINFO_DATA* data = (SP_DEVINFO_DATA*)DeviceInfoData;
    pnp_device_node_t* node;
    const char* str_data = NULL;
    char class_name[64];
    char class_guid_str[64];
    char inst_state[8];
    uint32_t reg_type = 1u;
    size_t data_len;
    (void)DeviceInfoSet;
    if (data == NULL) return FALSE;
    node = find_node_by_id(data->DevInst);
    if (node == NULL) return FALSE;

    switch (Property) {
        case SPDRP_HARDWAREID:
            str_data = node->hardware_ids;
            data_len = 0;
            {
                const char* p = str_data;
                while (*p != 0 || (*(p + 1) != 0 && p != str_data)) {
                    data_len += strlen(p) + 1u;
                    p += strlen(p) + 1u;
                    if (*p == 0) break;
                }
                data_len += 1u;
            }
            reg_type = 7u;
            break;
        case SPDRP_COMPATIBLEIDS:
            str_data = node->compatible_ids;
            data_len = 0;
            {
                const char* p = str_data;
                while (*p != 0 || (*(p + 1) != 0 && p != str_data)) {
                    data_len += strlen(p) + 1u;
                    p += strlen(p) + 1u;
                    if (*p == 0) break;
                }
                data_len += 1u;
            }
            reg_type = 7u;
            break;
        case SPDRP_SERVICE:
            str_data = node->service_name;
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_CLASS:
            strcpy(class_name, get_base_class_name(node->class_code[0]));
            str_data = class_name;
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_CLASSGUID:
            sprintf(class_guid_str, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                    (unsigned long)(0x4D36E967u + (uint32_t)node->class_code[0]),
                    (unsigned)(0xE325u + (uint16_t)node->class_code[1]),
                    (unsigned)(0x11CEu),
                    (uint8_t)(0xBF), (uint8_t)(0xC1),
                    (uint8_t)(0x08), (uint8_t)(0x00),
                    (uint8_t)(0x2B), (uint8_t)(0xE1),
                    (uint8_t)(0x03), (uint8_t)(0x18 + node->class_code[0]));
            str_data = class_guid_str;
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_DRIVER:
            str_data = node->service_name;
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_MFG:
            str_data = "Standard";
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_FRIENDLYNAME:
            str_data = node->friendly_name;
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_LOCATION_INFORMATION:
            str_data = node->instance_path;
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_PHYSICAL_DEVICE_OBJECT_NAME:
            str_data = "\\Device\\00000000";
            data_len = strlen(str_data) + 1u;
            reg_type = 1u;
            break;
        case SPDRP_INSTALL_STATE:
            sprintf(inst_state, "%d", node->started ? 0 : 1);
            str_data = inst_state;
            data_len = 4u;
            reg_type = 4u;
            break;
        default:
            return FALSE;
    }

    if (RequiredSize != NULL) *RequiredSize = (uint32_t)data_len;
    if (PropertyRegDataType != NULL) *PropertyRegDataType = reg_type;
    if (PropertyBuffer != NULL && PropertyBufferSize >= (uint32_t)data_len) {
        memcpy(PropertyBuffer, str_data, data_len);
        return TRUE;
    }
    if (PropertyBuffer == NULL) return TRUE;
    return FALSE;
}

BOOL SetupDiDestroyDeviceInfoList(void* DeviceInfoSet) {
    uint32_t i;
    uint32_t hval;
    uint32_t tag;
    (void)DeviceInfoSet;
    hval = (uint32_t)((uint64_t)DeviceInfoSet);
    for (i = 0; i < HDEVINFO_MAX; i++) {
        if (g_hdevinfo_set[i].signature != 0) {
            tag = 0x44494658u ^ g_hdevinfo_set[i].count;
            if (tag == hval) {
                if (g_hdevinfo_set[i].ids != NULL) {
                    memory_free(g_hdevinfo_set[i].ids);
                }
                memset(&g_hdevinfo_set[i], 0, sizeof(hdevinfo_t));
                break;
            }
        }
    }
    return TRUE;
}
