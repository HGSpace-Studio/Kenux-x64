/*
 * NTVDM (16-bit DOS Virtual Machine) + WOW64 process bridging.
 *
 * Freestanding implementation that provides a simplified 8086 instruction
 * interpreter, an 8259 PIC / keyboard hardware model, and a WOW64 process
 * registry for bridging 32-bit (i386) and 64-bit (AMD64) processes.
 */

#ifndef _WDM_LOCAL_PTR_TYPEDEFS
#define _WDM_LOCAL_PTR_TYPEDEFS
typedef char CCHAR;
typedef struct _DEVICE_OBJECT DEVICE_OBJECT;
typedef struct _DRIVER_OBJECT DRIVER_OBJECT;
typedef struct _IRP IRP;
typedef struct _IO_STACK_LOCATION IO_STACK_LOCATION;
typedef DEVICE_OBJECT* PDEVICE_OBJECT;
typedef DRIVER_OBJECT* PDRIVER_OBJECT;
typedef IRP* PIRP;
typedef IO_STACK_LOCATION* PIO_STACK_LOCATION;
#endif

#include <arch/win32.h>
#include <string.h>
#include <stdio.h>

extern void* memory_alloc(uint64_t size);
extern void  memory_free(void* ptr);

/* ================================================================== *
 * Constants & register IDs
 * ================================================================== */

#define MAX_VDM_CONTEXTS 4
#define VDM_MEM_SIZE     (1024 * 1024)   /* 1 MB per context */

/* Register identifiers for ntvdm_get_reg / ntvdm_set_reg. */
#define VDM_REG_AX    0
#define VDM_REG_BX    1
#define VDM_REG_CX    2
#define VDM_REG_DX    3
#define VDM_REG_SI    4
#define VDM_REG_DI    5
#define VDM_REG_BP    6
#define VDM_REG_SP    7
#define VDM_REG_CS    8
#define VDM_REG_DS    9
#define VDM_REG_ES    10
#define VDM_REG_SS    11
#define VDM_REG_IP    12
#define VDM_REG_FLAGS 13

/* Offsets into VDM_CONTEXT.registers[] for 16-bit GP registers (LE). */
#define REG_OFF_AX 0
#define REG_OFF_BX 2
#define REG_OFF_CX 4
#define REG_OFF_DX 6
#define REG_OFF_SI 8
#define REG_OFF_DI 10
#define REG_OFF_BP 12

#define WOW64_MAX_PROCS 64

/* ================================================================== *
 * Static state pools
 * ================================================================== */

static VDM_CONTEXT  g_vdm_contexts[MAX_VDM_CONTEXTS];
static VDM_HARDWARE g_vdm_hardware[MAX_VDM_CONTEXTS];

static WOW64_PROCESS_INFO g_wow64_procs[WOW64_MAX_PROCS];
static int                g_wow64_count;
static uint16_t           g_native_machine;

/* ================================================================== *
 * Internal helpers
 * ================================================================== */

static uint16_t vdm_reg16(const VDM_CONTEXT* ctx, int off)
{
    return (uint16_t)((uint16_t)ctx->registers[off] |
                      ((uint16_t)ctx->registers[off + 1] << 8));
}

static void vdm_set_reg16(VDM_CONTEXT* ctx, int off, uint16_t val)
{
    ctx->registers[off]     = (uint8_t)(val & 0xFF);
    ctx->registers[off + 1] = (uint8_t)((val >> 8) & 0xFF);
}

/* Map a VDM_CONTEXT pointer back to its hardware block, if the context
 * lives in the static pool.  Returns NULL for externally-allocated
 * contexts (which have no associated hardware state). */
static VDM_HARDWARE* vdm_find_hw(VDM_CONTEXT* ctx)
{
    int i;
    if (ctx == NULL) return NULL;
    for (i = 0; i < MAX_VDM_CONTEXTS; i++) {
        if (&g_vdm_contexts[i] == ctx)
            return &g_vdm_hardware[i];
    }
    return NULL;
}

/* ================================================================== *
 * 1. NTVDM - 16-bit DOS Virtual Machine
 * ================================================================== */

int ntvdm_init(void)
{
    memset(g_vdm_contexts, 0, sizeof(g_vdm_contexts));
    memset(g_vdm_hardware, 0, sizeof(g_vdm_hardware));
    return 0;
}

