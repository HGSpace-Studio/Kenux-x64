/*
 * kapi_kprobe.c —— kprobe / kretprobe 动态追踪机制实现
 *
 * 工作原理:
 *   1. 注册时，将被探测地址首字节替换为 INT3 (0xCC)，原字节保存
 *   2. CPU 执行到 INT3 触发 #BP 异常 (向量 3)
 *   3. IDT #BP handler 调用 kapi_kprobe_int3_handler()
 *   4. 查找探针 -> 调用 pre_handler -> 临时恢复原字节 -> 单步执行 (TF=1)
 *      -> #DB 触发 -> 重新写入 INT3 -> 调用 post_handler -> 返回
 *
 * 限制: 单步执行依赖 EFLAGS.TF 触发 #DB，需要在 IDT 中挂接 #DB handler。
 *      本文件提供 kapi_kprobe_int3_handler 入口，#DB 处理由调用者集成。
 */

#include "kapi_kprobe.h"
#include "kapi.h"

#include <arch/types.h>
#include <arch/spinlock.h>
#include <string.h>
#include <slab.h>

/* 符号表查询（在 stacktrace.c 中实现，kprobe 用它做 name -> addr 反查） */
typedef struct { uint64_t addr; const char* name; } ksym_entry_t;
extern uint64_t kernel_symbols_start[2];
extern uint64_t kernel_symbols_end[1];

static ksym_entry_t* kprobe_ksym_table(void) { return (ksym_entry_t*)kernel_symbols_start; }
static ksym_entry_t* kprobe_ksym_end(void) { return (ksym_entry_t*)kernel_symbols_end; }

static void* kprobe_lookup_name(const char* name)
{
    if (!name) return NULL;
    ksym_entry_t* sym = kprobe_ksym_table();
    ksym_entry_t* end = kprobe_ksym_end();
    while (sym < end) {
        if (sym->name && strcmp(sym->name, name) == 0) {
            return (void*)sym->addr;
        }
        sym++;
    }
    return NULL;
}

/* 探针链表 */
static kprobe_t* g_kprobe_list = NULL;
static kretprobe_t* g_kretprobe_list = NULL;
static spinlock_t g_kprobe_lock = SPINLOCK_INIT;
static int g_kprobe_inited = 0;

/* 单步执行中等待 #DB 的探针（同一时刻只支持一个） */
static kprobe_t* g_single_step_probe = NULL;

/* 写入一个字节到指定地址（绕过只读保护需要写 CR0.WP，这里简化为直接写） */
static void write_byte(uint8_t* addr, uint8_t val)
{
    /* 关闭 CR0.WP 以允许写只读 .text 段 */
    uint64_t cr0;
    __asm__ volatile ("movq %%cr0, %0" : "=r"(cr0));
    __asm__ volatile ("movq %0, %%cr0" : : "r"(cr0 & ~(1ULL << 16)));  /* clear WP */
    *addr = val;
    __asm__ volatile ("movq %0, %%cr0" : : "r"(cr0));  /* restore */
}

int kapi_kprobe_init(void)
{
    if (g_kprobe_inited) return KAPI_OK;
    spin_init(&g_kprobe_lock);
    g_kprobe_list = NULL;
    g_kretprobe_list = NULL;
    g_single_step_probe = NULL;
    g_kprobe_inited = 1;
    return KAPI_OK;
}

int kapi_kprobe_register(kprobe_t* kp)
{
    if (!kp || !kp->addr || kp->in_use) return KAPI_EINVAL;

    spin_lock(&g_kprobe_lock);
    /* 检查地址是否已被探测 */
    kprobe_t* cur = g_kprobe_list;
    while (cur) {
        if (cur->addr == kp->addr) {
            spin_unlock(&g_kprobe_lock);
            return KAPI_EBUSY;
        }
        cur = cur->next;
    }

    kp->saved_opcode = *(uint8_t*)kp->addr;
    kp->state = KPROBE_STATE_DISABLED;
    kp->in_use = 1;
    kp->hit_count = 0;
    kp->next = g_kprobe_list;
    g_kprobe_list = kp;
    spin_unlock(&g_kprobe_lock);
    return KAPI_OK;
}

