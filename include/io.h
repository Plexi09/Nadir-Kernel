/* Nadir x86 port I/O primitives (inb/outb/io_wait).
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) with the stage-2 GDT.
 * Privileged instructions: `in`/`out` trap with #GP(0) when CPL > IOPL,
 * so these helpers are Ring 0 only and must never be exposed to
 * userspace. Fault behavior: a bad port number cannot fault by itself
 * (unused ports read back 0xFF or are ignored); a fault here means the
 * caller programmed the wrong device, not a CPU trap.
 * Interrupts stay disabled by the caller where timing matters; these
 * helpers do not touch IF themselves.
 */

#ifndef NADIR_IO_H
#define NADIR_IO_H

#include <stdint.h>

/* Read one byte from ISA port `port`.
 * Preconditions: CPL 0, port is a valid x86 I/O address (0-0xFFFF).
 * Failure modes: none (returns bus value; 0xFF on unmapped ports). */
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;

    __asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/* Write byte `value` to ISA port `port`.
 * Preconditions: CPL 0, port is writable for this device.
 * Failure modes: none from the CPU; wrong port = device misbehavior. */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile("outb %0, %1" ::"a"(value), "Nd"(port));
}

/* Tiny delay for legacy ISA devices (writes to unused port 0x80).
 *
 * WHY: the 8259 PIC and other slow devices need ~1us between back to
 * back command bytes; port 0x80 is the conventional throwaway delay.
 * Preconditions: CPL 0. Failure modes: none. */
static inline void io_wait(void)
{
    outb(0x80U, 0U);
}

#endif /* NADIR_IO_H */
