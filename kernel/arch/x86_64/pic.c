/* Nadir 8259 PIC driver: cascaded master/slave remap + EOI + masking.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) with the stage-2 GDT.
 * Privileged instructions: `outb`/`inb` via io.h (trap #GP if CPL >
 * IOPL); no `cli`/`sti`/`lidt`.
 * Fault behavior: no CPU fault possible a wrong ICW order only
 * misroutes IRQs.
 */

#include <stdint.h>

#include "io.h"
#include "pic.h"

/* Init control words: ICW1 starts init, ICW2 sets the vector base,
 * ICW3 wires the cascade (master IRQ2 <-> slave identity 2), ICW4
 * selects 8086 mode. io_wait() after each byte gives the slow 8259
 * time to latch it. */
#define PIC_ICW1_INIT 0x11U
#define PIC_ICW3_MASTER_SLAVE_LINE 0x04U
#define PIC_ICW3_SLAVE_ID 0x02U
#define PIC_ICW4_8086 0x01U

void pic_remap(void)
{
    /* WHY this exact order: the 8259 latches ICW2-4 only while in init
     * mode, and the cascade identities must match or slave IRQs never
     * reach the CPU. Mask-all at the end keeps devices quiet until
     * their driver calls pic_unmask(). */
    outb(PIC1_CMD, PIC_ICW1_INIT);
    io_wait();
    outb(PIC2_CMD, PIC_ICW1_INIT);
    io_wait();
    outb(PIC1_DATA, PIC_IRQ0_VECTOR);
    io_wait();
    outb(PIC2_DATA, PIC_IRQ8_VECTOR);
    io_wait();
    outb(PIC1_DATA, PIC_ICW3_MASTER_SLAVE_LINE);
    io_wait();
    outb(PIC2_DATA, PIC_ICW3_SLAVE_ID);
    io_wait();
    outb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC1_DATA, 0xFFU);
    io_wait();
    outb(PIC2_DATA, 0xFFU);
    io_wait();
}

void pic_eoi(uint8_t irq)
{
    if (irq >= PIC_IRQ_COUNT) {
        return;
    }

    if (irq >= 8U) {
        outb(PIC2_CMD, PIC_EOI);
    }
    outb(PIC1_CMD, PIC_EOI);
}

void pic_mask(uint8_t irq)
{
    uint16_t port;
    uint8_t mask;

    if (irq >= PIC_IRQ_COUNT) {
        return;
    }

    port = (irq < 8U) ? PIC1_DATA : PIC2_DATA;
    mask = inb(port) | (uint8_t)(1U << (irq & 7U));
    outb(port, mask);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port;
    uint8_t mask;

    if (irq >= PIC_IRQ_COUNT) {
        return;
    }

    port = (irq < 8U) ? PIC1_DATA : PIC2_DATA;
    mask = inb(port) & (uint8_t) ~(1U << (irq & 7U));
    outb(port, mask);
}
