/* Nadir interrupt IDT + CPU exception entry points.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) with the stage-2 GDT.
 * Privileged primitive: `lidt` in idt_init(). Fault behavior: before
 * idt_init() any exception triple-faults (no IDT); after, CPU vectors
 * 0-31 land in the assembly stubs (isr.asm) and then in
 * exception_handler(), which prints and halts. Vectors 32-47 stay
 * not-present until the 8259 PIC is remapped and idt_install_irqs()
 * installs the IRQ gates; then they land in irq_dispatch(), which
 * sends the PIC EOI and calls the per-IRQ handler (if any).
 * Interrupts stay disabled throughout (IF=0); exceptions still fire.
 * IRQs only fire after the integrator enables them with `sti`.
 */

#ifndef NADIR_IDT_H
#define NADIR_IDT_H

#include <stdint.h>

/* Number of CPU exception vectors this base installs (0-31). */
#define IDT_EXCEPTION_COUNT 32

/* First remapped IRQ vector (IRQ 0 -> vector 32) and IRQ line count. */
#define IDT_IRQ_BASE 32
#define IDT_IRQ_COUNT 16

/* Stack layout on entry to exception_handler().
 *
 * Built by the CPU (RIP/CS/RFLAGS, plus an error code on vectors
 * 8, 10-14, 17, 21) plus isr.asm: the stub normalizes the stack to
 * [vector, error] and isr_common pushes the 15 general-purpose
 * registers. RDI points at this struct (System V first argument).
 * Field order matches the push order in isr_common — keep in sync.
 */
struct interrupt_frame {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rbp;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    /* Pushed by the stub: exception vector 0-31. */
    uint64_t vector;
    /* CPU-pushed error code, or 0 inserted by the stub. */
    uint64_t error;
    /* Pushed by the CPU. */
    uint64_t rip;
    uint64_t cs;
    uint64_t rflags;
    /* Pushed by the CPU only on a privilege change; garbage for
     * kernel-mode faults */
    uint64_t rsp;
    uint64_t ss;
};

/* Build the IDT (vectors 0-31 -> isr stubs) and load it with `lidt`.
 * Ring 0 only. Leaves IF cleared; returns with exceptions catchable. */
void idt_init(void);

/* C entry point for vectors 0-31, called from isr_common with the
 * frame above. Prints vector/error/RIP and halts; never returns. */
void exception_handler(struct interrupt_frame *frame);

/* C entry point for vectors 32-47, called from irq_common with the
 * same frame layout (vector = 32 + irq, error = 0 dummy). Runs at
 * Ring 0 with IF cleared; sends the PIC EOI and calls the handler
 * registered for that IRQ, if any. Spurious vectors outside 32-47
 * are ignored. */
void irq_dispatch(struct interrupt_frame *frame);

/* Register `handler` for IRQ line `irq` (0-15); NULL unregisters.
 * Preconditions: Ring 0, `irq` < 16 (out-of-range calls ignored).
 * Failure modes: none; takes effect on the next IRQ after return. */
void irq_register_handler(uint8_t irq, void (*handler)(void));

/* Install the 16 IRQ gates (vectors 32-47 -> irq stubs) with selector
 * 0x18 and flags 0x8E (present, DPL 0, 64-bit interrupt gate).
 * Preconditions: Ring 0, idt_init() already ran, PIC already remapped
 * via pic_remap(). Leaves IF cleared. Failure modes: none. */
void idt_install_irqs(void);

#endif /* NADIR_IDT_H */