int ntvdm_load_program(VDM_CONTEXT* ctx, const char* path)
{
    if (ctx == NULL)
        return -1;

    if (ctx->memory == NULL) {
        ctx->memory = (uint8_t*)memory_alloc(VDM_MEM_SIZE);
        if (ctx->memory == NULL)
            return -1;
    }
    ctx->memory_size = VDM_MEM_SIZE;
    memset(ctx->memory, 0, ctx->memory_size);

    if (path != NULL) {
        size_t n = strlen(path);
        if (n >= sizeof(ctx->program_name))
            n = sizeof(ctx->program_name) - 1;
        memcpy(ctx->program_name, path, n);
        ctx->program_name[n] = '\0';
    } else {
        ctx->program_name[0] = '\0';
    }

    /* Real-mode COM-style startup: CS=0, IP=0x100. */
    ctx->cs     = 0x0000;
    ctx->ip     = 0x0100;
    ctx->ss     = 0x0000;
    ctx->sp     = 0xFFFE;
    ctx->ds     = 0x0000;
    ctx->es     = 0x0000;
    ctx->eflags = 0x0002;   /* bit 1 is always set */

    memset(ctx->registers, 0, sizeof(ctx->registers));

    ctx->running           = 0;
    ctx->paused            = 0;
    ctx->initialized       = 1;
    ctx->cycle_count       = 0;
    ctx->instruction_count = 0;

    /* Write a minimal program: INT 0x20 (DOS terminate) at 0x100. */
    if (ctx->memory_size >= 0x102) {
        ctx->memory[0x100] = 0xCD;   /* INT */
        ctx->memory[0x101] = 0x20;   /* 0x20 */
    }

    return 0;
}

int ntvdm_execute(VDM_CONTEXT* ctx, uint32_t max_cycles)
{
    uint32_t cycle;

    if (ctx == NULL || ctx->memory == NULL)
        return 0;

    ctx->running = 1;
    ctx->paused  = 0;

    for (cycle = 0; cycle < max_cycles; cycle++) {
        uint32_t addr;
        uint8_t  op;

        if (!ctx->running || ctx->paused)
            break;

        addr = ((uint32_t)ctx->cs << 4) + ctx->ip;
        if (addr >= ctx->memory_size) {
            ctx->running = 0;
            break;
        }

        op = ctx->memory[addr];

        switch (op) {
        case 0xCD: {   /* INT imm8 */
            uint8_t inum = 0;
            if (addr + 1 < ctx->memory_size)
                inum = ctx->memory[addr + 1];
            ctx->ip += 2;
            ntvdm_handle_int(ctx, inum);
            break;
        }
        case 0xB4: {   /* MOV AH, imm8 */
            uint8_t imm = 0;
            if (addr + 1 < ctx->memory_size)
                imm = ctx->memory[addr + 1];
            ctx->registers[1] = imm;       /* AH */
            ctx->ip += 2;
            break;
        }
        case 0xB0: {   /* MOV AL, imm8 */
            uint8_t imm = 0;
            if (addr + 1 < ctx->memory_size)
                imm = ctx->memory[addr + 1];
            ctx->registers[0] = imm;       /* AL */
            ctx->ip += 2;
            break;
        }
        case 0xEB: {   /* JMP short rel8 */
            int8_t rel = 0;
            if (addr + 1 < ctx->memory_size)
                rel = (int8_t)ctx->memory[addr + 1];
            ctx->ip = (uint16_t)(ctx->ip + 2 + (int16_t)rel);
            break;
        }
        case 0x90:     /* NOP */
            ctx->ip += 1;
            break;
        case 0xF4:     /* HLT */
            ctx->paused = 1;
            ctx->ip += 1;
            break;
        case 0xC3: {   /* RET - pop IP from stack */
            uint32_t sp_addr = ((uint32_t)ctx->ss << 4) + ctx->sp;
            if (sp_addr + 1 < ctx->memory_size) {
                uint16_t ret = (uint16_t)((uint16_t)ctx->memory[sp_addr] |
                                          ((uint16_t)ctx->memory[sp_addr + 1] << 8));
                ctx->ip = ret;
                ctx->sp = (uint16_t)(ctx->sp + 2);
            } else {
                ctx->ip += 1;
            }
            break;
        }
        case 0x00:     /* ADD r/m8, r8 */
        case 0x01:     /* ADD r/m16, r16 */
        case 0x02:     /* ADD r8, r/m8 */
        case 0x03:     /* ADD r16, r/m16 */
        case 0x04:     /* ADD AL, imm8 */
        case 0x05:     /* ADD AX, imm16 */
            /* Simplified: assume a ModRM or immediate operand. */
            ctx->ip += 2;
            break;
        default:
            /* Unknown opcode: skip one byte. */
            ctx->ip += 1;
            break;
        }

        ctx->cycle_count++;
        ctx->instruction_count++;

        if (!ctx->running)
            break;
    }

    return (int)ctx->instruction_count;
}

