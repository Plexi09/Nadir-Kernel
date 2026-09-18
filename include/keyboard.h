/* Nadir PS/2 keyboard: scancode set-1 IRQ1 driver with ASCII queue.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0); uses `inb` on ports
 * 0x60 (data) / 0x64 (status), which fault with #GP above CPL 0, so
 * this driver is kernel-only. QEMU/SeaBIOS provides a PS/2 controller
 * in set-1 mode; the 8259 routes IRQ1 to vector 33 after the PIC remap.
 * Fault behavior: port I/O cannot fault at CPL 0. Reading 0x60 when
 * the controller has no byte returns stale data, so keyboard_on_irq()
 * must run only from IRQ1 (data-ready guaranteed).
 *
 * Locking: single core, one producer (IRQ1 handler) and one consumer
 * (polling kmain). `volatile` head/tail plus single-byte loads/stores
 * suffice — no locks, no atomics. IF stays cleared; the queue is both
 * polling-safe and IRQ-safe under that model.
 *
 * Preconditions: call keyboard_init() once before use. It registers
 * keyboard_on_irq() for IRQ1 (via irq_register_handler) and unmasks
 * the line (via pic_unmask); the PIC remap, IRQ-gate install, and
 * `sti` stay integrator duty, and IF stays cleared until then.
 * Failure modes: unknown/extended/release codes are dropped silently;
 * a full queue drops the newest byte; dequeue on empty returns 0.
 */

#ifndef NADIR_KEYBOARD_H
#define NADIR_KEYBOARD_H

/* Clear the queue and modifier state, register keyboard_on_irq() for
 * IRQ1, and unmask IRQ1. */
void keyboard_init(void);

/* IRQ1 entry point: read one scancode from 0x60, translate, enqueue.
 * Runs in interrupt context. */
void keyboard_on_irq(void);

/* Nonzero if at least one translated byte waits in the queue. */
int keyboard_has_key(void);

/* Remove and return the oldest byte, or 0 when the queue is empty.
 * Backspace arrives as '\b', Enter as '\n'. */
char keyboard_dequeue(void);

#endif /* NADIR_KEYBOARD_H */
