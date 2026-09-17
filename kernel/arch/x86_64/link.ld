/* Nadir kernel linker script.
 *
 * Links the whole kernel (stage-2 entry + C code) as one flat binary loaded
 * by the boot sector at 0x10000. Virtual addresses equal physical ones: the
 * stage-2 page tables identity-map the first 1GB, so linked addresses work
 * as-is the moment long mode is on.
 *
 * Layout rules: the stage-2 entry (.boot32) must be the very first bytes of
 * the image, BSS exposes __bss_start/__bss_end for runtime zeroing, and
 * toolchain metadata sections are discarded so the binary is pure code+data.
 */

ENTRY(boot32_entry)
OUTPUT_ARCH(i386:x86-64)

SECTIONS
{
    . = 0x10000;

    .boot : {
        *(.boot32)
    }

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(COMMON)
        __bss_end = .;
    }

    /DISCARD/ : {
        *(.note*)
        *(.comment*)
        *(.eh_frame*)
    }
}
