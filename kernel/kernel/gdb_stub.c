

#include "gdb_stub.h"
#include <arch/io.h>
#include <string.h>

#define COM1_BASE    0x3F8
#define COM1_THR     (COM1_BASE + 0)
#define COM1_RBR     (COM1_BASE + 0)
#define COM1_IER     (COM1_BASE + 1)
#define COM1_IIR     (COM1_BASE + 2)
#define COM1_LCR     (COM1_BASE + 3)
#define COM1_MCR     (COM1_BASE + 4)
#define COM1_LSR     (COM1_BASE + 5)

#define GDB_BP_MAX   16
static uint64_t gdb_breakpoints[GDB_BP_MAX];
static uint8_t  gdb_bp_saved[GDB_BP_MAX][1];
static int gdb_bp_count = 0;
static int gdb_active = 0;

#define GDB_BUF_SIZE  4096
static char gdb_buf[GDB_BUF_SIZE];

static void com1_init(void)
{
    outb(COM1_IER, 0x00);
    outb(COM1_LCR, 0x80);
    outb(COM1_BASE + 0, 3);
    outb(COM1_BASE + 1, 0);
    outb(COM1_LCR, 0x03);
    outb(COM1_MCR, 0x03);
    outb(COM1_IER, 0x01);
}

static int com1_can_read(void)
{
    return (inb(COM1_LSR) & 0x01) != 0;
}

static int com1_can_write(void)
{
    return (inb(COM1_LSR) & 0x20) != 0;
}

static char com1_read(void)
{
    while (!com1_can_read());
    return inb(COM1_RBR);
}

static void com1_write(char c)
{
    while (!com1_can_write());
    outb(COM1_THR, c);
}

static void gdb_put_char(char c)
{
    com1_write(c);
}

static char gdb_get_char(void)
{
    return com1_read();
}

static void gdb_put_packet(const char* buf)
{
    uint8_t checksum = 0;
    gdb_put_char('$');
    while (*buf) {
        gdb_put_char(*buf);
        checksum += (uint8_t)*buf;
        buf++;
    }
    gdb_put_char('#');
    gdb_put_char("0123456789abcdef"[checksum >> 4]);
    gdb_put_char("0123456789abcdef"[checksum & 0xF]);
}

static int gdb_get_packet(char* buf, int max)
{
    uint8_t checksum = 0;
    uint8_t recv_checksum = 0;
    char c;

    while ((c = gdb_get_char()) != '$');

    int count = 0;
    while ((c = gdb_get_char()) != '#') {
        if (count < max - 1) {
            buf[count++] = c;
            checksum += (uint8_t)c;
        }
    }
    buf[count] = '\0';

    c = gdb_get_char();
    recv_checksum = (c >= 'a') ? (c - 'a' + 10) : (c - '0') << 4;
    c = gdb_get_char();
    recv_checksum |= (c >= 'a') ? (c - 'a' + 10) : (c - '0');

    gdb_put_char(checksum == recv_checksum ? '+' : '-');
    return count;
}

