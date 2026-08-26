/*
 * kapi_ebpf.c —— eBPF 子系统 kapi 层封装
 *
 * 底层实现在 kernel/arch/x86_64/bpf.c，本文件提供 kapi_ 前缀的统一接口、
 * 默认 helper 注册以及简单的程序 dump 工具。
 */

#include "kapi_ebpf.h"
#include "kapi.h"

#include <arch/bpf.h>
#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

/* 程序名注册表（可选，仅用于调试） */
#define KAPI_BPF_PROG_MAX  32
typedef struct {
    bpf_program_t* prog;
    char name[16];
} kapi_bpf_entry_t;

static kapi_bpf_entry_t g_prog_table[KAPI_BPF_PROG_MAX];
static spinlock_t g_prog_lock = SPINLOCK_INIT;
static int g_ebpf_inited = 0;

/* 底层 helper 注册函数（在 bpf.c 中实现） */
typedef u64 (*bpf_helper_native_t)(u64 r1, u64 r2, u64 r3, u64 r4, u64 r5);
extern int bpf_register_helper(int id, bpf_helper_native_t fn);

int kapi_ebpf_init(void)
{
    if (g_ebpf_inited) return KAPI_OK;

    bpf_init();
    spin_init(&g_prog_lock);
    for (int i = 0; i < KAPI_BPF_PROG_MAX; i++) {
        g_prog_table[i].prog = NULL;
        g_prog_table[i].name[0] = '\0';
    }
    g_ebpf_inited = 1;
    return KAPI_OK;
}

int kapi_ebpf_register_helper(int id, kapi_bpf_helper_fn_t fn)
{
    if (!fn) return KAPI_EINVAL;
    if (id < 0 || id >= BPF_HELPER_MAX) return KAPI_EINVAL;
    return bpf_register_helper(id, (bpf_helper_native_t)fn);
}

bpf_map_t* kapi_ebpf_map_create(u32 type, const char* name,
                                 u32 key_size, u32 value_size,
                                 u32 max_entries)
{
    (void)name;
    return bpf_map_create(type, key_size, value_size, max_entries);
}

int kapi_ebpf_map_destroy(bpf_map_t* map)
{
    if (!map) return KAPI_EINVAL;
    bpf_map_destroy(map);
    return KAPI_OK;
}

bpf_program_t* kapi_ebpf_prog_load(const char* name, int type,
                                    const bpf_insn_t* insns, u32 insn_cnt)
{
    if (!insns || insn_cnt == 0 || insn_cnt > MAX_INSNS) return NULL;

    bpf_program_t* prog = bpf_prog_alloc((bpf_insn_t*)insns, insn_cnt, type);
    if (!prog) return NULL;

    /* 注册程序名（可选） */
    if (name) {
        spin_lock(&g_prog_lock);
        for (int i = 0; i < KAPI_BPF_PROG_MAX; i++) {
            if (g_prog_table[i].prog == NULL) {
                g_prog_table[i].prog = prog;
                strncpy(g_prog_table[i].name, name, 15);
                g_prog_table[i].name[15] = '\0';
                break;
            }
        }
        spin_unlock(&g_prog_lock);
    }
    return prog;
}

int kapi_ebpf_prog_unload(bpf_program_t* prog)
{
    if (!prog) return KAPI_EINVAL;
    spin_lock(&g_prog_lock);
    for (int i = 0; i < KAPI_BPF_PROG_MAX; i++) {
        if (g_prog_table[i].prog == prog) {
            g_prog_table[i].prog = NULL;
            g_prog_table[i].name[0] = '\0';
            break;
        }
    }
    spin_unlock(&g_prog_lock);
    bpf_prog_free(prog);
    return KAPI_OK;
}

int kapi_ebpf_prog_verify(const bpf_insn_t* insns, u32 insn_cnt)
{
    if (!insns || insn_cnt == 0 || insn_cnt > MAX_INSNS) return KAPI_EINVAL;
    bpf_program_t tmp;
    tmp.insns = (bpf_insn_t*)insns;
    tmp.num_insns = insn_cnt;
    return bpf_verifier(&tmp);
}

u64 kapi_ebpf_prog_run(bpf_program_t* prog, void* ctx)
{
    if (!prog) return (u64)(-1);
    return bpf_prog_run(prog, ctx);
}

/* 串口输出辅助 */
static void ebpf_serial_putc(char c)
{
    __asm__ volatile ("outb %0, %1" : : "a"(c), "d"((unsigned short)0x3F8));
}

static void ebpf_serial_puts(const char* s)
{
    while (*s) ebpf_serial_putc(*s++);
}

static void ebpf_serial_hex(u64 v)
{
    char buf[17];
    for (int i = 15; i >= 0; i--) {
        int n = (int)((v >> (i * 4)) & 0xF);
        buf[15 - i] = (n < 10) ? ('0' + n) : ('a' + n - 10);
    }
    buf[16] = '\0';
    ebpf_serial_puts(buf);
}

void kapi_ebpf_prog_dump(const bpf_program_t* prog)
{
    if (!prog) return;
    ebpf_serial_puts("[BPF] prog dump: insns=");
    ebpf_serial_hex(prog->num_insns);
    ebpf_serial_puts(" loaded=");
    ebpf_serial_hex(prog->loaded ? 1 : 0);
    ebpf_serial_puts("\n");
    for (u32 i = 0; i < prog->num_insns && i < 64; i++) {
        const bpf_insn_t* ins = &prog->insns[i];
        ebpf_serial_putc(' ');
        ebpf_serial_hex(i);
        ebpf_serial_puts(": code=");
        ebpf_serial_hex(ins->code);
        ebpf_serial_puts(" dst=");
        ebpf_serial_hex(ins->dst_reg);
        ebpf_serial_puts(" src=");
        ebpf_serial_hex(ins->src_reg);
        ebpf_serial_puts(" off=");
        ebpf_serial_hex((u16)ins->off);
        ebpf_serial_puts(" imm=");
        ebpf_serial_hex((u32)ins->imm);
        ebpf_serial_puts("\n");
    }
}
