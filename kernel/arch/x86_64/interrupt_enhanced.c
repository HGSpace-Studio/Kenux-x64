#include <arch/interrupt.h>
#include <arch/apic.h>
#include <arch/ioapic.h>
#include <arch/spinlock.h>
#include <string.h>
#include <arch/time.h>

#define MAX_INTERRUPT_HANDLERS 256
#define MAX_ISR_CHAIN_DEPTH   8

typedef struct irq_handler {
    int (*handler)(void* data);
    void* data;
    uint32_t flags;
#define IRQ_FLAG_SHARED     0x01
#define IRQ_FLAG_EDGE       0x02
#define IRQ_FLAG_LEVEL      0x04
#define IRQ_FLAG_HIGH       0x08
#define IRQ_FLAG_LOW        0x10
#define IRQ_FLAG_RISING     0x20
#define IRQ_FLAG_FALLING    0x40
    char name[32];
    int use_count;
    struct irq_handler* next;
} irq_handler_t;

typedef struct {
    irq_handler_t* handlers[MAX_ISR_CHAIN_DEPTH];
    int handler_count;
    spinlock_t lock;
    uint32_t trigger_type;
    uint32_t polarity;
    bool enabled;
    bool pending;
    bool in_progress;
    uint64_t last_fired_time;
    uint64_t fire_count;
    uint64_t spurious_count;
    uint64_t total_handle_time_ns;
    uint64_t max_handle_time_ns;
    uint64_t avg_handle_time_ns;
} irq_desc_t;

static irq_desc_t irq_descriptors[MAX_INTERRUPT_HANDLERS];

typedef struct interrupt_stats {
    uint64_t total_interrupts;
    uint64_t spurious_interrupts;
    uint64_t handled_interrupts;
    uint64_t masked_interrupts;
    uint64_t deferred_interrupts;
    uint64_t nested_interrupts;
    uint64_t max_nesting_depth;
    uint64_t total_handle_time_ns;
    uint64_t avg_latency_ns;
    uint64_t max_latency_ns;
    uint64_t context_switches_from_irq;
    int current_irq_depth;
    int cpu_id;
    double interrupt_load;
} interrupt_stats_t;

static interrupt_stats_t irq_stats[MAX_CPUS];
static spinlock_t irq_global_lock;
static int initialized = 0;

static void (*default_irq_handlers[256])(int, registers_t*) = {NULL};

void interrupt_system_init(void)
{
    if (initialized) return;

    spin_init(&irq_global_lock);

    memset(irq_descriptors, 0, sizeof(irq_descriptors));
    memset(irq_stats, 0, sizeof(irq_stats));

    for (int i = 0; i < MAX_INTERRUPT_HANDLERS; i++) {
        spin_init(&irq_descriptors[i].lock);
        irq_descriptors[i].trigger_type = IRQ_TRIGGER_EDGE;
        irq_descriptors[i].polarity = IRQ_POLARITY_HIGH;
    }

    for (int i = 0; i < MAX_CPUS; i++) {
        irq_stats[i].cpu_id = i;
    }

    initialized = 1;
}

int request_irq(uint32_t irq, int (*handler)(void*), void* data,
                uint32_t flags, const char* name)
{
    if (!initialized || !handler || irq >= MAX_INTERRUPT_HANDLERS) return -EINVAL;

    irq_desc_t* desc = &irq_descriptors[irq];
    spin_lock(&desc->lock);

    if (desc->handler_count >= MAX_ISR_CHAIN_DEPTH) {
        spin_unlock(&desc->lock);
        return -EBUSY;
    }

    for (int i = 0; i < desc->handler_count; i++) {
        if (!(desc->handlers[i]->flags & IRQ_FLAG_SHARED)) {
            spin_unlock(&desc->lock);
            return -EBUSY;
        }
    }

    irq_handler_t* new_handler = kzalloc(sizeof(irq_handler_t));
    if (!new_handler) {
        spin_unlock(&desc->lock);
        return -ENOMEM;
    }

    new_handler->handler = handler;
    new_handler->data = data;
    new_handler->flags = flags | IRQ_FLAG_EDGE;
    if (name) strncpy(new_handler->name, name, 31);
    new_handler->use_count = 0;
    new_handler->next = NULL;

    desc->handlers[desc->handler_count++] = new_handler;
    desc->enabled = true;

    spin_unlock(&desc->lock);

    ioapic_enable_irq(irq, flags & IRQ_FLAG_LEVEL ? IOAPIC_TRIGGER_LEVEL : IOAPIC_TRIGGER_EDGE,
                      flags & IRQ_FLAG_LOW ? IOAPIC_POLARITY_LOW : IOAPIC_POLARITY_HIGH);

    apic_unmask_irq(irq_to_vector(irq));

    return 0;
}

