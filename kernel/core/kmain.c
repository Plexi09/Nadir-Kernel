/* Nadir kernel entry point: first C code to run.
 *
 * Called once from the stage-2 entry stub after the CPU is in 64-bit long
 * mode with a valid stack, zeroed BSS and identity-mapped memory. Runs at
 * Ring 0 (CPL 0) with IF=0 on entry under the stage-2 GDT (CS 0x18);
 * `-mno-red-zone` keeps IRQ pushes off the red zone so interrupt frames
 * are safe. Installs the exception IDT first so from then on a fault
 * prints diagnostics instead of triple-faulting. `sti` runs exactly once
 * after every device is programmed; the main loop keeps IF=1 and sleeps
 * in `hlt` (which wakes on each IRQ) — never `cli`-parks, or timer ticks
 * and keystrokes would stop. It never returns; on return the caller halts.
 */

#include <stdint.h>

#include "console.h"
#include "idt.h"
#include "keyboard.h"
#include "pic.h"
#include "pit.h"
#include "serial.h"

/* A value that only comes out right if the CPU truly executes 64-bit
 * arithmetic: every nibble survives the shifts above 32 bits. */
#define PROOF_VALUE 0x0123456789ABCDEFULL

/* Print an unsigned decimal via console_putchar (no libc printf). */
static void print_dec(uint64_t v)
{
    char buf[20];
    int n = 0;

    if (v == 0) {
        console_putchar('0');
        return;
    }
    while (v != 0 && n < (int)sizeof(buf)) {
        buf[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    }
    while (n > 0) {
        n--;
        console_putchar(buf[n]);
    }
}

/* Mirror one echoed keystroke to COM1 when the UART looks present. */
static void serial_echo(char c, int ok)
{
    char tmp[2];

    if (ok == 0) {
        return;
    }
    if (c == '\b') {
        /* Erase on a serial terminal: back, blank, back. */
        serial_puts("\b \b");
    } else if (c == '\n') {
        serial_puts("\r\n");
    } else {
        tmp[0] = c;
        tmp[1] = '\0';
        serial_puts(tmp);
    }
}

void kmain(void)
{
    int serial_present;
    uint64_t last_uptime;

    console_clear();
    console_puts("Hello World from Nadir Kernel !\n");
    console_puts("Running C code in 64-bit long mode (Ring 0).\n");

    /* WHY this order: pic_remap must precede idt_install_irqs so IRQ
     * gates land on valid remapped vectors 32-47 (pre-remap they would
     * overlap CPU exceptions 0-15 and a tick would look like a fault);
     * devices (serial/PIT/keyboard) are programmed before sti so no IRQ
     * fires into a half-wired handler; pic_remap masks everything and
     * each device opts in via pic_unmask, so stray lines stay silent. */
    idt_init();
    console_puts("IDT installed (vectors 0-31).\n");

    pic_remap();
    console_puts("PIC remapped (IRQ 0-15 -> vectors 32-47, masked).\n");

    idt_install_irqs();
    console_puts("IRQ gates installed (vectors 32-47).\n");

    serial_init();
    serial_present = serial_ok();
    console_puts("Serial COM1 38400 8N1 ready.\n");
    if (serial_present != 0) {
        serial_puts("Hello World from Nadir Kernel !\n");
        serial_puts("Serial COM1 38400 8N1 ready.\n");
    }

    pit_init();
    console_puts("PIT 100Hz on IRQ0 ready.\n");
    if (serial_present != 0) {
        serial_puts("PIT 100Hz on IRQ0 ready.\n");
    }

    keyboard_init();
    console_puts("Keyboard PS/2 IRQ1 ready.\n");
    if (serial_present != 0) {
        serial_puts("Keyboard PS/2 IRQ1 ready.\n");
    }

    /* Explicit opt-in for the two live lines; idempotent even though
     * keyboard_init already unmasked IRQ1. */
    pic_unmask(0);
    pic_unmask(1);

    __asm__ volatile("sti");
    console_puts("Interrupts enabled (IRQs 0-1).\n");
    if (serial_present != 0) {
        serial_puts("Interrupts enabled (IRQs 0-1).\n");
    }

    console_puts("64-bit proof value: ");
    console_puthex64(PROOF_VALUE);
    console_puts("\nHalting in hlt loop (timer + keyboard live).\n");

    /* Main loop: hlt sleeps until the next IRQ (IF=1 throughout), then
     * one poll pass prints uptime ~1/s and drains echoed keys. */
    last_uptime = 0;
    for (;;) {
        uint64_t now;

        __asm__ volatile("hlt");
        now = pit_ticks();
        if ((now - last_uptime) >= 100U) {
            uint64_t secs = now / 100U;
            uint64_t tenths = (now / 10U) % 10U;

            last_uptime = now;
            console_puts("uptime ");
            print_dec(secs);
            console_puts(".");
            print_dec(tenths);
            console_puts("s (ticks ");
            console_puthex64(now);
            console_puts(")\n");
            if (serial_present != 0) {
                serial_puts("uptime (see VGA for value)\n");
            }
        }
        while (keyboard_has_key() != 0) {
            char c = keyboard_dequeue();

            /* console_putchar erases on '\b' and wraps on '\n'. */
            console_putchar(c);
            serial_echo(c, serial_present);
        }
    }
}
