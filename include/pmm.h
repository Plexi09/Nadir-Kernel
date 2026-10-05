/* Nadir physical memory manager
 *
 * Runs in 64-bit long mode at Ring 0 (CPL 0) under the stage-2 identity
 * map; physical addresses are used as-is. Called once from kmain with
 * interrupts disabled (IF=0), before sti, so no IRQ handler can allocate
 * concurrently. No faults are possible beyond a programming bug; with no
 * IDT yet at the call point a wild access would triple-fault.
 */

#ifndef NADIR_PMM_H
#define NADIR_PMM_H

#include <stdint.h>

#include "e820.h"

/* 4KB frames over the identity-mapped first 1GB: hard capacity limit of
 * this allocator (256K frames, 32KB bitmap in .bss). Regions above 1GB
 * reported by E820 are ignored. */
#define PMM_FRAME_SIZE 4096U
#define PMM_MAX_BYTES 0x40000000ULL
#define PMM_MAX_FRAMES (PMM_MAX_BYTES / PMM_FRAME_SIZE)

/* Memory-map globals: filled by the stage-2 entry (copy from the
 * stage-1 staging buffer) before kmain runs. pmm_init reads them. */
extern uint32_t memmap_count;
extern struct e820_entry memmap_entries[E820_MAX];

/* Build the bitmap from the E820 map: type-1 (usable) frames start free,
 * everything else starts reserved, then the kernel image, page tables,
 * staging buffer, low 1MB, stack and VGA regions are reserved.
 * Preconditions: count <= E820_MAX, entries points at count valid
 * entries, called once with IF=0 before any alloc/free.
 * Failure modes: none — an empty/corrupt map just leaves 0 free frames. */
void pmm_init(uint32_t count, const struct e820_entry *entries);

/* Hand out one 4KB frame: returns its physical base address, or 0 when
 * out of memory (frame 0 is always reserved, so 0 is never valid).
 * Preconditions: pmm_init ran. Failure modes: returns 0 on OOM. */
uint64_t pmm_alloc_frame(void);

/* Return a frame from pmm_alloc_frame to the free pool. Silently ignores
 * unaligned, out-of-range, or already-free addresses.
 * Preconditions: pmm_init ran. Failure modes: none (no fault, no panic). */
void pmm_free_frame(uint64_t addr);

/* Live counters for stats printouts.
 * Preconditions: pmm_init ran (0 before that). Failure modes: none. */
uint64_t pmm_total_count(void);
uint64_t pmm_free_count(void);
uint64_t pmm_usable_count(void);

#endif /* NADIR_PMM_H */