void free_irq(uint32_t irq, void* data)
{
    if (!initialized || irq >= MAX_INTERRUPT_HANDLERS) return;

    irq_desc_t* desc = &irq_descriptors[irq];
    spin_lock(&desc->lock);

    for (int i = 0; i < desc->handler_count; i++) {
        if (desc->handlers[i] && desc->handlers[i]->data == data) {
            kfree(desc->handlers[i]);
            desc->handlers[i] = NULL;

            for (int j = i; j < desc->handler_count - 1; j++) {
                desc->handlers[j] = desc->handlers[j + 1];
            }
            desc->handler_count--;
            break;
        }
    }

    if (desc->handler_count == 0) {
        desc->enabled = false;
        apic_mask_irq(irq_to_vector(irq));
    }

    spin_unlock(&desc->lock);
}

void enable_irq(uint32_t irq)
{
    if (!initialized || irq >= MAX_INTERRUPT_HANDLERS) return;

    irq_descriptors[irq].enabled = true;
    apic_unmask_irq(irq_to_vector(irq));
}

void disable_irq(uint32_t irq)
{
    if (!initialized || irq >= MAX_INTERRUPT_HANDLERS) return;

    irq_descriptors[irq].enabled = false;
    apic_mask_irq(irq_to_vector(irq));
}

bool is_irq_enabled(uint32_t irq)
{
    if (!initialized || irq >= MAX_INTERRUPT_HANDLERS) return false;
    return irq_descriptors[irq].enabled;
}

void ack_irq(uint32_t irq)
{
    if (irq >= MAX_INTERRUPT_HANDLERS) return;

    apic_eoi();
    if (ioapic_is_level_triggered(irq)) {
        ioapic_eoi(irq);
    }
}

void set_irq_trigger(uint32_t irq, uint32_t trigger, uint32_t polarity)
{
    if (!initialized || irq >= MAX_INTERRUPT_HANDLERS) return;

    irq_desc_t* desc = &irq_descriptors[irq];
    desc->trigger_type = trigger;
    desc->polarity = polarity;

    ioapic_configure_irq(irq, trigger == IRQ_TRIGGER_LEVEL ? IOAPIC_TRIGGER_LEVEL : IOAPIC_TRIGGER_EDGE,
                         polarity == IRQ_POLARITY_LOW ? IOAPIC_POLARITY_LOW : IOAPIC_POLARITY_HIGH);
}

uint64_t handle_irq_chain(uint32_t irq, registers_t* regs)
{
    if (!initialized || irq >= MAX_INTERRUPT_HANDLERS) return 0;

    int cpu = smp_processor_id();
    if (cpu >= MAX_CPUS) cpu = 0;

    irq_desc_t* desc = &irq_descriptors[irq];
    interrupt_stats_t* stats = &irq_stats[cpu];

    stats->total_interrupts++;
    stats->current_irq_depth++;

    if (stats->current_irq_depth > stats->max_nesting_depth) {
        stats->max_nesting_depth = stats->current_irq_depth;
    }

    if (stats->current_irq_depth > 1) {
        stats->nested_interrupts++;
    }

    uint64_t start_time = get_current_time_ns();

    spin_lock(&desc->lock);

    if (!desc->enabled || desc->handler_count == 0) {
        desc->spurious_count++;
        stats->spurious_interrupts++;
        spin_unlock(&desc->lock);
        stats->current_irq_depth--;
        return 0;
    }

    desc->fire_count++;
    desc->last_fired_time = start_time;
    desc->in_progress = true;

    int ret = 0;
    for (int i = 0; i < desc->handler_count; i++) {
        irq_handler_t* h = desc->handlers[i];
        if (h && h->handler) {
            h->use_count++;

            spin_unlock(&desc->lock);
            ret = h->handler(h->data);
            spin_lock(&desc->lock);

            if (ret == IRQ_HANDLED) break;
        }
    }

    desc->in_progress = false;
    spin_unlock(&desc->lock);

    uint64_t handle_time = get_current_time_ns() - start_time;
    desc->total_handle_time_ns += handle_time;

    if (handle_time > desc->max_handle_time_ns) {
        desc->max_handle_time_ns = handle_time;
    }
    desc->avg_handle_time_ns =
        (desc->avg_handle_time_ns * (desc->fire_count - 1) + handle_time) / desc->fire_count;

    stats->handled_interrupts++;
    stats->total_handle_time_ns += handle_time;

    if (ret == IRQ_HANDLED) {
        stats->avg_latency_ns =
            (stats->avg_latency_ns + handle_time) / 2;
        if (handle_time > stats->max_latency_ns) {
            stats->max_latency_ns = handle_time;
        }
    }

    stats->current_irq_depth--;

    return handle_time;
}

void default_irq_handler(int irq, registers_t* regs)
{
    if (irq >= 256 || !default_irq_handlers[irq]) return;

    default_irq_handlers[irq](irq, regs);

    ack_irq(irq);
}

