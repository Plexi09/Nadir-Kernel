/* Nadir interrupt IDT + CPU exception entry points.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) with the stage-2 GDT.
 * Privileged primitive: `lidt` in idt_init(). Fault behavior: before
 * idt_init() any exception triple-faults (no IDT); after, CPU vectors
 * 0-31 land in the assembly stubs (isr.asm) and then in
 * exception_handler(), which prints and halts. Vectors 32-255 are left
 * zero (not present) on purpose because PIC remap + IRQ handling is a later step.
 * Interrupts stay disabled throughout (IF=0); exceptions still fire.
 */

#ifndef NADIR_IDT_H
#define NADIR_IDT_H

#include <stdint.h>

/* Number of CPU exception vectors this base installs (0-31). */
#define IDT_EXCEPTION_COUNT 32

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

#endif /* NADIR_IDT_H */
