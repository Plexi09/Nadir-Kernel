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
 *
 * Every line goes through klog`[    0.000000] subsystem: message`, 
 * time from pit_ticks()/PIT_HZ).
 * Serial COM1 mirrors the same text without the timestamp prefix.
 */

#include <stdint.h>

#include "console.h"
#include "idt.h"
#include "keyboard.h"
#include "klog.h"
#include "pic.h"
#include "pit.h"
#include "pmm.h"
#include "serial.h"

/* A value that only comes out right if the CPU truly executes 64-bit
 * arithmetic: every nibble survives the shifts above 32 bits. */
#define PROOF_VALUE 0x0123456789ABCDEFULL

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
    /* Boot banner: keeps the "64-bit" needle the CI boot checker greps
     * for (see .github/scripts/check_boot.py). */
    klog("nadir", "Hello World from Nadir kernel (64-bit long mode, Ring 0)");

    /* WHY this order: pic_remap must precede idt_install_irqs so IRQ
     * gates land on valid remapped vectors 32-47 (pre-remap they would
     * overlap CPU exceptions 0-15 and a tick would look like a fault);
     * devices (serial/PIT/keyboard) are programmed before sti so no IRQ
     * fires into a half-wired handler; pic_remap masks everything and
     * each device opts in via pic_unmask, so stray lines stay silent. */
    idt_init();
    klog("idt", "exceptions 0-31 installed");

    pic_remap();
    klog("pic", "remapped IRQ 0-15 to vectors 32-47");

    idt_install_irqs();
    klog("idt", "IRQ gates 32-47 installed");

    serial_init();
    serial_present = serial_ok();
    klog("serial", "COM1 38400 8N1 ready");
    if (serial_present != 0) {
        serial_puts("Hello World from Nadir kernel\n");
    }

    pit_init();
    klog("pit", "100Hz on IRQ0");
    if (serial_present != 0) {
        serial_puts("pit: 100Hz on IRQ0\n");
    }

    keyboard_init();
    klog("keyboard", "PS/2 ready on IRQ1");
    if (serial_present != 0) {
        serial_puts("keyboard: PS/2 ready on IRQ1\n");
    }

    /* pmm_init only reads the staged E820 globals and writes
     * its own .bss bitmap, so it runs
     * after all devices are programmed but before sti, with IF=0, so no
     * handler can observe a half-built allocator. Stats go through the
     * polling VGA console, which is IRQ-safe by construction. */
    pmm_init(memmap_count, memmap_entries);
    klog_begin("pmm");
    klog_str("E820 entries ");
    klog_dec(memmap_count);
    klog_str(", usable frames ");
    klog_dec(pmm_usable_count());
    klog_str(" / total frames ");
    klog_dec(pmm_total_count());
    klog_str(", free frames ");
    klog_dec(pmm_free_count());
    klog_str(", reserved ");
    klog_dec(pmm_usable_count() - pmm_free_count());
    klog_end();

    /* Explicit opt-in for the two live lines; idempotent even though
     * keyboard_init already unmasked IRQ1. */
    pic_unmask(0);
    pic_unmask(1);

    __asm__ volatile("sti");
    klog("nadir", "interrupts enabled (IRQs 0-1)");
    if (serial_present != 0) {
        serial_puts("nadir: interrupts enabled (IRQs 0-1)\n");
    }

    klog_begin("nadir");
    klog_str("64-bit proof value: ");
    klog_hex64(PROOF_VALUE);
    klog_end();
    klog("nadir", "halting in hlt loop (timer + keyboard live)");

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
            klog_begin("nadir");
            klog_str("uptime ");
            klog_dec(secs);
            klog_str(".");
            klog_dec(tenths);
            klog_str("s (ticks ");
            klog_hex64(now);
            klog_str(")");
            klog_end();
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
