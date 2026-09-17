/* Nadir early console: VGA text-mode driver.
 *
 * Writes straight to the VGA text buffer at 0xB8000, which the stage-2
 * page tables identity-map. Runs in 64-bit long mode at Ring 0 (CPL 0);
 * no faults are possible beyond a programming bug. There is no IDT yet,
 * so a wild pointer here triple-faults the machine.
 */

#ifndef NADIR_CONSOLE_H
#define NADIR_CONSOLE_H

#include <stdint.h>

/* Clear the screen and park the cursor at the top left. */
void console_clear(void);

/* Print one character. '\n' moves to the start of the next line; the
 * screen scrolls up when the cursor runs past the last line. */
void console_putchar(char c);

/* Print a NUL-terminated string. */
void console_puts(const char *s);

/* Print a 64-bit value as 0x-prefixed, zero-padded hexadecimal. */
void console_puthex64(uint64_t value);

#endif /* NADIR_CONSOLE_H */