int kapi_kprobe_unregister(kprobe_t* kp)
{
    if (!kp || !kp->in_use) return KAPI_EINVAL;

    /* 先禁用 */
    if (kp->state == KPROBE_STATE_ACTIVE) {
        kapi_kprobe_disable(kp);
    }

    spin_lock(&g_kprobe_lock);
    kprobe_t** pp = &g_kprobe_list;
    while (*pp) {
        if (*pp == kp) {
            *pp = kp->next;
            kp->next = NULL;
            kp->in_use = 0;
            spin_unlock(&g_kprobe_lock);
            return KAPI_OK;
        }
        pp = &(*pp)->next;
    }
    spin_unlock(&g_kprobe_lock);
    return KAPI_ENOENT;
}

int kapi_kprobe_enable(kprobe_t* kp)
{
    if (!kp || !kp->in_use) return KAPI_EINVAL;
    if (kp->state == KPROBE_STATE_ACTIVE) return KAPI_OK;
    /* 写入 INT3 */
    write_byte((uint8_t*)kp->addr, 0xCC);
    /* 内存屏障确保写入对其他 CPU 可见 */
    __asm__ volatile ("" ::: "memory");
    kp->state = KPROBE_STATE_ACTIVE;
    return KAPI_OK;
}

int kapi_kprobe_disable(kprobe_t* kp)
{
    if (!kp || !kp->in_use) return KAPI_EINVAL;
    if (kp->state != KPROBE_STATE_ACTIVE) return KAPI_OK;
    /* 恢复原 opcode */
    write_byte((uint8_t*)kp->addr, kp->saved_opcode);
    __asm__ volatile ("" ::: "memory");
    kp->state = KPROBE_STATE_DISABLED;
    return KAPI_OK;
}

int kapi_kretprobe_register(kretprobe_t* rp)
{
    if (!rp || !rp->addr || rp->in_use) return KAPI_EINVAL;

    spin_lock(&g_kprobe_lock);
    rp->saved_opcode = *(uint8_t*)rp->addr;
    rp->state = KPROBE_STATE_DISABLED;
    rp->in_use = 1;
    rp->hit_count = 0;
    rp->next = g_kretprobe_list;
    g_kretprobe_list = rp;

    /* 自动启用 */
    write_byte((uint8_t*)rp->addr, 0xCC);
    rp->state = KPROBE_STATE_ACTIVE;

    spin_unlock(&g_kprobe_lock);
    return KAPI_OK;
}

int kapi_kretprobe_unregister(kretprobe_t* rp)
{
    if (!rp || !rp->in_use) return KAPI_EINVAL;

    spin_lock(&g_kprobe_lock);
    if (rp->state == KPROBE_STATE_ACTIVE) {
        write_byte((uint8_t*)rp->addr, rp->saved_opcode);
        rp->state = KPROBE_STATE_DISABLED;
    }

    kretprobe_t** pp = &g_kretprobe_list;
    while (*pp) {
        if (*pp == rp) {
            *pp = rp->next;
            rp->next = NULL;
            rp->in_use = 0;
            spin_unlock(&g_kprobe_lock);
            return KAPI_OK;
        }
        pp = &(*pp)->next;
    }
    spin_unlock(&g_kprobe_lock);
    return KAPI_ENOENT;
}

kprobe_t* kapi_kprobe_register_by_name(const char* func_name,
                                        kprobe_handler_t pre,
                                        kprobe_handler_t post)
{
    void* addr = kprobe_lookup_name(func_name);
    if (!addr) return NULL;

    kprobe_t* kp = (kprobe_t*)kzalloc(sizeof(kprobe_t));
    if (!kp) return NULL;
    strncpy(kp->name, func_name, KPROBE_NAME_MAX - 1);
    kp->addr = addr;
    kp->pre_handler = pre;
    kp->post_handler = post;

    if (kapi_kprobe_register(kp) != KAPI_OK) {
        kfree(kp);
        return NULL;
    }
    kapi_kprobe_enable(kp);
    return kp;
}

