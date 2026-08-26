#ifndef _KAPI_GDB_PROTO_H
#define _KAPI_GDB_PROTO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Cross-domain GDB Remote Serial Protocol (RSP) extension.
 *
 * Supports multiple transport backends (COM1 serial, TCP socket, named pipe,
 * in-memory buffer) and multiple debug targets. The packet layer implements the
 * classic "$payload#checksum" framing with ACK ('+') / NACK ('-') handshake.
 *
 * This is a freestanding, static-allocation component: no dynamic memory is
 * used. All state lives in compile-time sized arrays.
 */

#define KAPI_GDB_PKT_BUF_SIZE   4096   /* Max packet payload size           */
#define KAPI_GDB_MAX_TARGETS    8       /* Max simultaneously debugged targets */
#define KAPI_GDB_MAX_TRANSPORTS 4      /* Max registered transport backends    */
#define KAPI_GDB_MAX_BP_TARGET  16     /* Max breakpoints per target          */
#define KAPI_GDB_MAX_MEM_XFER   512     /* Max bytes per memory transfer reply */

/* ---- Transport backend types ------------------------------------------ */
typedef enum {
    KAPI_GDB_TRANSPORT_COM1 = 0,   /* COM1 16550 UART serial port */
    KAPI_GDB_TRANSPORT_TCP,        /* TCP socket (loopback agent)  */
    KAPI_GDB_TRANSPORT_PIPE,       /* Named pipe / FIFO            */
    KAPI_GDB_TRANSPORT_MEM,        /* In-memory ring buffer        */
} kapi_gdb_transport_type_t;

/* ---- Breakpoint types ------------------------------------------------- */
typedef enum {
    KAPI_GDB_BP_SOFT = 0,          /* Software breakpoint: INT3 (0xCC) */
    KAPI_GDB_BP_HW,                /* Hardware breakpoint (DR0..DR3)  */
    KAPI_GDB_BP_WATCH_READ,        /* Read watchpoint                 */
    KAPI_GDB_BP_WATCH_WRITE,       /* Write watchpoint                */
    KAPI_GDB_BP_WATCH_ACCESS,      /* Read/Write access watchpoint    */
} kapi_gdb_bp_type_t;

/* GDB stop reason signal numbers (subset of the POSIX signal set GDB uses). */
#define KAPI_GDB_SIGINT         2
#define KAPI_GDB_SIGTRAP        5
#define KAPI_GDB_SIGKILL        9
#define KAPI_GDB_SIGSTOP        19

/* ---- x86_64 register set ---------------------------------------------- */
typedef struct kapi_gdb_regs {
    /* General purpose registers (16) */
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8,  r9,  r10, r11;
    uint64_t r12, r13, r14, r15;
    /* Instruction pointer and flags */
    uint64_t rip;
    uint64_t rflags;
    /* Segment registers */
    uint64_t cs, ss, ds, es, fs, gs;
    /* Control registers (debugging context) */
    uint64_t cr0, cr2, cr3, cr4;
} kapi_gdb_regs_t;

/* ---- Breakpoint descriptor -------------------------------------------- */
typedef struct kapi_gdb_bp {
    int                 in_use;    /* Slot occupied flag                */
    kapi_gdb_bp_type_t  type;      /* Breakpoint kind                  */
    uint64_t            addr;      /* Target address                   */
    uint64_t            size;      /* Watch length (1/2/4/8), 0 = code */
    uint8_t             saved;     /* Original byte (software BP only) */
} kapi_gdb_bp_t;

/* ---- Transport backend ------------------------------------------------ */
typedef struct kapi_gdb_transport {
    int                          in_use;
    kapi_gdb_transport_type_t    type;
    const char*                  name;
    int                          (*init)(void);          /* Backend setup        */
    int                          (*can_read)(void);      /* Data available?     */
    int                          (*can_write)(void);     /* Can send a byte?    */
    char                         (*get_char)(void);       /* Blocking read 1 byte*/
    void                         (*put_char)(char c);    /* Blocking write 1 byte*/
    void                         (*flush)(void);         /* Drain output (opt.) */
} kapi_gdb_transport_t;

/* ---- Debug target ----------------------------------------------------- */
typedef struct kapi_gdb_target {
    int              in_use;
    int              id;                 /* External target id              */
    int              attached;           /* Attached to a transport?         */
    int              transport_id;       /* Index into gdb_transports[]      */
    int              active;             /* Inside the monitor loop?         */
    int              last_signal;        /* Last reported stop signal        */
    kapi_gdb_regs_t  regs;               /* Cached register snapshot         */
    kapi_gdb_bp_t    bps[KAPI_GDB_MAX_BP_TARGET]; /* Per-target breakpoints */
    int              bp_count;
} kapi_gdb_target_t;

/* ---- API -------------------------------------------------------------- */

/* Initialize the GDB RSP subsystem (clears targets/transports). */
void kapi_gdb_init(void);

/* Register a transport backend. Returns the transport id (>=0) or -1. */
int kapi_gdb_register_transport(kapi_gdb_transport_t* t);

/* Attach a debug target to a registered transport. Returns target id or -1. */
int kapi_gdb_attach_target(int target_id, int transport_id);

/* Main packet entry point: read one packet and dispatch it for `target_id`.
 * Returns 0 to continue, 1 if a detach/kill was requested. */
int kapi_gdb_handle_packet(int target_id);

/* Breakpoint management (software BP uses 0xCC and saves the original byte). */
int kapi_gdb_set_bp(int target_id, kapi_gdb_bp_type_t type,
                    uint64_t addr, uint64_t size);
int kapi_gdb_clear_bp(int target_id, kapi_gdb_bp_type_t type, uint64_t addr);

/* Register access (reads/writes the cached register snapshot of a target). */
int kapi_gdb_read_regs(int target_id, kapi_gdb_regs_t* out);
int kapi_gdb_write_regs(int target_id, const kapi_gdb_regs_t* in);

/* Memory access (direct virtual memory, used by 'm'/'M' packets). */
int kapi_gdb_read_mem(uint64_t addr, uint8_t* buf, uint64_t len);
int kapi_gdb_write_mem(uint64_t addr, const uint8_t* buf, uint64_t len);

/* Execution control. continue() returns the stop signal, step() sets TF. */
int kapi_gdb_continue(int target_id);
int kapi_gdb_step(int target_id);

#ifdef __cplusplus
}
#endif

#endif /* _KAPI_GDB_PROTO_H */
