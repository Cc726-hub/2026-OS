#include "boot.h"
#include "mem.h"
#include "memlayout.h"
#include "printf.h"
#include "string.h"

static alloc_region_t kern_region, user_region;
/* One bit per RAM page catches double frees without changing page contents. */
static uint32 free_bits[DRAM_SIZE / PAGE_SIZE / 32];
static uint64 dtb_begin, dtb_end;
static bool initialized;

static uint32
read_be32(const uint8 *p)
{
    return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) |
           ((uint32)p[2] << 8) | p[3];
}

static bool
reserved(uint64 pa)
{
    return pa >= dtb_begin && pa < dtb_end;
}

static void
set_free(uint64 pa, bool value)
{
    uint64 index = (pa - DRAM_BASE) / PAGE_SIZE;
    /* Pools have page-aligned boundaries, but may share one bitmap word.
       Atomic bit operations avoid losing updates under separate pool locks. */
    uint32 mask = 1U << (index % 32);
    if (value)
        __atomic_fetch_or(&free_bits[index / 32], mask, __ATOMIC_RELAXED);
    else
        __atomic_fetch_and(&free_bits[index / 32], ~mask, __ATOMIC_RELAXED);
}

static bool
is_free(uint64 pa)
{
    uint64 index = (pa - DRAM_BASE) / PAGE_SIZE;
    return (__atomic_load_n(&free_bits[index / 32], __ATOMIC_RELAXED) &
            (1U << (index % 32))) != 0;
}

static void
init_region(alloc_region_t *region, uint64 begin, uint64 end, const char *name)
{
    region->begin = begin;
    region->end = end;
    initlock(&region->lk, name);
    for (uint64 pa = begin; pa < end; pa += PAGE_SIZE) {
        if (reserved(pa))
            continue;
        page_node_t *node = (page_node_t *)pa;
        node->next = region->list_head.next;
        region->list_head.next = node;
        region->allocable++;
        set_free(pa, true);
    }
}

void
pmem_init(void)
{
    if (initialized)
        panic("pmem_init: already initialized");
    uint64 begin = PAGE_UP(kernel_end);
    uint64 split = begin + KERNEL_PAGES * PAGE_SIZE;
    if (begin < DRAM_BASE || split >= DRAM_END)
        panic("pmem_init: invalid RAM layout");

    /* QEMU puts the cold-boot DTB inside RAM. Preserve it for later labs. */
    if (boot_dtb >= DRAM_BASE && boot_dtb <= DRAM_END - 8) {
        const uint8 *dtb = (const uint8 *)boot_dtb;
        uint32 size = read_be32(dtb + 4);
        if (read_be32(dtb) != 0xd00dfeedU || size < 40 ||
            size > DRAM_END - boot_dtb)
            panic("pmem_init: invalid DTB");
        dtb_begin = PAGE_DOWN(boot_dtb);
        dtb_end = PAGE_UP(boot_dtb + size);
        if (dtb_begin < split && dtb_end > (uint64)_kernel_start)
            panic("pmem_init: DTB overlaps kernel pool");
    }
    init_region(&kern_region, begin, split, "kernel pages");
    init_region(&user_region, split, DRAM_END, "user pages");
    initialized = true;
    printf("physical pages: kernel=%d user=%d; kernel_end=%p\n",
           (int)kern_region.allocable, (int)user_region.allocable, kernel_end);
}

void *
pmem_alloc(bool kernel)
{
    if (!initialized)
        panic("pmem_alloc: not initialized");
    alloc_region_t *region = kernel ? &kern_region : &user_region;
    acquire(&region->lk);
    page_node_t *node = region->list_head.next;
    if (node == 0)
        panic(kernel ? "pmem_alloc: kernel pool exhausted" :
                       "pmem_alloc: user pool exhausted");
    region->list_head.next = node->next;
    region->allocable--;
    set_free((uint64)node, false);
    release(&region->lk);
    memset(node, 0, PAGE_SIZE);
    return node;
}

void
pmem_free(uint64 pa, bool kernel)
{
    if (!initialized)
        panic("pmem_free: not initialized");
    alloc_region_t *region = kernel ? &kern_region : &user_region;
    if (pa % PAGE_SIZE || pa < region->begin || pa >= region->end || reserved(pa))
        panic("pmem_free: invalid page or wrong pool");
    acquire(&region->lk);
    if (is_free(pa))
        panic("pmem_free: double free");
    memset((void *)pa, 1, PAGE_SIZE);
    page_node_t *node = (page_node_t *)pa;
    node->next = region->list_head.next;
    region->list_head.next = node;
    region->allocable++;
    set_free(pa, true);
    release(&region->lk);
}

uint32
pmem_available(bool kernel)
{
    if (!initialized)
        panic("pmem_available: not initialized");
    alloc_region_t *region = kernel ? &kern_region : &user_region;
    acquire(&region->lk);
    uint32 count = region->allocable;
    release(&region->lk);
    return count;
}
