/* PS/2 scancode set-1 IRQ1 driver (see keyboard.h).
 *
 * IRQ wiring uses the live framework: keyboard_init() registers
 * keyboard_on_irq() via irq_register_handler() (idt.h) and unmasks
 * line 1 via pic_unmask() (pic.h), both feature-detected with
 * __has_include so this file still compiles if either header is
 * absent (registration then falls back to caller duty). The driver
 * itself never executes `sti`: IF stays cleared and IRQs only fire
 * after the integrator remaps the PIC, installs the IRQ gates, and
 * enables interrupts. 8259 IRQ1 -> vector 33.
 */

#include <stdint.h>

#include "keyboard.h"

/* Port I/O owned by include/io.h; no local inb duplicate. */
#include "io.h"

#if defined(__has_include)
#if __has_include("idt.h")
#include "idt.h"
#define NADIR_HAS_IRQ_REG 1
#endif
#if __has_include("pic.h")
#include "pic.h"
#define NADIR_HAS_PIC 1
#endif
#endif

#define KBD_DATA 0x60U /* PS/2 data port: one scancode per IRQ1 */
#define KBD_BUF_SIZE 128U /* power of two so wrap is cheap */

#define SC_RELEASE 0x80U /* high bit set = key release, ignore */
#define SC_EXTENDED 0xE0U /* prefix for arrows/extended keys, ignore */
#define SC_LSHIFT 0x2AU
#define SC_RSHIFT 0x36U
#define SC_CAPS 0x3AU

/* US set-1 codes without Shift (0 = no printable mapping). Backspace
 * 0x0E stays '\b' and Enter 0x1C stays '\n' so editing works raw. */
static const char k_unshifted[128] = {
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\n',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x39] = ' ',
};

/* Same codes with Shift held (letters stay lowercase here; the Shift
 * + CapsLock XOR in keyboard_on_irq() uppercases them instead). */
static const char k_shifted[128] = {
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\n',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M', [0x33] = '<',
    [0x34] = '>', [0x35] = '?', [0x39] = ' ',
};

/* Single-producer (IRQ) / single-consumer (poll) ring. `volatile` stops
 * the compiler caching head/tail across the IRQ boundary; on one core
 * with byte-sized index traffic no lock is needed because the producer
 * only writes head (+buf slot) and the consumer only writes tail. */
static char k_buf[KBD_BUF_SIZE];
static volatile uint8_t k_head;
static volatile uint8_t k_tail;
/* IRQ-only state: touched solely in keyboard_on_irq(), so plain. */
static uint8_t k_shift;
static uint8_t k_caps;
static uint8_t k_e0;

/* Drop-newest on full: keeps the consumer's prefix intact for editing. */
static void enqueue(char c)
{
    uint8_t next = (uint8_t)((k_head + 1) % KBD_BUF_SIZE);
    if (next == k_tail) {
        return;
    }
    k_buf[k_head] = c;
    k_head = next;
}

void keyboard_init(void)
{
    k_head = 0;
    k_tail = 0;
    k_shift = 0;
    k_caps = 0;
    k_e0 = 0;
#ifdef NADIR_HAS_IRQ_REG
    /* Route vector 33 -> keyboard_on_irq(); without idt.h this stays
     * caller duty (manual irq_register_handler call). */
    irq_register_handler(1, keyboard_on_irq);
#endif
#ifdef NADIR_HAS_PIC
    pic_unmask(1);
#endif
}

void keyboard_on_irq(void)
{
    uint8_t code = inb(KBD_DATA);

    if (code == SC_EXTENDED) {
        k_e0 = 1;
        return;
    }
    if (k_e0 != 0) {
        /* Extended make/break: swallow, maps to 0. */
        k_e0 = 0;
        return;
    }
    /* only Shift release changes state the rest is ignored. */
    if ((code & SC_RELEASE) != 0) {
        uint8_t make = (uint8_t)(code & (uint8_t)~SC_RELEASE);
        if (make == SC_LSHIFT || make == SC_RSHIFT) {
            k_shift = 0;
        }
        return;
    }
    if (code == SC_LSHIFT || code == SC_RSHIFT) {
        k_shift = 1;
        return;
    }
    /* CapsLock toggles on press only */
    if (code == SC_CAPS) {
        k_caps = (uint8_t)(k_caps == 0 ? 1 : 0);
        return;
    }
    if (code >= sizeof(k_unshifted)) {
        return;
    }
    char base = k_unshifted[code];
    if (base == '\0') {
        return;
    }
    char out = base;
    if (base >= 'a' && base <= 'z') {
        /* Shift XOR CapsLock uppercases letters. */
        if ((k_shift != 0) != (k_caps != 0)) {
            out = (char)(base - 'a' + 'A');
        }
    } else if (k_shift != 0) {
        char shifted = k_shifted[code];
        if (shifted != '\0') {
            out = shifted;
        }
    } else if (k_caps != 0) {
        /* CapsLock alone must not shift digits/symbols */
    }
    enqueue(out);
}

int keyboard_has_key(void)
{
    return k_head != k_tail;
}

char keyboard_dequeue(void)
{
    if (k_head == k_tail) {
        return 0;
    }
    char c = k_buf[k_tail];
    k_tail = (uint8_t)((k_tail + 1) % KBD_BUF_SIZE);
    return c;
}
