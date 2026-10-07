/* Nadir kernel heap allocator (see kmalloc.h)
 *
 * Page-granular first-fit heap over pmm_alloc_frame. WHY page-granular
 * for now: the PMM only hands out 4KB frames, and a slab/buddy layer
 * on top is a later step; this keeps the first heap small enough to
 * review line by line while giving the kernel a real kmalloc/kfree to
 * build on.
 *
 * Layout of an allocated run:
 *
 *   frame 0: [ header (16 bytes) | payload ... ]
 *   frames 1..n-1: payload continuation
 *
 * The header records the run length in frames so kfree can validate
 * and release the whole run, plus a magic so a wild pointer is caught
 * instead of corrupting the free list. Free runs carry a next pointer
 * and a frame count in their first 16 bytes.
 *
 * Ownership: kmalloc_init pulls KMALLOC_INIT_FRAMES frames from the
 * PMM once; from then on the heap owns those frames exclusively and
 * the PMM never sees them again. kfree returns runs to the heap free
 * list only. WHY: the PMM bitmap is the
 * authoritative owner of every frame, and a frame that is both on the
 * heap free list and free in the PMM would be handed out twice.
 *
 * WHY first-fit with split (no coalescing): simplest correct policy.
 * Fragmentation is bounded because every allocation is a whole number
 * of frames and the boot self-test exercises alloc/free churn.
 */

#include <stddef.h>
#include <stdint.h>

#include "kmalloc.h"
#include "pmm.h"

/* Magic stamped into allocated headers: catches kfree of a wild or
 * double-freed pointer before the free list is corrupted. */
#define KMALLOC_MAGIC 0x4E414449524D4148ULL /* "NADIRHAM" */

/* Frames the heap starts with: 64 frames = 256KB of kernel heap,
 * pulled from the PMM at init. Enough for early boot structures. */
#define KMALLOC_INIT_FRAMES 64U

struct kmalloc_header {
    uint64_t magic;
    uint64_t frames; /* total frames in this run, header included */
};

/* A free run: next pointer + frame count in the first 16 bytes. */
struct kmalloc_free {
    struct kmalloc_free *next;
    uint64_t frames;
};

/* Free list head: runs of whole frames available for allocation. */
static struct kmalloc_free *g_free_list;

static uint64_t g_free_frames;
static uint64_t g_used_frames;

/* Round a byte count up to whole frames, header included. A request
 * of `size` bytes needs ceil((size + header) / FRAME) frames. */
static uint64_t frames_for(size_t size)
{
    uint64_t bytes;

    if (size == 0U) {
        size = 1U;
    }
    bytes = (uint64_t)size + sizeof(struct kmalloc_header);
    return (bytes + PMM_FRAME_SIZE - 1U) / PMM_FRAME_SIZE;
}

void kmalloc_init(void)
{
    uint64_t addr = 0U;
    uint64_t i;
    struct kmalloc_free *run;

    g_free_list = NULL;
    g_free_frames = 0;
    g_used_frames = 0;

    /* Pull the init frames from the PMM. pmm_alloc_frame scans from
     * frame 0 upward, so consecutive calls return consecutive frames
     * while the low memory is free. */
    for (i = 0; i < KMALLOC_INIT_FRAMES; i++) {
        uint64_t next = pmm_alloc_frame();

        if (next == 0U) {
            break; /* PMM out of frames */
        }
        if (addr != 0U && next != addr + i * PMM_FRAME_SIZE) {
            break; /* non-contiguous */
        }
        if (addr == 0U) {
            addr = next;
        }
    }

    if (addr != 0U) {
        run = (struct kmalloc_free *)(uintptr_t)addr;
        run->next = NULL;
        run->frames = i;
        g_free_list = run;
        g_free_frames = i;
    }
}

void *kmalloc(size_t size)
{
    uint64_t need = frames_for(size);
    struct kmalloc_free **link = &g_free_list;
    struct kmalloc_free *run;

    /* walk the free list for the first run that fits. */
    while (*link != NULL) {
        run = *link;
        if (run->frames >= need) {
            struct kmalloc_header *hdr;
            uint64_t run_frames = run->frames;

            if (run->frames > need) {
                /* Split: the tail replaces this run in the free list,
                 * and the head `need` frames become the allocation.
                 * WHY contiguous: a run is always a contiguous frame
                 * range, so the split point is exact. WHY replace (not
                 * keep) the head in the list: the allocation header
                 * overwrites the first 16 bytes of the head, which
                 * would clobber the free node's next/frames fields. */
                struct kmalloc_free *tail;

                tail = (struct kmalloc_free *)
                    ((uintptr_t)run + need * PMM_FRAME_SIZE);
                tail->next = run->next;
                tail->frames = run->frames - need;
                *link = tail; /* tail takes this run's list slot */
                run_frames = need;
            } else {
                *link = run->next;
            }
            g_free_frames -= run_frames;
            hdr = (struct kmalloc_header *)run;
            hdr->magic = KMALLOC_MAGIC;
            hdr->frames = run_frames;
            g_used_frames += run_frames;
            return (void *)((uintptr_t)hdr + sizeof(struct kmalloc_header));
        }
        link = &run->next;
    }
    return NULL; /* no run big enough: OOM */
}

void kfree(void *ptr)
{
    struct kmalloc_header *hdr;
    struct kmalloc_free *node;

    if (ptr == NULL) {
        return;
    }
    hdr = (struct kmalloc_header *)
        ((uintptr_t)ptr - sizeof(struct kmalloc_header));
    if (hdr->magic != KMALLOC_MAGIC) {
        return; /* ignore if wild or double-freed pointer */
    }
    if (hdr->frames == 0U) {
        return;
    }
    /* Invalidate the magic first so a double free is caught. */
    hdr->magic = 0U;

    /* Return the run to the free list at the head. WHY head insert:
     * O(1), and first-fit finds it on the next kmalloc. */
    node = (struct kmalloc_free *)hdr;
    node->next = g_free_list;
    node->frames = hdr->frames;
    g_free_list = node;
    g_free_frames += hdr->frames;
    g_used_frames -= hdr->frames;
}

uint64_t kmalloc_free_frames(void)
{
    return g_free_frames;
}

uint64_t kmalloc_used_frames(void)
{
    return g_used_frames;
}