int ntvdm_terminate(VDM_CONTEXT* ctx)
{
    if (ctx == NULL)
        return -1;
    ctx->running     = 0;
    ctx->initialized = 0;
    return 0;
}

int ntvdm_step(VDM_CONTEXT* ctx)
{
    uint32_t addr;
    uint8_t  op;

    if (ctx == NULL || ctx->memory == NULL || !ctx->initialized)
        return 0;

    addr = ((uint32_t)ctx->cs << 4) + ctx->ip;
    if (addr >= ctx->memory_size)
        return 0;

    op = ctx->memory[addr];

    switch (op) {
    case 0xCD: {   /* INT imm8 */
        uint8_t inum = 0;
        if (addr + 1 < ctx->memory_size)
            inum = ctx->memory[addr + 1];
        ctx->ip += 2;
        ntvdm_handle_int(ctx, inum);
        break;
    }
    case 0xB4: {   /* MOV AH, imm8 */
        uint8_t imm = 0;
        if (addr + 1 < ctx->memory_size)
            imm = ctx->memory[addr + 1];
        ctx->registers[1] = imm;
        ctx->ip += 2;
        break;
    }
    case 0xB0: {   /* MOV AL, imm8 */
        uint8_t imm = 0;
        if (addr + 1 < ctx->memory_size)
            imm = ctx->memory[addr + 1];
        ctx->registers[0] = imm;
        ctx->ip += 2;
        break;
    }
    case 0xEB: {   /* JMP short rel8 */
        int8_t rel = 0;
        if (addr + 1 < ctx->memory_size)
            rel = (int8_t)ctx->memory[addr + 1];
        ctx->ip = (uint16_t)(ctx->ip + 2 + (int16_t)rel);
        break;
    }
    case 0x90:
        ctx->ip += 1;
        break;
    case 0xF4:
        ctx->paused = 1;
        ctx->ip += 1;
        break;
    case 0xC3: {   /* RET */
        uint32_t sp_addr = ((uint32_t)ctx->ss << 4) + ctx->sp;
        if (sp_addr + 1 < ctx->memory_size) {
            uint16_t ret = (uint16_t)((uint16_t)ctx->memory[sp_addr] |
                                      ((uint16_t)ctx->memory[sp_addr + 1] << 8));
            ctx->ip = ret;
            ctx->sp = (uint16_t)(ctx->sp + 2);
        } else {
            ctx->ip += 1;
        }
        break;
    }
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
        ctx->ip += 2;
        break;
    default:
        ctx->ip += 1;
        break;
    }

    ctx->cycle_count++;
    ctx->instruction_count++;
    return 0;
}

int ntvdm_get_reg(VDM_CONTEXT* ctx, int reg_id, uint32_t* value)
{
    if (ctx == NULL || value == NULL)
        return -1;

    switch (reg_id) {
    case VDM_REG_AX:    *value = vdm_reg16(ctx, REG_OFF_AX); break;
    case VDM_REG_BX:    *value = vdm_reg16(ctx, REG_OFF_BX); break;
    case VDM_REG_CX:    *value = vdm_reg16(ctx, REG_OFF_CX); break;
    case VDM_REG_DX:    *value = vdm_reg16(ctx, REG_OFF_DX); break;
    case VDM_REG_SI:    *value = vdm_reg16(ctx, REG_OFF_SI); break;
    case VDM_REG_DI:    *value = vdm_reg16(ctx, REG_OFF_DI); break;
    case VDM_REG_BP:    *value = vdm_reg16(ctx, REG_OFF_BP); break;
    case VDM_REG_SP:    *value = ctx->sp;     break;
    case VDM_REG_CS:    *value = ctx->cs;     break;
    case VDM_REG_DS:    *value = ctx->ds;     break;
    case VDM_REG_ES:    *value = ctx->es;     break;
    case VDM_REG_SS:    *value = ctx->ss;     break;
    case VDM_REG_IP:    *value = ctx->ip;     break;
    case VDM_REG_FLAGS: *value = ctx->eflags; break;
    default:
        return -1;
    }
    return 0;
}

