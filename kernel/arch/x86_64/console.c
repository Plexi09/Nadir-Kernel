/* VGA text-mode driver (see console.h). */

#include <stdint.h>

#include "console.h"
#include "io.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDR 0xB8000
#define COLOR_DEFAULT 0x0FU /* white on black */

/* VGA CRT controller ports: index selects the register, data reads/writes
 * it. Registers 0x0E/0x0F hold the high/low byte of the cursor offset. */
#define VGA_CRT_INDEX 0x3D4U
#define VGA_CRT_DATA 0x3D5U
#define VGA_CRT_CURSOR_HIGH 0x0EU
#define VGA_CRT_CURSOR_LOW 0x0FU

/* Cursor position. Lives in BSS. */
static uint8_t cursor_row;
static uint8_t cursor_col;

/* Text buffer, one 16-bit cell per character, low byte is the ASCII code,
 * high byte is the color attribute. `volatile` so the compiler keeps every
 * store in order. */
static volatile uint16_t *const vga = (uint16_t *)VGA_ADDR;

/* Sync the blinking hardware cursor to cursor_row/cursor_col.
 * WHY it exists: writing VGA cells alone never moves the CRT cursor, so
 * without this the hardware cursor stays where SeaBIOS left it (issue #3).
 * Runs at Ring 0 (CPL 0): `out` traps with #GP(0) if CPL > IOPL, so this
 * must never run in userspace. A wrong port value cannot fault (open bus
 * ignores it); it would only misplace the visible cursor. */
static void update_hw_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_row * VGA_WIDTH + cursor_col);

    outb(VGA_CRT_INDEX, VGA_CRT_CURSOR_LOW);
    outb(VGA_CRT_DATA, (uint8_t)(pos & 0xFFU));
    outb(VGA_CRT_INDEX, VGA_CRT_CURSOR_HIGH);
    outb(VGA_CRT_DATA, (uint8_t)((pos >> 8) & 0xFFU));
}

static void scroll(void)
{
    for (uint8_t row = 1; row < VGA_HEIGHT; row++) {
        for (uint8_t col = 0; col < VGA_WIDTH; col++) {
            vga[(row - 1) * VGA_WIDTH + col] = vga[row * VGA_WIDTH + col];
        }
    }
    for (uint8_t col = 0; col < VGA_WIDTH; col++) {
        vga[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
            (uint16_t)((COLOR_DEFAULT << 8) | ' ');
    }
}

void console_clear(void)
{
    for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (uint16_t)((COLOR_DEFAULT << 8) | ' ');
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hw_cursor();
}

void console_putchar(char c)
{
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\b') {
        /* kmain echoes keyboard '\b' for editing. Without this the
         * raw 0x08 glyph would print instead of erasing. Move back one
         * cell and blank it (IRQ-safe: mainline only, IF=1 but the
         * IRQ handlers never touch VGA). */
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_row > 0) {
            cursor_row--;
            cursor_col = VGA_WIDTH - 1;
        } else {
            return;
        }
        vga[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)((COLOR_DEFAULT << 8) | ' ');
    } else {
        vga[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)((COLOR_DEFAULT << 8) | c);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            /* Wrap to the next line. */
            cursor_col = 0;
            cursor_row++;
        }
    }
    /* Past the bottom line scroll and stay on the last line. */
    if (cursor_row >= VGA_HEIGHT) {
        scroll();
        cursor_row = VGA_HEIGHT - 1;
    }
    update_hw_cursor();
}

void console_puts(const char *s)
{
    while (*s != '\0') {
        console_putchar(*s);
        s++;
    }
}

void console_puthex64(uint64_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    console_puts("0x");
    /* Print 16 nibbles, most significant first (shifts 60, 56, …, 0). */
    for (int shift = 60; shift >= 0; shift -= 4) {
        console_putchar(digits[(value >> shift) & 0xFU]);
    }
}