static int hex2int(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void int2hex(char* buf, uint64_t val, int bytes)
{
    static const char hex[] = "0123456789abcdef";
    for (int i = bytes * 2 - 1; i >= 0; i--) {
        buf[i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[bytes * 2] = '\0';
}

static uint64_t hex_to_u64(const char* s)
{
    uint64_t val = 0;
    while (*s) {
        val <<= 4;
        val |= hex2int(*s);
        s++;
    }
    return val;
}

static void gdb_get_regs_str(char* buf, gdb_regs_t* regs)
{

    int pos = 0;
    int2hex(buf + pos, regs->rax, 8); pos += 16;
    int2hex(buf + pos, regs->rbx, 8); pos += 16;
    int2hex(buf + pos, regs->rcx, 8); pos += 16;
    int2hex(buf + pos, regs->rdx, 8); pos += 16;
    int2hex(buf + pos, regs->rsi, 8); pos += 16;
    int2hex(buf + pos, regs->rdi, 8); pos += 16;
    int2hex(buf + pos, regs->rbp, 8); pos += 16;
    int2hex(buf + pos, regs->rsp, 8); pos += 16;
    int2hex(buf + pos, regs->r8,  8); pos += 16;
    int2hex(buf + pos, regs->r9,  8); pos += 16;
    int2hex(buf + pos, regs->r10, 8); pos += 16;
    int2hex(buf + pos, regs->r11, 8); pos += 16;
    int2hex(buf + pos, regs->r12, 8); pos += 16;
    int2hex(buf + pos, regs->r13, 8); pos += 16;
    int2hex(buf + pos, regs->r14, 8); pos += 16;
    int2hex(buf + pos, regs->r15, 8); pos += 16;
    int2hex(buf + pos, regs->rip, 8); pos += 16;
    int2hex(buf + pos, regs->rflags, 8); pos += 16;
    int2hex(buf + pos, regs->cs, 8); pos += 16;
    int2hex(buf + pos, regs->ss, 8); pos += 16;
    int2hex(buf + pos, regs->ds, 8); pos += 16;
    int2hex(buf + pos, regs->es, 8); pos += 16;
    int2hex(buf + pos, regs->fs, 8); pos += 16;
    int2hex(buf + pos, regs->gs, 8); pos += 16;
    buf[pos] = '\0';
}

static void gdb_set_regs_str(const char* buf, gdb_regs_t* regs)
{
    regs->rax = hex_to_u64(buf);
    regs->rbx = hex_to_u64(buf + 16);
    regs->rcx = hex_to_u64(buf + 32);
    regs->rdx = hex_to_u64(buf + 48);
    regs->rsi = hex_to_u64(buf + 64);
    regs->rdi = hex_to_u64(buf + 80);
    regs->rbp = hex_to_u64(buf + 96);
    regs->rsp = hex_to_u64(buf + 112);
    regs->r8  = hex_to_u64(buf + 128);
    regs->r9  = hex_to_u64(buf + 144);
    regs->r10 = hex_to_u64(buf + 160);
    regs->r11 = hex_to_u64(buf + 176);
    regs->r12 = hex_to_u64(buf + 192);
    regs->r13 = hex_to_u64(buf + 208);
    regs->r14 = hex_to_u64(buf + 224);
    regs->r15 = hex_to_u64(buf + 240);
    regs->rip = hex_to_u64(buf + 256);
    regs->rflags = hex_to_u64(buf + 272);
}

int gdb_set_breakpoint(uint64_t addr)
{
    if (gdb_bp_count >= GDB_BP_MAX) return -1;

    gdb_breakpoints[gdb_bp_count] = addr;
    uint8_t* p = (uint8_t*)addr;
    gdb_bp_saved[gdb_bp_count][0] = *p;
    *p = 0xCC;

    gdb_bp_count++;
    return 0;
}

int gdb_clear_breakpoint(uint64_t addr)
{
    for (int i = 0; i < gdb_bp_count; i++) {
        if (gdb_breakpoints[i] == addr) {

            *(uint8_t*)addr = gdb_bp_saved[i][0];
            gdb_breakpoints[i] = gdb_breakpoints[gdb_bp_count - 1];
            memcpy(gdb_bp_saved[i], gdb_bp_saved[gdb_bp_count - 1], 1);
            gdb_bp_count--;
            return 0;
        }
    }
    return -1;
}

void gdb_stub_init(void)
{
    com1_init();
    gdb_bp_count = 0;
    gdb_active = 0;
}

void gdb_stub_enter(gdb_regs_t* regs)
{
    gdb_active = 1;

    for (;;) {
        char cmd = gdb_get_char();

        switch (cmd) {
            case '?': {

                gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                gdb_put_packet("S05");
                break;
            }

            case 'g': {

                gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                gdb_get_regs_str(gdb_buf, regs);
                gdb_put_packet(gdb_buf);
                break;
            }

            case 'G': {

                int len = gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                if (len > 0) {
                    gdb_set_regs_str(gdb_buf, regs);
                    gdb_put_packet("OK");
                }
                break;
            }

            case 'm': {

                int len = gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                uint64_t addr = 0, size = 0;
                char* comma = (char*)memchr(gdb_buf, ',', len);
                if (comma) {
                    *comma = '\0';
                    addr = hex_to_u64(gdb_buf);
                    size = hex_to_u64(comma + 1);
                }
                if (size > 256) size = 256;
                char reply[1024];
                int pos = 0;
                for (uint64_t i = 0; i < size; i++) {
                    int2hex(reply + pos, (uint64_t)*((uint8_t*)addr + i), 1);
                    pos += 2;
                }
                reply[pos] = '\0';
                gdb_put_packet(reply);
                break;
            }

            case 'M': {

                int len = gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                char* comma = (char*)memchr(gdb_buf, ',', len);
                if (comma) {
                    *comma = '\0';
                    uint64_t addr = hex_to_u64(gdb_buf);
                    char* colon = (char*)memchr(comma + 1, ':', len - (comma - gdb_buf) - 1);
                    if (colon) {
                        *colon = '\0';
                        uint64_t size = hex_to_u64(comma + 1);
                        const char* data = colon + 1;
                        for (uint64_t i = 0; i < size; i++) {
                            ((uint8_t*)addr)[i] = (uint8_t)hex_to_u64(data + i * 2);
                        }
                        gdb_put_packet("OK");
                    }
                }
                break;
            }

            case 'z': {

                int len = gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                char type = gdb_buf[0];
                (void)type;
                char* comma = (char*)memchr(gdb_buf + 2, ',', len);
                if (comma) {
                    uint64_t addr = hex_to_u64(gdb_buf + 2);
                    gdb_clear_breakpoint(addr);
                    gdb_put_packet("OK");
                }
                break;
            }

            case 'Z': {

                int len = gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                char type = gdb_buf[0];
                (void)type;
                char* comma = (char*)memchr(gdb_buf + 2, ',', len);
                if (comma) {
                    uint64_t addr = hex_to_u64(gdb_buf + 2);
                    gdb_set_breakpoint(addr);
                    gdb_put_packet("OK");
                }
                break;
            }

            case 'c': {

                gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                gdb_active = 0;
                return;
            }

            case 's': {

                gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                regs->rflags |= (1ULL << 8);
                gdb_active = 0;
                return;
            }

            case 'k': {

                gdb_put_packet("OK");
                while (1) {
                    __asm__ volatile ("cli; hlt");
                }
                break;
            }

            case 'q': {

                int len = gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                if (strncmp(gdb_buf, "Supported", 9) == 0) {
                    gdb_put_packet("PacketSize=4096");
                } else if (strncmp(gdb_buf, "Attached", 8) == 0) {
                    gdb_put_packet("1");
                } else {
                    gdb_put_packet("");
                }
                break;
            }

            default:
                gdb_get_packet(gdb_buf, sizeof(gdb_buf));
                gdb_put_packet("");
                break;
        }
    }
}
