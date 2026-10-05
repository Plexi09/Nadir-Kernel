/* Nadir BIOS E820 memory-map entry (int 0x15, eax=0xE820).
 *
 * Staged by the stage-1 boot sector in 16-bit real mode at Ring 0 (CPL 0)
 * into low memory (0x5000 count, 0x5020 entries) while BIOS calls still
 * work, then copied by the stage-2 entry into the .bss globals
 * memmap_count / memmap_entries owned by the PMM. Runs under the
 * identity-mapped first 1GB, so physical == virtual addresses here.
 * Fault behavior: no IDT exists during staging/copy, so a bad pointer
 * triple-faults; the entry count is clamped to E820_MAX on both ends.
 */

#ifndef NADIR_E820_H
#define NADIR_E820_H

#include <stdint.h>

/* Max entries the boot sector stages and the kernel keeps. */
#define E820_MAX 64

/* Usable-RAM type in the E820 type field. */
#define E820_USABLE 1

/* One 24-byte BIOS E820 entry: byte-based [base, base+length) range.
 * Preconditions: filled by BIOS int 0x15 or by the boot-sector fallback
 * (single type-1 region at 1MB); length may be 0 (ignored by the PMM).
 * Failure modes: none in C — a corrupt table only mis-marks frames. */
struct e820_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t acpi_attr;
} __attribute__((packed));

#endif /* NADIR_E820_H */
