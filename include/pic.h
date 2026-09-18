/* Nadir 8259 PIC driver: remap, EOI, IRQ masking.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) with the stage-2 GDT.
 * Privileged instructions: `outb`/`inb` (via io.h) trap with #GP(0)
 * when CPL > IOPL, so every entry point below is Ring 0 only.
 * Fault behavior: no CPU fault is possible beyond a programming bug;
 * a wrong sequence masks the wrong IRQ or loses an EOI (hung line),
 * never a triple fault by itself. IF stays cleared by the caller;
 * this driver never executes `sti`/`cli`.
 */

#ifndef NADIR_PIC_H
#define NADIR_PIC_H

#include <stdint.h>

/* Command/data ports for the master (PIC1) and slave (PIC2). */
#define PIC1_CMD 0x20U
#define PIC1_DATA 0x21U
#define PIC2_CMD 0xA0U
#define PIC2_DATA 0xA1U

/* End-of-interrupt command byte. */
#define PIC_EOI 0x20U

/* Remapped vector bases: IRQ 0-7 -> vectors 32-39, IRQ 8-15 -> 40-47. */
#define PIC_IRQ0_VECTOR 0x20U
#define PIC_IRQ8_VECTOR 0x28U

/* Number of IRQ lines behind the cascaded 8259 pair. */
#define PIC_IRQ_COUNT 16U

/* Remap the cascaded 8259 pair to vectors 32-47 and mask everything.
 * After reset IRQs overlap CPU exceptions 0-15, so without a
 * remap a timer tick looks like a fault. Mask-all leaves each device
 * opt-in via pic_unmask().
 * Preconditions: Ring 0, called once before idt_install_irqs().
 * Failure modes: none. */
void pic_remap(void);

/* Signal end-of-interrupt for IRQ line `irq` (0-15).
 * WHY cascade order: the slave must be acked before the master or the
 * master keeps the cascade line busy and IRQ 2 stops firing.
 * Preconditions: Ring 0, `irq` < 16.
 * Failure modes: out-of-range `irq` is ignored. */
void pic_eoi(uint8_t irq);

/* Mask (disable) IRQ line `irq` (0-15).
 * Preconditions: Ring 0, `irq` < 16.
 * Failure modes: out-of-range `irq` is ignored. */
void pic_mask(uint8_t irq);

/* Unmask (enable) IRQ line `irq` (0-15).
 * Preconditions: Ring 0, `irq` < 16, PIC already remapped.
 * Failure modes: out-of-range `irq` is ignored. */
void pic_unmask(uint8_t irq);

#endif /* NADIR_PIC_H */
