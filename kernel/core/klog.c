/* Nadir boot logger (see klog.h).
 *
 * The only backends are console_putchar (VGA polling)
 * and pit_ticks (monotonic u64) — no hardware addresses, no port I/O, no
 * serial dependency live here. Serial mirroring stays in kmain, which
 * prints the same text without the timestamp prefix (kept simple).
 *
 * Timestamp math: secs = ticks / PIT_HZ, frac = (ticks % PIT_HZ) * 10000
 * (1000000us/s divided by 100Hz). The field is 12 wide, space-padded on
 * the left (`[    0.000000]`); past 99999s it overflows the
 * pad and just prints long.
 */

#include <stdint.h>

#include "console.h"
#include "klog.h"
#include "pit.h"

/* Width of the `secs.frac` field between `[` and `]`. */
#define KLOG_TIME_WIDTH 12U

/* Print the `[    0.000000] subsystem: ` prefix. Reads pit_ticks() live
 * so piecewise lines and one-shot lines share one code path. */
static void klog_prefix(const char *subsystem)
{
    uint64_t ticks = pit_ticks();
    uint64_t secs = ticks / (uint64_t)PIT_HZ;
    uint64_t frac = (ticks % (uint64_t)PIT_HZ) * 10000ULL;
    char num[22];
    int n = 0;
    uint64_t v;
    uint64_t div;
    int digits;
    uint64_t len;

    /* Decimal seconds into num (reversed then flipped below). */
    v = secs;
    if (v == 0U) {
        num[n++] = '0';
    } else {
        while (v != 0U && n < 20) {
            num[n++] = (char)('0' + (v % 10U));
            v /= 10U;
        }
    }
    /* Field length is digits + '.' + 6 frac digits; pad left with spaces
     * to KLOG_TIME_WIDTH so short uptimes align */
    digits = n;
    len = (uint64_t)digits + 7U;
    console_putchar('[');
    while (len < (uint64_t)KLOG_TIME_WIDTH) {
        console_putchar(' ');
        len++;
    }
    while (n > 0) {
        n--;
        console_putchar(num[n]);
    }
    console_putchar('.');
    /* Exactly 6 fraction digits, zero-padded (10ms ticks leave the last
     * 4 as zeros — resolution limit, not a bug). */
    div = 100000ULL;
    while (div > 0U) {
        console_putchar((char)('0' + ((frac / div) % 10U)));
        div /= 10U;
    }
    console_puts("] ");
    console_puts(subsystem);
    console_puts(": ");
}

void klog(const char *subsystem, const char *msg)
{
    klog_begin(subsystem);
    klog_str(msg);
    klog_end();
}

void klog_begin(const char *subsystem)
{
    klog_prefix(subsystem);
}

void klog_str(const char *s)
{
    console_puts(s);
}

void klog_dec(uint64_t v)
{
    char buf[20];
    int n = 0;

    if (v == 0U) {
        console_putchar('0');
        return;
    }
    while (v != 0U && n < (int)sizeof(buf)) {
        buf[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    }
    while (n > 0) {
        n--;
        console_putchar(buf[n]);
    }
}

void klog_hex64(uint64_t v)
{
    console_puthex64(v);
}

void klog_end(void)
{
    console_putchar('\n');
}
