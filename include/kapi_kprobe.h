#ifndef KAPI_KPROBE_H
#define KAPI_KPROBE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* kprobe / kretprobe 动态追踪机制
 *
 * 简化实现：通过维护探针链表，在被探测函数入口/出口处插入 INT3 断点
 * (0xCC) 替换原指令首字节。INT3 中断处理程序查找探针并调用其处理函数。
 *
 * 限制:
 *   - 不支持嵌套 kprobe（同地址只能注册一个探针）
 *   - 不支持 NMI 上下文中的探针
 *   - 单步执行原指令后返回（避免复杂的相对跳转重写）
 */

#define KPROBE_NAME_MAX     64
#define KPROBE_MAX          64
#define KRETPROBE_MAX       32

/* 探针状态 */
typedef enum {
    KPROBE_STATE_DISABLED = 0,
    KPROBE_STATE_ACTIVE   = 1,
} kprobe_state_t;

/* 探针命中时的寄存器快照（与 arch/x86_64 CPU 状态一致） */
typedef struct kprobe_regs {
    uint64_t r15, r14, r13, r12;
    uint64_t rbp, rbx;
    uint64_t r11, r10, r9, r8;
    uint64_t rax, rcx, rdx, rsi, rdi;
    uint64_t orig_rax;
    uint64_t rip, cs, rflags, rsp, ss;
} kprobe_regs_t;

/* 探针处理函数原型 */
typedef void (*kprobe_handler_t)(const char* name, kprobe_regs_t* regs);
typedef void (*kretprobe_handler_t)(const char* name, kprobe_regs_t* regs);

/* kprobe 描述符 */
typedef struct kprobe {
    char               name[KPROBE_NAME_MAX];  /* 函数名（调试用） */
    void*              addr;                    /* 被探测地址 */
    kprobe_handler_t   pre_handler;             /* 入口前处理 */
    kprobe_handler_t   post_handler;            /* 入口后处理（可选） */
    uint8_t            saved_opcode;            /* 原 opcode 首字节 */
    uint8_t            state;                   /* kprobe_state_t */
    int                in_use;
    uint64_t           hit_count;
    struct kprobe*     next;
} kprobe_t;

/* kretprobe 描述符 */
typedef struct kretprobe {
    char                  name[KPROBE_NAME_MAX];
    void*                 addr;
    kretprobe_handler_t   handler;              /* 返回时处理 */
    uint8_t               saved_opcode;
    uint8_t               state;
    int                   in_use;
    uint64_t              hit_count;
    struct kretprobe*     next;
} kretprobe_t;

/* 初始化 kprobe 子系统 */
int kapi_kprobe_init(void);

/* 注册/注销 kprobe */
int kapi_kprobe_register(kprobe_t* kp);
int kapi_kprobe_unregister(kprobe_t* kp);

/* 启用/禁用 */
int kapi_kprobe_enable(kprobe_t* kp);
int kapi_kprobe_disable(kprobe_t* kp);

/* 注册/注销 kretprobe */
int kapi_kretprobe_register(kretprobe_t* rp);
int kapi_kretprobe_unregister(kretprobe_t* rp);

/* 通过函数名注册（自动查找地址） */
kprobe_t* kapi_kprobe_register_by_name(const char* func_name,
                                        kprobe_handler_t pre,
                                        kprobe_handler_t post);

/* INT3 中断处理入口（由 IDT #BP handler 调用） */
void kapi_kprobe_int3_handler(kprobe_regs_t* regs);

/* 探针命中后单步执行原指令再重新安装断点 */
void kapi_kprobe_resume_execution(kprobe_t* kp, kprobe_regs_t* regs);

/* 统计与查询 */
int      kapi_kprobe_count(void);
kprobe_t* kapi_kprobe_get_first(void);
kprobe_t* kapi_kprobe_get_next(kprobe_t* cur);
kretprobe_t* kapi_kretprobe_get_first(void);

#ifdef __cplusplus
}
#endif

#endif
