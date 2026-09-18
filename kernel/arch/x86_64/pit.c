/* 8254 PIT timer driver (see pit.h).
 *
 * Programs channel 0 in mode 3 (square-wave generator) at 100Hz on IRQ0.
 *
 * Why 1193182Hz base: the PIT input clock is the IBM-PC crystal frequency
 * (14.31818MHz / 12); every PC-compatible PIT ticks at this rate, so the
 * divisor for N Hz is always 1193182 / N.
 * Why mode 3: square-wave output stays high/low for half the period and
 * retriggers the IRQ line cleanly each cycle; mode 2 (rate generator)
 * emits narrow pulses that are fine too, but mode 3 is the conventional
 * choice for periodic system ticks on channel 0.
 * Why volatile + no locking: single-core kernel; s_ticks is written only
 * in IRQ0 context and read in mainline code. `volatile` keeps the compiler
 * from caching the counter across the hlt loop, and on x86-64 an aligned
 * 64-bit load/store is atomic, so no spinlock is needed while IF gates the
 * writer.
 */

#include <stdint.h>

#include "idt.h"
#include "io.h"
#include "pit.h"

#define PIT_CMD 0x43U
#define PIT_CH0 0x40U
/* Ch0, lobyte/hibyte access, mode 3 square-wave, binary (not BCD). */
#define PIT_CMD_CH0_MODE3 0x36U

/* Compile-time guard: a zero divisor would mean 65536 to the hardware and
 * silently run at ~18Hz instead of PIT_HZ. Frequency is fixed on purpose. */
_Static_assert(PIT_DIVISOR != 0, "PIT divisor must not be zero");
_Static_assert(PIT_DIVISOR <= 0xFFFFU, "PIT divisor must fit in 16 bits");

/* Port I/O is owned by include/io.h (single owner); irq_register_handler
 * is owned by idt.c via include/idt.h. */

/* Monotonic tick counter: written in IRQ context. */
static volatile uint64_t s_ticks;
/* pit_init is idempotent; re-entry after the first programming is a no-op. */
static uint8_t s_initialized;

void pit_init(void)
{
    uint16_t divisor;
    uint8_t lo;
    uint8_t hi;

    if (s_initialized != 0) {
        return;
    }

    /* No cli/sti here on purpose: latch programming is two plain outb
     * writes (safe with IF=0 at boot) and interrupt policy belongs to the
     * caller (kmain integrator), which unmasks IRQ0 and stis afterwards. */
    divisor = (uint16_t)PIT_DIVISOR;
    lo = (uint8_t)(divisor & 0xFFU);
    hi = (uint8_t)((divisor >> 8) & 0xFFU);

    /* Latch order matters: command byte first selects ch0 + lobyte/hibyte
     * mode, then the divisor follows low byte first, high byte second. */
    outb(PIT_CMD, PIT_CMD_CH0_MODE3);
    outb(PIT_CH0, lo);
    outb(PIT_CH0, hi);

    /* Strong idt.h registration: IRQ0 -> vector 32 wires pit_on_tick. */
    irq_register_handler(PIT_IRQ, pit_on_tick);

    s_initialized = 1;
}

void pit_on_tick(void)
{
    /* Single increment async-safe, no logging or port I/O here. The PIC
     * EOI is owned by the IRQ dispatcher not this driver. */
    s_ticks++;
}

uint64_t pit_ticks(void)
{
    return s_ticks;
}

void pit_sleep(uint64_t ticks)
{
    uint64_t start = s_ticks;

    /* hlt (not a spin loop) so the CPU sleeps until the next interrupt.
     * Assumes IF=1 and IRQ0 unmasked; with IF=0 this would halt forever,
     * which is why pit_init leaves interrupt policy to the caller. The
     * unsigned delta keeps working if s_ticks wraps mid-sleep. */
    while ((s_ticks - start) < ticks) {
        __asm__ volatile("hlt");
    }
}
