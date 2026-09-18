/* Nadir kernel entry point: first C code to run.
 *
 * Called once from the stage-2 entry stub after the CPU is in 64-bit long
 * mode with a valid stack, zeroed BSS and identity-mapped memory. Runs at
 * Ring 0 (CPL 0) interrupts disabled. Installs the exception IDT first
 * so from then on a fault prints diagnostics instead of triple-faulting.
 * It never returns; on return the caller halts the CPU.
 */

#include "console.h"
#include "idt.h"

/* A value that only comes out right if the CPU truly executes 64-bit
 * arithmetic: every nibble survives the shifts above 32 bits. */
#define PROOF_VALUE 0x0123456789ABCDEFULL

void kmain(void)
{
    console_clear();
    console_puts("Hello World from Nadir Kernel !\n");
    console_puts("Running C code in 64-bit long mode (Ring 0).\n");
    idt_init();
    console_puts("IDT installed (vectors 0-31).\n");
    console_puts("64-bit proof value: ");
    console_puthex64(PROOF_VALUE);
    console_puts("\nHalting.\n");

    for (;;) {
        __asm__ volatile("hlt");
    }
}
