/* Nadir serial log: COM1 (16550 UART) polling driver.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0); uses `inb`/`outb` which fault
 * with #GP if executed above CPL 0, so this driver is kernel-only.
 * Ports: COM1 0x3F8-0x3FF (data, int-enable, divisor latch, FIFO, line
 * control, modem control, line status). QEMU/SeaBIOS provides COM1; the
 * VGA console stays primary and serial is the secondary log.
 * Fault behavior: port I/O cannot fault at CPL 0. The TX poll spins on
 * LSR bit 5 and hangs only if the UART hardware is absent or wedged —
 * call serial_ok() first to check LSR sanity.
 *
 * Preconditions: call serial_init() once before any put. serial_puts()
 * requires a valid NUL-terminated string (NULL is ignored, not crashed
 * on). Failure modes: on absent hardware, init writes go nowhere and
 * serial_ok() returns 0; TX polls may then spin forever.
 */

#ifndef NADIR_SERIAL_H
#define NADIR_SERIAL_H

/* Program COM1 for 38400 8N1 polling (interrupts off, FIFO on).
 * Ring 0 only. Safe to call again to re-program the port. */
void serial_init(void);

/* Block until the THR is empty (LSR bit 5), then emit one byte. */
void serial_putchar(char c);

/* Emit a NUL-terminated string via serial_putchar(). NULL is a no-op. */
void serial_puts(const char *s);

/* Nonzero if a UART looks present (LSR reads back sane, not 0xFF
 * as an open bus returns). Zero means "do not trust TX polls". */
int serial_ok(void);

#endif /* NADIR_SERIAL_H */