void register_default_handler(int irq, void (*handler)(int, registers_t*))
{
    if (irq >= 256 || !handler) return;
    default_irq_handlers[irq] = handler;
}

void unregister_default_handler(int irq)
{
    if (irq >= 256) return;
    default_irq_handlers[irq] = NULL;
}

void mask_all_irqs(void)
{
    if (!initialized) return;

    for (int i = 0; i < MAX_INTERRUPT_HANDLERS; i++) {
        if (irq_descriptors[i].enabled) {
            apic_mask_irq(irq_to_vector(i));
            irq_stats[smp_processor_id()].masked_interrupts++;
        }
    }
}

void unmask_all_irqs(void)
{
    if (!initialized) return;

    for (int i = 0; i < MAX_INTERRUPT_HANDLERS; i++) {
        if (irq_descriptors[i].enabled) {
            apic_unmask_irq(irq_to_vector(i));
        }
    }
}

int get_irq_stats(int irq, irq_desc_t* desc)
{
    if (!desc || irq >= MAX_INTERRUPT_HANDLERS || !initialized) return -EINVAL;

    spin_lock(&irq_descriptors[irq].lock);
    memcpy(desc, &irq_descriptors[irq], sizeof(irq_desc_t));
    spin_unlock(&irq_descriptors[irq].lock);

    return 0;
}

int get_system_interrupt_stats(int cpu, interrupt_stats_t* stats)
{
    if (!stats || cpu >= MAX_CPUS || !initialized) return -EINVAL;

    memcpy(stats, &irq_stats[cpu], sizeof(interrupt_stats_t));

    if (stats->total_interrupts > 0) {
        stats->interrupt_load =
            (double)stats->total_handle_time_ns /
            (double)get_elapsed_time_ns() * 100.0;
    } else {
        stats->interrupt_load = 0.0;
    }

    return 0;
}

void reset_interrupt_stats(void)
{
    if (!initialized) return;

    int cpu = smp_processor_id();
    if (cpu >= MAX_CPUS) cpu = 0;

    memset(&irq_stats[cpu], 0, sizeof(interrupt_stats_t));
    irq_stats[cpu].cpu_id = cpu;
}

void dump_interrupt_info(void)
{
    if (!initialized) return;

    printk("Interrupt System Status:\n\n");

    for (int i = 0; i < MAX_INTERRUPT_HANDLERS; i++) {
        irq_desc_t* desc = &irq_descriptors[i];

        if (desc->fire_count > 0 || desc->spurious_count > 0) {
            printk("IRQ %d:\n", i);
            printk("  Enabled:      %s\n", desc->enabled ? "yes" : "no");
            printk("  Handlers:     %d\n", desc->handler_count);
            printk("  Fire count:   %llu\n", desc->fire_count);
            printk("  Spurious:     %llu\n", desc->spurious_count);
            printk("  Total time:   %llu us\n",
                   desc->total_handle_time_ns / 1000ULL);
            printk("  Avg time:     %llu ns\n", desc->avg_handle_time_ns);
            printk("  Max time:     %llu us\n",
                   desc->max_handle_time_ns / 1000ULL);
            printk("  Last fired:   %llu ms ago\n",
                   (get_current_time_ns() - desc->last_fired_time) / 1000000ULL);

            for (int j = 0; j < desc->handler_count; j++) {
                irq_handler_t* h = desc->handlers[j];
                if (h) {
                    printk("  Handler[%d]: %s (calls=%d)\n",
                           j, h->name, h->use_count);
                }
            }
            printk("\n");
        }
    }

    int cpu = smp_processor_id();
    if (cpu >= MAX_CPUS) cpu = 0;
    interrupt_stats_t* stats = &irq_stats[cpu];

    printk("CPU %d Statistics:\n", cpu);
    printk("  Total interrupts:    %llu\n", stats->total_interrupts);
    printk("  Handled:             %llu\n", stats->handled_interrupts);
    printk("  Spurious:            %llu\n", stats->spurious_interrupts);
    printk("  Masked:              %llu\n", stats->masked_interrupts);
    printk("  Nested:              %llu\n", stats->nested_interrupts);
    printk("  Max nesting depth:   %llu\n", stats->max_nesting_depth);
    printk("  Total handle time:   %llu ms\n",
           stats->total_handle_time_ns / 1000000ULL);
    printk("  Avg latency:         %llu ns\n", stats->avg_latency_ns);
    printk("  Max latency:         %llu us\n",
           stats->max_latency_ns / 1000ULL);
    printk("  Current depth:       %d\n", stats->current_irq_depth);
    printk("  Interrupt load:      %.2f%%\n", stats->interrupt_load);
}

void init_irq_controller(void)
{
    if (!initialized) return;

    ioapic_init();
    apic_init();

    for (int i = 0; i < 16; i++) {
        ioapic_redirect_irq(i, i + IRQ_OFFSET);
    }

    apic_set_spurious_vector(SPURIOUS_INT_VECTOR);
    apic_enable();
}