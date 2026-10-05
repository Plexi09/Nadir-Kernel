/* Nadir boot logger: timestamped `[    0.000000] sub: msg` lines.
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) on the VGA console via
 * console_putchar, which is polling-only and IRQ-safe by construction.
 *
 * Timestamp source: pit_ticks() / PIT_HZ (100Hz, 10ms per tick), integer
 * math only. At 100Hz one
 * tick is 10ms, so the 6-digit fraction only changes in steps of 10000
 * (e.g. 0.010000); the last 4 digits are always zero by construction.
 * Early boot note: IF=0 until kmain runs `sti`, so the PIT IRQ cannot fire
 * and ticks stay frozen at 0.
 *
 * IF/IRQ safety: mainline only, never IRQ context. The PIT IRQ handler
 * (pit_on_tick) must never call this. It only bumps the tick counter.
 * Single-core kernel, so no locking is needed.
 *
 * Preconditions: console is usable (identity-mapped VGA); pit_ticks() is
 * linked in (BSS-zeroed counter reads 0 before pit_init).
 * Failure modes: none
 */

#ifndef NADIR_KLOG_H
#define NADIR_KLOG_H

#include <stdint.h>

/* Print one full line: `[    0.000000] subsystem: msg` + '\n'. */
void klog(const char *subsystem, const char *msg);

/* Piecewise line builder for values (PMM stats, uptime): prints the
 * `[    0.000000] subsystem: ` prefix with no newline; append with
 * klog_str/dec/hex64, then close with klog_end() ('\n').
 * Preconditions: calls nest as begin -> (str|dec|hex64)* -> end. */
void klog_begin(const char *subsystem);

/* Append a NUL-terminated string to the open line. */
void klog_str(const char *s);

/* Append an unsigned decimal value (no libc printf). */
void klog_dec(uint64_t v);

/* Append a 64-bit value as 0x-prefixed, zero-padded hexadecimal. */
void klog_hex64(uint64_t v);

/* Terminate the open line with '\n'. */
void klog_end(void);

#endif /* NADIR_KLOG_H */
