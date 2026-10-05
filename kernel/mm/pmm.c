/* Nadir physical memory manager (PMM-only)
 *
 * very address it touches arrives as a plain
 * physical range (E820 entries from the caller, reservation bounds from
 * the linker/boot chain). Runs in 64-bit long mode at Ring 0 (CPL 0)
 * under the stage-2 identity map, so physical addresses are dereferenced
 * as-is. pmm_init runs once with IF=0 before sti; alloc/free never block
 * and never touch hardware, so they are safe under any IF state.
 * Fault behavior: no page fault is possible inside the first 1GB
 * identity window; a caller passing a wild E820 pointer faults before we
 * run, not in here. Bitmap: bit set = used/reserved, bit clear = free.
 */

#include <stdint.h>

#include "e820.h"
#include "pmm.h"

/* Filled by the stage-2 entry (copy from the stage-1 staging buffer at
 * 0x5000/0x5020) after BSS zeroing, before kmain runs. */
uint32_t memmap_count;
struct e820_entry memmap_entries[E820_MAX];

/* Kernel image bounds from the linker script (linked at 0x10000). */
extern char __kernel_start[];
extern char __kernel_end[];

/* One bit per 4KB frame over the first 1GB: 256K frames = 32KB in .bss. */
static uint8_t bitmap[PMM_MAX_FRAMES / 8U];

static uint64_t g_usable;
static uint64_t g_free;

/* WHY bit ops instead of a byte-per-frame array: 32KB of .bss keeps the
 * kernel image small (sectors are loaded from floppy LBA 1..N, so every
 * KB costs boot time); the shift/mask cost is negligible next to a VGA
 * print. */
static void set_bit(uint64_t frame)
{
    bitmap[frame / 8U] |= (uint8_t)(1U << (frame % 8U));
}

static void clear_bit(uint64_t frame)
{
    bitmap[frame / 8U] &= (uint8_t)~(1U << (frame % 8U));
}

static int test_bit(uint64_t frame)
{
    return (bitmap[frame / 8U] & (uint8_t)(1U << (frame % 8U))) != 0;
}

/* Mark every frame in [base, base+len) reserved. Out-of-window and
 * already-reserved frames are skipped, so overlapping/low regions are
 * safe to reserve twice. */
static void reserve_range(uint64_t base, uint64_t len)
{
    uint64_t end;
    uint64_t frame;

    if (len == 0U) {
        return;
    }
    end = base + len;
    if (end < base) {
        end = UINT64_MAX;
    }
    if (base >= PMM_MAX_BYTES) {
        return;
    }
    if (end > PMM_MAX_BYTES) {
        end = PMM_MAX_BYTES;
    }
    for (frame = base / PMM_FRAME_SIZE; frame < end / PMM_FRAME_SIZE; frame++) {
        if (test_bit(frame) == 0) {
            set_bit(frame);
            g_free--;
        }
    }
}

void pmm_init(uint32_t count, const struct e820_entry *entries)
{
    uint64_t i;
    uint64_t frame;
    uint64_t start;
    uint64_t end;

    /* Start fully reserved; usable E820 frames are cleared below. This
     * way unknown memory can never be handed out by accident. */
    for (i = 0; i < sizeof(bitmap); i++) {
        bitmap[i] = 0xFFU;
    }
    g_usable = 0;
    g_free = 0;

    if (entries == 0) {
        count = 0;
    }
    if (count > E820_MAX) {
        count = E820_MAX;
    }
    for (i = 0; i < count; i++) {
        if (entries[i].type != E820_USABLE) {
            continue;
        }
        if (entries[i].length == 0U) {
            continue;
        }
        start = entries[i].base;
        end = start + entries[i].length;
        if (end < start) {
            end = UINT64_MAX;
        }
        if (start >= PMM_MAX_BYTES) {
            continue; /* above the identity-mapped window: ignored */
        }
        if (end > PMM_MAX_BYTES) {
            end = PMM_MAX_BYTES;
        }
        /* Align the range to whole frames */
        start = (start + PMM_FRAME_SIZE - 1U) / PMM_FRAME_SIZE;
        end = end / PMM_FRAME_SIZE;
        for (frame = start; frame < end; frame++) {
            if (test_bit(frame) != 0) {
                clear_bit(frame);
                g_usable++;
                g_free++;
            }
        }
    }

    /* Second pass: carve out every non-usable E820 range. WHY a separate
     * pass instead of skipping overlaps up front: BIOS maps routinely
     * stack a reserved entry (e.g. ACPI data) inside a larger usable one,
     * and reserve_range is idempotent, so usable-then-reserved converges
     * on the right bitmap whatever order the firmware lists. */
    for (i = 0; i < count; i++) {
        if (entries[i].type != E820_USABLE) {
            reserve_range(entries[i].base, entries[i].length);
        }
    }

    reserve_range(0x00000000ULL, 0x00100000ULL);
    reserve_range((uint64_t)(uintptr_t)__kernel_start,
                  (uint64_t)(uintptr_t)__kernel_end - (uint64_t)(uintptr_t)__kernel_start);
    reserve_range(0x00070000ULL, 0x00004000ULL);
    reserve_range(0x00005000ULL, 0x00000620ULL);
    reserve_range(0x0008C000ULL, 0x00004000ULL);
    reserve_range(0x000B8000ULL, 0x00001000ULL);
}

uint64_t pmm_alloc_frame(void)
{
    uint64_t frame;

    for (frame = 0; frame < PMM_MAX_FRAMES; frame++) {
        if (test_bit(frame) == 0) {
            set_bit(frame);
            g_free--;
            return frame * PMM_FRAME_SIZE;
        }
    }
    return 0; /* frame 0 is always reserved: 0 unambiguously means OOM */
}

void pmm_free_frame(uint64_t addr)
{
    uint64_t frame;

    if ((addr % PMM_FRAME_SIZE) != 0U) {
        return;
    }
    if (addr >= PMM_MAX_BYTES) {
        return;
    }
    frame = addr / PMM_FRAME_SIZE;
    if (test_bit(frame) != 0) {
        clear_bit(frame);
        g_free++;
    }
}

uint64_t pmm_total_count(void)
{
    return PMM_MAX_FRAMES;
}

uint64_t pmm_free_count(void)
{
    return g_free;
}

uint64_t pmm_usable_count(void)
{
    return g_usable;
}
