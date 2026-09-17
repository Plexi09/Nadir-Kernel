/* Nadir console: VGA text-mode driver (see console.h). */

#include <stdint.h>

#include "console.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ADDR 0xB8000
#define COLOR_DEFAULT 0x0FU /* white on black */

/* Cursor position. Lives in BSS, zeroed by stage 2 before kmain runs. */
static uint8_t cursor_row;
static uint8_t cursor_col;

/* Text buffer: one 16-bit cell per character, low byte is the ASCII code,
 * high byte is the color attribute. `volatile` so the compiler keeps every
 * store in order: this is memory-mapped I/O, and dropped or reordered
 * writes would corrupt the screen. */
static volatile uint16_t *const vga = (uint16_t *)VGA_ADDR;

/* Move every line one row up and blank the last line */
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
}

void console_putchar(char c)
{
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else {
        vga[cursor_row * VGA_WIDTH + cursor_col] =
            (uint16_t)((COLOR_DEFAULT << 8) | c);
        cursor_col++;
        if (cursor_col >= VGA_WIDTH) {
            /* Past the right edge: wrap to the next line. */
            cursor_col = 0;
            cursor_row++;
        }
    }
    /* Past the bottom line: scroll and stay on the last line. */
    if (cursor_row >= VGA_HEIGHT) {
        scroll();
        cursor_row = VGA_HEIGHT - 1;
    }
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