int ntvdm_set_reg(VDM_CONTEXT* ctx, int reg_id, uint32_t value)
{
    uint16_t v = (uint16_t)(value & 0xFFFF);

    if (ctx == NULL)
        return -1;

    switch (reg_id) {
    case VDM_REG_AX:    vdm_set_reg16(ctx, REG_OFF_AX, v); break;
    case VDM_REG_BX:    vdm_set_reg16(ctx, REG_OFF_BX, v); break;
    case VDM_REG_CX:    vdm_set_reg16(ctx, REG_OFF_CX, v); break;
    case VDM_REG_DX:    vdm_set_reg16(ctx, REG_OFF_DX, v); break;
    case VDM_REG_SI:    vdm_set_reg16(ctx, REG_OFF_SI, v); break;
    case VDM_REG_DI:    vdm_set_reg16(ctx, REG_OFF_DI, v); break;
    case VDM_REG_BP:    vdm_set_reg16(ctx, REG_OFF_BP, v); break;
    case VDM_REG_SP:    ctx->sp     = v; break;
    case VDM_REG_CS:    ctx->cs     = v; break;
    case VDM_REG_DS:    ctx->ds     = v; break;
    case VDM_REG_ES:    ctx->es     = v; break;
    case VDM_REG_SS:    ctx->ss     = v; break;
    case VDM_REG_IP:    ctx->ip     = v; break;
    case VDM_REG_FLAGS: ctx->eflags = value; break;
    default:
        return -1;
    }
    return 0;
}

uint8_t ntvdm_read_mem8(VDM_CONTEXT* ctx, uint32_t addr)
{
    if (ctx == NULL || ctx->memory == NULL || addr >= ctx->memory_size)
        return 0;
    return ctx->memory[addr];
}

void ntvdm_write_mem8(VDM_CONTEXT* ctx, uint32_t addr, uint8_t val)
{
    if (ctx == NULL || ctx->memory == NULL || addr >= ctx->memory_size)
        return;
    ctx->memory[addr] = val;
}

uint16_t ntvdm_read_mem16(VDM_CONTEXT* ctx, uint32_t addr)
{
    if (ctx == NULL || ctx->memory == NULL)
        return 0;
    if (addr + 1 >= ctx->memory_size)
        return 0;
    return (uint16_t)((uint16_t)ctx->memory[addr] |
                     ((uint16_t)ctx->memory[addr + 1] << 8));
}

void ntvdm_write_mem16(VDM_CONTEXT* ctx, uint32_t addr, uint16_t val)
{
    if (ctx == NULL || ctx->memory == NULL)
        return;
    if (addr + 1 >= ctx->memory_size)
        return;
    ctx->memory[addr]     = (uint8_t)(val & 0xFF);
    ctx->memory[addr + 1] = (uint8_t)((val >> 8) & 0xFF);
}

uint16_t ntvdm_read_io(VDM_CONTEXT* ctx, uint16_t port)
{
    VDM_HARDWARE* hw = vdm_find_hw(ctx);

    if (hw != NULL) {
        switch (port) {
        case 0x20:   /* 8259 PIC - Interrupt Mask Register */
            return hw->pic_imr;
        case 0x21:   /* 8259 PIC - Interrupt Request Register */
            return hw->pic_irr;
        case 0x40:   /* 8254 PIT channel 0 */
        case 0x41:   /* channel 1 */
        case 0x42:   /* channel 2 */
        case 0x43:   /* mode/command */
            return hw->pit_counter[port - 0x40];
        case 0x60: { /* keyboard controller */
            uint8_t sc = 0;
            if (vdm_hw_keyboard_pop(hw, &sc))
                return sc;
            return 0;
        }
        case 0x61:   /* PPI speaker */
            return 0;
        case 0x70:   /* CMOS address */
            return 0;
        case 0x71:   /* CMOS data */
            return hw->cmos[0];
        default:
            return 0xFF;
        }
    }
    return 0xFF;
}

void ntvdm_write_io(VDM_CONTEXT* ctx, uint16_t port, uint16_t val)
{
    VDM_HARDWARE* hw = vdm_find_hw(ctx);
    uint8_t v = (uint8_t)(val & 0xFF);

    if (hw != NULL) {
        switch (port) {
        case 0x20:   /* PIC EOI */
            vdm_hw_pic_acknowledge(hw);
            break;
        case 0x21:   /* PIC IMR */
            hw->pic_imr = v;
            break;
        case 0x40:
        case 0x41:
        case 0x42:
        case 0x43:
            hw->pit_counter[port - 0x40] = v;
            break;
        case 0x61:   /* speaker */
            break;
        case 0x70:   /* CMOS address register */
            if (v < 128)
                hw->cmos[0] = v;
            break;
        case 0x71: { /* CMOS data */
            uint8_t idx = hw->cmos[0];
            if (idx < 128)
                hw->cmos[idx] = v;
            break;
        }
        default:
            break;
        }
    }
}

