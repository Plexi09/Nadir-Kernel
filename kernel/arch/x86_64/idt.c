/* Nadir IDT base: 32 CPU-exception gates + fault printer.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) with the stage-2 GDT
 * (kernel code selector 0x18). Privileged primitive: `lidt` in
 * idt_init(). Fault behavior: before idt_init() any fault
 * triple-faults; after, vectors 0-31 print diagnostics and halt.
 * Vectors 32-47 stay not-present until pic_remap() +
 * idt_install_irqs() install the IRQ gates; vectors 48-255 stay
 * not-present (no APIC yet). IF stays cleared; exceptions fire
 * regardless of IF, IRQs only after the integrator runs `sti`.
 */

#include <stdint.h>

#include "console.h"
#include "idt.h"
#include "pic.h"

/* 64-bit interrupt gate descriptor (Intel SDM Vol. 3, IDT gate). */
struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

/* IDTR image for `lidt`. */
struct idtr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

/* Gate flags: present, DPL 0, 64-bit interrupt gate (0xE). */
#define IDT_FLAG_PRESENT_INT 0x8EU
/* Kernel code segment from the stage-2 GDT (entry.asm). */
#define KERNEL_CS 0x18U

/* Full 256-entry table, 16-byte aligned as the CPU prefers. BSS. */
static struct idt_entry idt[256] __attribute__((aligned(16)));
static struct idtr idtr;

/* Stub addresses from isr.asm, one per exception vector 0-31. */
extern uint64_t isr_stub_table[IDT_EXCEPTION_COUNT];

/* Stub addresses from isr.asm, one per remapped IRQ vector 32-47. */
extern uint64_t irq_stub_table[IDT_IRQ_COUNT];

/* Per-IRQ handlers for lines 0-15; NULL means "EOI only".
 * WHY a plain table: one subsystem per file — device drivers own
 * their logic and only register a callback here. BSS-zeroed. */
static void (*irq_handlers[IDT_IRQ_COUNT])(void);

/* Install one gate: handler address + segment + IST index + flags. */
static void idt_set_gate(uint8_t vector, uint64_t handler, uint16_t selector,
                         uint8_t ist, uint8_t flags)
{
    struct idt_entry *e = &idt[vector];

    e->offset_low = (uint16_t)(handler & 0xFFFFU);
    e->selector = selector;
    e->ist = ist & 0x07U;
    e->type_attr = flags;
    e->offset_mid = (uint16_t)((handler >> 16) & 0xFFFFU);
    e->offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFFU);
    e->reserved = 0;
}

/* Human-readable names for vectors 0-31 (Intel SDM Vol. 3). */
static const char *const exception_names[IDT_EXCEPTION_COUNT] = {
    "Divide Error (#DE)", "Debug Exception (#DB)", "NMI",
    "Breakpoint (#BP)", "Overflow (#OF)", "Bound Range (#BR)",
    "Invalid Opcode (#UD)", "Device Not Available (#NM)",
    "Double Fault (#DF)", "Coprocessor Segment Overrun",
    "Invalid TSS (#TS)", "Segment Not Present (#NP)",
    "Stack-Segment Fault (#SS)", "General Protection (#GP)",
    "Page Fault (#PF)", "Reserved (15)",
    "x87 Floating-Point (#MF)", "Alignment Check (#AC)",
    "Machine Check (#MC)", "SIMD Floating-Point (#XM)",
    "Virtualization (#VE)", "Control Protection (#CP)",
    "Reserved (22)", "Reserved (23)", "Reserved (24)", "Reserved (25)",
    "Reserved (26)", "Reserved (27)", "Hypervisor Injection (#HV)",
    "VMM Communication (#VC)", "Security Exception (#SX)", "Reserved (31)",
};

void idt_init(void)
{
    for (uint16_t i = 0; i < IDT_EXCEPTION_COUNT; i++) {
        idt_set_gate((uint8_t)i, isr_stub_table[i], KERNEL_CS, 0,
                     IDT_FLAG_PRESENT_INT);
    }

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base = (uint64_t)&idt[0];
    __asm__ volatile("lidt %0" ::"m"(idtr) : "memory");
}

void idt_install_irqs(void)
{
    for (uint16_t i = 0; i < IDT_IRQ_COUNT; i++) {
        idt_set_gate((uint8_t)(IDT_IRQ_BASE + i), irq_stub_table[i],
                     KERNEL_CS, 0, IDT_FLAG_PRESENT_INT);
    }
}

void irq_register_handler(uint8_t irq, void (*handler)(void))
{
    if (irq >= IDT_IRQ_COUNT) {
        return;
    }
    irq_handlers[irq] = handler;
}

void irq_dispatch(struct interrupt_frame *frame)
{
    uint64_t vector = frame->vector;
    uint8_t irq;

    if (vector < IDT_IRQ_BASE || vector >= IDT_IRQ_BASE + IDT_IRQ_COUNT) {
        return;
    }
    irq = (uint8_t)(vector - IDT_IRQ_BASE);

    /* WHY handler before EOI: the EOI re-arms the (edge-triggered) PIC
     * line, so acking first would let the same IRQ re-enter before the
     * driver finished. A NULL slot just gets an EOI. */
    if (irq_handlers[irq] != 0) {
        irq_handlers[irq]();
    }
    pic_eoi(irq);
}

void exception_handler(struct interrupt_frame *frame)
{
    uint64_t vector = frame->vector;
    const char *name = "Unknown Exception";

    if (vector < IDT_EXCEPTION_COUNT) {
        name = exception_names[vector];
    }

    console_puts("\n*** EXCEPTION: ");
    console_puts(name);
    console_puts(" (vector ");
    console_puthex64(vector);
    console_puts(") ***\nerror: ");
    console_puthex64(frame->error);
    console_puts(" rip: ");
    console_puthex64(frame->rip);
    console_puts(" cs: ");
    console_puthex64(frame->cs);
    console_puts(" rflags: ");
    console_puthex64(frame->rflags);
    console_puts("\nSystem halted.\n");

    for (;;) {
        __asm__ volatile("cli; hlt");
    }
}
