/* Nadir kernel heap allocator
 *
 * A page-granular heap on top of the PMM. Runs in 64-bit long
 * mode at Ring 0 (CPL 0) under the stage-2 identity map; every address
 * handed out is a physical address used as-is. There is no virtual
 * address space yet, so kmalloc returns identity-mapped physical
 * addresses.
 *
 * Design: allocations are rounded up to whole 4KB frames. The first
 * bytes of the first frame hold a header (magic + frame count) so
 * kfree can validate and release the whole run. Free runs are kept on
 * a singly-linked free list; kmalloc takes the first run that fits
 * (first-fit) and splits the tail. No coalescing yet — adjacent free
 * runs stay separate until a later change merges them.
 *
 * IF/IRQ safety: not interrupt-safe. Callers must not kmalloc/kfree
 * from an IRQ handler; the PIT/keyboard handlers never allocate.
 * Single-core kernel, so no locking is needed.
 *
 * Preconditions: pmm_init ran (kmalloc calls pmm_alloc_frame).
 * Failure modes: kmalloc returns 0 (NULL) on out-of-memory or when
 * the request rounds up to zero frames; kfree silently ignores NULL,
 * bad magic, and out-of-window addresses.
 */

#ifndef NADIR_KMALLOC_H
#define NADIR_KMALLOC_H

#include <stddef.h>
#include <stdint.h>

/* Allocate `size` bytes (size 0 behaves like size 1). Returns an
 * identity-mapped physical address, or NULL when the PMM is out of
 * frames. The returned block is NOT zeroed.
 * Preconditions: pmm_init ran. Failure modes: returns NULL on OOM. */
void *kmalloc(size_t size);

/* Release a block from kmalloc. Silently ignores NULL, pointers with
 * a bad header magic, and addresses outside the PMM window.
 * Preconditions: pmm_init ran. Failure modes: none (no fault, no
 * panic on bad input). */
void kfree(void *ptr);

/* Live counters for stats printouts and the boot self-test.
 * Preconditions: kmalloc_init ran (0 before that). Failure modes: none. */
uint64_t kmalloc_free_frames(void);
uint64_t kmalloc_used_frames(void);

/* One-time heap init: takes the first PMM frames as the free pool.
 * Called once from kmain after pmm_init, with IF=0.
 * Preconditions: pmm_init ran. Failure modes: none — with zero frames
 * handed over, kmalloc always returns NULL. */
void kmalloc_init(void);

#endif /* NADIR_KMALLOC_H */