int ntvdm_handle_int(VDM_CONTEXT* ctx, uint8_t int_num)
{
    if (ctx == NULL)
        return 0;

    switch (int_num) {
    case 0x10:   /* Video BIOS */
        return 0;

    case 0x16: { /* Keyboard BIOS - return key from buffer */
        VDM_HARDWARE* hw = vdm_find_hw(ctx);
        uint8_t sc = 0;
        if (hw != NULL)
            vdm_hw_keyboard_pop(hw, &sc);
        ctx->registers[0] = sc;       /* AL */
        return 0;
    }

    case 0x19:   /* Boot load */
        return 0;

    case 0x20:   /* DOS terminate */
        ctx->running = 0;
        return 0;

    case 0x21: { /* DOS API */
        uint8_t ah = ctx->registers[1];
        switch (ah) {
        case 0x4C:   /* terminate with return code (AL) */
            ctx->running = 0;
            break;
        case 0x09:   /* print '$'-terminated string at DS:DX */
            /* No console in simulation. */
            break;
        case 0x01: { /* read character with echo */
            VDM_HARDWARE* hw = vdm_find_hw(ctx);
            uint8_t sc = 0;
            if (hw != NULL)
                vdm_hw_keyboard_pop(hw, &sc);
            ctx->registers[0] = sc;   /* AL */
            break;
        }
        default:
            break;
        }
        return 0;
    }

    default:
        return 0;
    }
}

void ntvdm_dump_state(const VDM_CONTEXT* ctx)
{
    static char buf[256];

    if (ctx == NULL)
        return;

    sprintf(buf,
            "CS:IP=%04X:%04X SS:SP=%04X:%04X AX=%04X BX=%04X CX=%04X DX=%04X FLAGS=%08X",
            ctx->cs, ctx->ip, ctx->ss, ctx->sp,
            vdm_reg16(ctx, REG_OFF_AX), vdm_reg16(ctx, REG_OFF_BX),
            vdm_reg16(ctx, REG_OFF_CX), vdm_reg16(ctx, REG_OFF_DX),
            ctx->eflags);
    /* buf holds the formatted state; a real console would display it. */
}

/* ================================================================== *
 * 2. VDM Hardware (8259 PIC + Keyboard)
 * ================================================================== */

void vdm_hw_init(VDM_HARDWARE* hw)
{
    if (hw == NULL)
        return;
    memset(hw, 0, sizeof(*hw));
    /* Mask all IRQs except IRQ2 (cascade line). */
    hw->pic_imr = 0xFB;
}

uint8_t vdm_hw_pic_get_interrupt(VDM_HARDWARE* hw)
{
    uint8_t pending;
    int i;

    if (hw == NULL)
        return 0xFF;

    pending = (uint8_t)(hw->pic_irr & ~hw->pic_imr & ~hw->pic_isr);

    for (i = 0; i < 8; i++) {
        if (pending & (uint8_t)(1u << i)) {
            hw->pic_isr |= (uint8_t)(1u << i);
            hw->pic_irr &= (uint8_t)~(uint8_t)(1u << i);
            return (uint8_t)(0x08 + i);   /* IRQ0-7 -> vectors 0x08-0x0F */
        }
    }
    return 0xFF;
}

void vdm_hw_pic_request(VDM_HARDWARE* hw, uint8_t irq)
{
    if (hw == NULL || irq > 7)
        return;
    hw->pic_irr |= (uint8_t)(1u << irq);
}

void vdm_hw_pic_acknowledge(VDM_HARDWARE* hw)
{
    int i;

    if (hw == NULL)
        return;

    /* EOI: clear the highest-priority (lowest-numbered) bit in ISR. */
    for (i = 0; i < 8; i++) {
        if (hw->pic_isr & (uint8_t)(1u << i)) {
            hw->pic_isr &= (uint8_t)~(uint8_t)(1u << i);
            return;
        }
    }
}

void vdm_hw_keyboard_push(VDM_HARDWARE* hw, uint8_t scancode)
{
    if (hw == NULL)
        return;
    if (hw->keyboard_count >= 16)
        return;   /* buffer full */
    hw->keyboard_buffer[hw->keyboard_tail] = scancode;
    hw->keyboard_tail = (uint8_t)((hw->keyboard_tail + 1) & 0x0F);
    hw->keyboard_count++;
}