/* INT3 (#BP) 中断处理入口 */
void kapi_kprobe_int3_handler(kprobe_regs_t* regs)
{
    /* rip 此时指向 INT3 之后的指令，被探测地址是 rip - 1 */
    uint64_t probe_addr = regs->rip - 1;
    kprobe_t* hit = NULL;

    spin_lock(&g_kprobe_lock);
    kprobe_t* cur = g_kprobe_list;
    while (cur) {
        if (cur->state == KPROBE_STATE_ACTIVE && (uint64_t)cur->addr == probe_addr) {
            hit = cur;
            break;
        }
        cur = cur->next;
    }

    /* kretprobe 也参与查找 */
    kretprobe_t* hit_ret = NULL;
    if (!hit) {
        kretprobe_t* rc = g_kretprobe_list;
        while (rc) {
            if (rc->state == KPROBE_STATE_ACTIVE && (uint64_t)rc->addr == probe_addr) {
                hit_ret = rc;
                break;
            }
            rc = rc->next;
        }
    }
    spin_unlock(&g_kprobe_lock);

    if (hit) {
        hit->hit_count++;
        /* 调用 pre_handler */
        if (hit->pre_handler) {
            hit->pre_handler(hit->name, regs);
        }
        /* 临时恢复原指令并单步执行 */
        kapi_kprobe_resume_execution(hit, regs);
        /* 调用 post_handler */
        if (hit->post_handler) {
            hit->post_handler(hit->name, regs);
        }
    } else if (hit_ret) {
        hit_ret->hit_count++;
        if (hit_ret->handler) {
            hit_ret->handler(hit_ret->name, regs);
        }
        /* kretprobe: 简化处理，直接恢复并继续 */
        write_byte((uint8_t*)hit_ret->addr, hit_ret->saved_opcode);
        regs->rip = probe_addr;  /* 退回到原指令 */
    } else {
        /* 未注册探针的 INT3：忽略（可能是其他用途） */
    }
}

/* 单步执行原指令后重新安装断点
 * 实现: 临时恢复原字节 -> 设置 EFLAGS.TF 触发 #DB -> 在 #DB 中重装断点
 * 简化: 直接执行原字节并推进 RIP（仅对 1 字节指令安全，多字节指令需解码）
 */
void kapi_kprobe_resume_execution(kprobe_t* kp, kprobe_regs_t* regs)
{
    if (!kp || !regs) return;

    /* 恢复原字节 */
    write_byte((uint8_t*)kp->addr, kp->saved_opcode);

    /* 设置 TF 标志，下一条指令执行后触发 #DB */
    regs->rflags |= (1ULL << 8);

    /* 记录当前探针，#DB handler 会用它重装断点 */
    g_single_step_probe = kp;

    /* rip 退回到原指令 */
    regs->rip = (uint64_t)kp->addr;
}

/* #DB (调试异常) 处理入口 —— 由 IDT #DB handler 在单步完成后调用
 * 用于重装 kprobe 断点
 */
void kapi_kprobe_debug_handler(kprobe_regs_t* regs)
{
    (void)regs;
    if (g_single_step_probe) {
        write_byte((uint8_t*)g_single_step_probe->addr, 0xCC);
        g_single_step_probe = NULL;
        /* 清除 TF */
        if (regs) regs->rflags &= ~(1ULL << 8);
    }
}

int kapi_kprobe_count(void)
{
    int n = 0;
    spin_lock(&g_kprobe_lock);
    kprobe_t* cur = g_kprobe_list;
    while (cur) { n++; cur = cur->next; }
    spin_unlock(&g_kprobe_lock);
    return n;
}

kprobe_t* kapi_kprobe_get_first(void)
{
    return g_kprobe_list;
}

kprobe_t* kapi_kprobe_get_next(kprobe_t* cur)
{
    return cur ? cur->next : NULL;
}

kretprobe_t* kapi_kretprobe_get_first(void)
{
    return g_kretprobe_list;
}
