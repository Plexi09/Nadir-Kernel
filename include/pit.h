/* Nadir 8254 PIT timer driver (see pit.c).
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0): programs the PIT via `outb`
 * to ISA ports PIT_CMD (0x43) and PIT_CH0 (0x40), and handles IRQ0
 * (8259 PIC IRQ0 -> CPU vector 32 after pic_remap).
 *
 * Ports used: 0x43 (command/mode register, write-only), 0x40 (channel 0
 * data, write-only for the divisor latch).
 *
 * Preconditions: caller owns interrupt policy — pit_init() neither enables
 * (sti) nor disables (cli) interrupts; target init order is
 * pic_remap -> idt_install_irqs -> pit_init -> pic_unmask(0) -> sti.
 * pit_sleep() requires interrupts enabled, otherwise `hlt` never wakes.
 *
 * Failure modes: divisor is fixed at compile time (PIT_DIVISOR); a zero
 * divisor would mean "65536" to the hardware and the wrong rate, so it is
 * rejected with a compile-time _Static_assert. Frequency is fixed at 100Hz;
 * any other rate needs a deliberate driver change, not a runtime argument.
 */

#ifndef NADIR_PIT_H
#define NADIR_PIT_H

#include <stdint.h>

/* PIT input clock: crystal-derived base rate shared by all IBM-PC PITs. */
#define PIT_BASE_HZ 1193182U
/* Driver tick rate: 100Hz = 10ms per tick. */
#define PIT_HZ 100U
/* Divisor latched into channel 0 (1193182/100 = 11931). */
#define PIT_DIVISOR (PIT_BASE_HZ / PIT_HZ)
/* 8259 PIC IRQ line carrying channel-0 ticks (remapped to vector 32). */
#define PIT_IRQ 0U

/* Program channel 0 for mode 3 square-wave at PIT_HZ and register
 * pit_on_tick as the IRQ0 handler (if irq_register_handler is linked in).
 * Ring 0 only. Idempotent: second and later calls do nothing. Leaves IF
 * exactly as found; the caller decides when to sti. */
void pit_init(void);

/* IRQ0 handler: increments the tick counter. Called from IRQ context
 * (interrupts already gated by the CPU/IRQ stub); must stay async-safe
 * and never block. */
void pit_on_tick(void);

/* Monotonic tick count since pit_init (10ms per tick at 100Hz). Wraps only
 * after ~5.8 billion years; unsigned subtraction stays correct across it. */
uint64_t pit_ticks(void);

/* Busy-wait `ticks` ticks using `hlt` to save power between interrupts.
 * Requires interrupts enabled (IF=1) and the PIT IRQ unmasked, otherwise
 * the CPU sleeps forever. Uses unsigned delta so a counter wrap mid-sleep
 * still terminates. */
void pit_sleep(uint64_t ticks);

#endif /* NADIR_PIT_H */
