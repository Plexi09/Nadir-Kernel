/* COM1 16550 UART polling log (see serial.h). */

#include <stdint.h>

#include "io.h"
#include "serial.h"

/* Register offsets from the COM1 base. */
#define COM1_BASE 0x3F8U
#define COM_DATA 0x3F8U /* THR/RBR (DLAB=0) / divisor low (DLAB=1) */
#define COM_IER 0x3F9U /* interrupt enable / divisor high (DLAB=1) */
#define COM_FIFO 0x3FAU /* FIFO control */
#define COM_LCTL 0x3FBU /* line control + DLAB bit 7 */
#define COM_MCTL 0x3FCU /* modem control */
#define COM_LSR 0x3FDU /* line status: bit 5 = THR empty, bit 0 = RX ready */

#define LSR_TX_EMPTY 0x20U /* THR holds no byte: safe to write */
#define LSR_RX_READY 0x01U /* RBR holds a byte (reserved for future RX) */

/* Port I/O owned by include/io.h; no local inb/outb duplicates. */

void serial_init(void)
{
    /* No UART interrupts: pure polling, so the 8259/IDT stays untouched. */
    outb(COM_IER, 0x00);
    /* DLAB on to program the divisor. */
    outb(COM_LCTL, 0x80);
    /* Divisor 3 = 115200/3 = 38400 baud. */
    outb(COM_DATA, 0x03);
    outb(COM_IER, 0x00);
    /* 8N1: 8 bits, no parity, one stop bit; DLAB back off. */
    outb(COM_LCTL, 0x03);
    /* FIFO on, clear both queues, 14-byte threshold. */
    outb(COM_FIFO, 0xC7);
    /* DTR+RTS on, OUT2 on so the UART drives interrupts (unused). */
    outb(COM_MCTL, 0x0B);
}

void serial_putchar(char c)
{
    /* Spin until THR is empty; hangs only if hardware is absent/wedged,
     * which is why callers gate on serial_ok() when unsure. */
    while ((inb(COM_LSR) & LSR_TX_EMPTY) == 0) {
    }
    outb(COM_DATA, (uint8_t)c);
}

void serial_puts(const char *s)
{
    if (s == 0) {
        return;
    }
    while (*s != '\0') {
        serial_putchar(*s);
        s++;
    }
}

int serial_ok(void)
{
    /* An absent port reads back 0xFF (open bus); a real 16550 never
     * reports all status bits set at once, so that means no UART. */
    return inb(COM_LSR) != 0xFFU;
}