int vdm_hw_keyboard_pop(VDM_HARDWARE* hw, uint8_t* scancode)
{
    if (hw == NULL || scancode == NULL)
        return 0;
    if (hw->keyboard_count == 0)
        return 0;   /* empty */
    *scancode = hw->keyboard_buffer[hw->keyboard_head];
    hw->keyboard_head = (uint8_t)((hw->keyboard_head + 1) & 0x0F);
    hw->keyboard_count--;
    return 1;
}

/* ================================================================== *
 * 3. WOW64 Process Bridging
 * ================================================================== */

int wow64_init(void)
{
    memset(g_wow64_procs, 0, sizeof(g_wow64_procs));
    g_wow64_count    = 0;
    g_native_machine = IMAGE_FILE_MACHINE_AMD64;
    return 0;
}

int wow64_get_process_machines(uint32_t process_id,
                                uint16_t* process_machine,
                                uint16_t* native_machine)
{
    int i;

    if (native_machine != NULL)
        *native_machine = IMAGE_FILE_MACHINE_AMD64;

    if (process_machine != NULL)
        *process_machine = IMAGE_FILE_MACHINE_AMD64;   /* default: native */

    if (process_id == 0)
        return 0;

    for (i = 0; i < WOW64_MAX_PROCS; i++) {
        if (g_wow64_procs[i].process_id == process_id) {
            if (g_wow64_procs[i].is_wow64 && process_machine != NULL)
                *process_machine = IMAGE_FILE_MACHINE_I386;
            break;
        }
    }
    return 0;
}

int wow64_is_wow64_process(uint32_t process_id)
{
    int i;

    if (process_id == 0)
        return 0;

    for (i = 0; i < WOW64_MAX_PROCS; i++) {
        if (g_wow64_procs[i].process_id == process_id)
            return g_wow64_procs[i].is_wow64 ? 1 : 0;
    }
    return 0;
}

int wow64_register_process(uint32_t process_id, uint16_t machine_type)
{
    int i;

    if (process_id == 0)
        return -1;

    for (i = 0; i < WOW64_MAX_PROCS; i++) {
        if (g_wow64_procs[i].process_id == 0) {
            g_wow64_procs[i].process_id   = process_id;
            g_wow64_procs[i].machine_type = machine_type;
            g_wow64_procs[i].is_wow64     = (machine_type != IMAGE_FILE_MACHINE_AMD64) ? 1 : 0;
            g_wow64_procs[i].wow64_info   = NULL;
            g_wow64_count++;
            return 0;
        }
    }
    return -1;   /* table full */
}

int wow64_unregister_process(uint32_t process_id)
{
    int i;

    if (process_id == 0)
        return 0;

    for (i = 0; i < WOW64_MAX_PROCS; i++) {
        if (g_wow64_procs[i].process_id == process_id) {
            memset(&g_wow64_procs[i], 0, sizeof(g_wow64_procs[i]));
            if (g_wow64_count > 0)
                g_wow64_count--;
            return 0;
        }
    }
    return 0;
}

int wow64_thunk_parameter(void* param, uint16_t src_machine,
                          uint16_t dst_machine, uint32_t param_type)
{
    if (src_machine == dst_machine)
        return 0;
    if (param == NULL)
        return 0;

    if (param_type == 0) {   /* pointer */
        if (src_machine == IMAGE_FILE_MACHINE_I386 &&
            dst_machine == IMAGE_FILE_MACHINE_AMD64) {
            /* i386 -> AMD64: widen 32-bit pointer to 64-bit. */
            uint32_t lo = 0;
            uint64_t wide;
            memcpy(&lo, param, sizeof(lo));
            wide = (uint64_t)lo;
            memcpy(param, &wide, sizeof(wide));
        } else if (src_machine == IMAGE_FILE_MACHINE_AMD64 &&
                   dst_machine == IMAGE_FILE_MACHINE_I386) {
            /* AMD64 -> i386: narrow 64-bit pointer (copy low 32 bits). */
            uint64_t val = 0;
            uint32_t lo;
            memcpy(&val, param, sizeof(val));
            lo = (uint32_t)(val & 0xFFFFFFFFu);
            memcpy(param, &lo, sizeof(lo));
        }
    }
    /* Handles (1) and structures (2): no-op in this simulation. */
    return 0;
}
