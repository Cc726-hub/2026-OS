#include "lab2_test.h"
#if LAB2_TEST
#include "mem.h"
#include "memlayout.h"
#include "param.h"
#include "printf.h"
#include "riscv.h"
#include "string.h"

static uint64 pages[DRAM_SIZE / PAGE_SIZE];
static uint32 owners[DRAM_SIZE / PAGE_SIZE];
static uint32 allocated[NCPU], freed[NCPU], done[NCPU];
static uint32 kernel_before, user_before;
static const uint64 alias = UART_BASE + 16 * PAGE_SIZE;

static void
check(bool condition, const char *message)
{
    if (!condition)
        panic(message);
}

static void
check_zero(void *page)
{
    for (uint32 i = 0; i < PAGE_SIZE / sizeof(uint64); i++)
        check(((uint64 *)page)[i] == 0, "test: allocation not zeroed");
}

static uint64
claim(void *page, uint32 hart)
{
    uint64 pa = (uint64)page;
    check(pa >= (uint64)kernel_end && pa < DRAM_END && pa % PAGE_SIZE == 0,
          "test: page out of RAM");
    uint64 index = (pa - DRAM_BASE) / PAGE_SIZE;
    check(__atomic_exchange_n(&owners[index], hart + 1, __ATOMIC_ACQ_REL) == 0,
          "test: duplicate allocation");
    return index;
}

static void
exhaust_and_restore(bool kernel)
{
    uint32 count = pmem_available(kernel);
    for (uint32 i = 0; i < count; i++) {
        pages[i] = (uint64)pmem_alloc(kernel);
        (void)claim((void *)pages[i], 0);
    }
    check(pmem_available(kernel) == 0, "test: pool did not exhaust");
    for (uint32 i = 0; i < count; i++) {
        __atomic_store_n(&owners[(pages[i] - DRAM_BASE) / PAGE_SIZE], 0, __ATOMIC_RELEASE);
        pmem_free(pages[i], kernel);
    }
    check(pmem_available(kernel) == count, "test: pool count not restored");
}

void
lab2_test_physical(void)
{
    if (TEST_CASE == 1 || TEST_CASE == 2) {
        bool kernel = TEST_CASE == 1;
        uint32 count = pmem_available(kernel);
        for (uint32 i = 0; i <= count; i++)
            (void)pmem_alloc(kernel);
        panic("test: exhaustion did not panic");
    }
    if (TEST_CASE == 3)
        pmem_free((uint64)pmem_alloc(true), false);
    if (TEST_CASE == 4) {
        uint64 pa = (uint64)pmem_alloc(false);
        pmem_free(pa, false);
        pmem_free(pa, false);
    }
    if (TEST_CASE == 5)
        pmem_free((uint64)pmem_alloc(false) + 1, false);
    if (TEST_CASE == 16)
        pmem_free(0x80000000UL, true);
    if (TEST_CASE == 17)
        pmem_free((uint64)kernel_end + KERNEL_PAGES * PAGE_SIZE, true);

    for (int pool = 0; pool < 2; pool++) {
        bool kernel = pool != 0;
        uint32 count = pmem_available(kernel);
        for (int i = 0; i < 10; i++) {
            void *page = pmem_alloc(kernel);
            check_zero(page);
            memset(page, 0xaa, PAGE_SIZE);
            pages[i] = (uint64)page;
        }
        check(pmem_available(kernel) == count - 10, "test: allocation count");
        for (int i = 0; i < 10; i++)
            pmem_free(pages[i], kernel);
        check(pmem_available(kernel) == count, "test: free count");
        void *page = pmem_alloc(kernel);
        check_zero(page);
        pmem_free((uint64)page, kernel);
        exhaust_and_restore(kernel);
    }
    printf("PASS physical: both pools, zeroing, exhaustion and recovery\n");
}

static void
check_mapping(uint64 va, uint64 perm)
{
    pte_t *pte = vm_getpte(kernel_pgtbl, va, false);
    check(pte != 0 && (*pte & PTE_V) && PTE_TO_PA(*pte) == PAGE_DOWN(va),
          "test: identity mapping");
    check((*pte & (PTE_R | PTE_W | PTE_X | PTE_U)) == perm, "test: mapping permission");
}

void
lab2_test_virtual(void)
{
    check_mapping((uint64)_kernel_start, PTE_R | PTE_X);
    check_mapping((uint64)text_end, PTE_R);
    check_mapping((uint64)rodata_end, PTE_R | PTE_W);
    check_mapping(DRAM_END - PAGE_SIZE, PTE_R | PTE_W);
    check_mapping(UART_BASE, PTE_R | PTE_W);
    check_mapping(PLIC_BASE, PTE_R | PTE_W);
    check_mapping(PLIC_BASE + PLIC_SIZE - PAGE_SIZE, PTE_R | PTE_W);
    pte_t *hole = vm_getpte(kernel_pgtbl, DRAM_BASE, false);
    check(hole == 0 || !(*hole & PTE_V), "test: firmware mapped");
    hole = vm_getpte(kernel_pgtbl, 0x02000000UL, false);
    check(hole == 0 || !(*hole & PTE_V), "test: CLINT mapped");

    if (TEST_CASE == 10)
        *(volatile uint32 *)_kernel_start = 0;
    if (TEST_CASE == 11)
        *(volatile uint32 *)text_end = 0;
    if (TEST_CASE == 12)
        *(volatile uint32 *)DRAM_BASE = 0;

    uint32 kc = pmem_available(true), uc = pmem_available(false);
    pgtbl_t root = pmem_alloc(true);
    uint64 pa = (uint64)pmem_alloc(false);
    if (TEST_CASE == 6) {
        vm_mappages(root, 0, pa, PAGE_SIZE, PTE_R);
        vm_mappages(root, 0, pa + PAGE_SIZE, PAGE_SIZE, PTE_R);
    }
    if (TEST_CASE == 7)
        vm_mappages(root, 0, pa, PAGE_SIZE, PTE_W);
    if (TEST_CASE == 8)
        vm_mappages(root, VA_MAX - PAGE_SIZE, pa, PAGE_SIZE + 1, PTE_R);
    if (TEST_CASE == 9)
        vm_unmappages(root, 0, PAGE_SIZE, false);
    if (TEST_CASE == 13)
        (void)vm_getpte(root, VA_MAX, false);
    if (TEST_CASE == 14)
        vm_mappages(root, 0, PA_MAX - PAGE_SIZE, PAGE_SIZE + 1, PTE_R);
    if (TEST_CASE == 15)
        vm_mappages(root, 0, pa, ~(uint64)0, PTE_R);
    if (TEST_CASE == 18)
        vm_mappages(root, 1, pa, PAGE_SIZE, PTE_R);
    if (TEST_CASE == 19)
        vm_mappages(root, 0, pa, 0, PTE_R);
    if (TEST_CASE == 20)
        vm_mappages(root, 0, pa, PAGE_SIZE, PTE_R | (1UL << 63));
    pmem_free(pa, false);

    const uint64 vas[] = {0, PAGE_SIZE * 10, PAGE_SIZE * 512,
                         PAGE_SIZE * 512 * 512, VA_MAX - PAGE_SIZE};
    const uint64 lens[] = {PAGE_SIZE, PAGE_SIZE / 2, PAGE_SIZE - 1, PAGE_SIZE, PAGE_SIZE};
    const uint64 perms[] = {PTE_R, PTE_R | PTE_W, PTE_R | PTE_X,
                           PTE_R | PTE_X, PTE_R | PTE_W};
    for (int i = 0; i < 5; i++) {
        pages[i] = (uint64)pmem_alloc(false);
        vm_mappages(root, vas[i], pages[i], lens[i], perms[i]);
        pte_t *pte = vm_getpte(root, vas[i], false);
        check(pte && PTE_TO_PA(*pte) == pages[i], "test: VA to PA translation");
        check((*pte & 255) == (perms[i] | PTE_V | PTE_A | PTE_D), "test: leaf flags");
    }
    printf("test page table before unmap:\n");
    vm_print(root);
    vm_mappages(root, 0, pages[0], PAGE_SIZE, PTE_R | PTE_W);
    check((*vm_getpte(root, 0, false) & PTE_W) != 0, "test: permission update");
    for (int i = 0; i < 5; i++) {
        vm_unmappages(root, vas[i], lens[i], true);
        check(*vm_getpte(root, vas[i], false) == 0, "test: unmap leaf");
    }
    printf("test page table after unmap:\n");
    vm_print(root);
    vm_freewalk(root);
    check(pmem_available(true) == kc && pmem_available(false) == uc, "test: VM page leak");

    /* This alias shares UART's existing level-0 table: no extra table leaks.
       Only the boot hart is running while the shared kernel table changes. */
    void *page = pmem_alloc(false);
    vm_mappages(kernel_pgtbl, alias, (uint64)page, PAGE_SIZE, PTE_R | PTE_W);
    *(volatile uint64 *)alias = 0x123456789abcdef0UL;
    check(*(volatile uint64 *)page == 0x123456789abcdef0UL, "test: MMU alias write");
    vm_unmappages(kernel_pgtbl, alias, PAGE_SIZE, false);
    if (TEST_CASE == 21)
        *(volatile uint64 *)alias = 0;
    void *other = pmem_alloc(false);
    vm_mappages(kernel_pgtbl, alias, (uint64)other, PAGE_SIZE, PTE_R | PTE_W);
    check(*(volatile uint64 *)alias == 0, "test: TLB remapping");
    if (TEST_CASE == 22) {
        *(volatile uint32 *)other = 0x00008067U; /* ret, if executable */
        __asm__ volatile("fence.i" ::: "memory");
        ((void (*)(void))alias)();
    }
    vm_unmappages(kernel_pgtbl, alias, PAGE_SIZE, true);
    pmem_free((uint64)page, false);
    check(pmem_available(true) == kc && pmem_available(false) == uc, "test: alias leak");
    printf("PASS virtual: boundaries, flags, remap, unmap, MMU alias and recovery\n");
    kernel_before = pmem_available(true);
    user_before = pmem_available(false);
}

static void
barrier(uint32 *flags, uint32 hart)
{
    __atomic_store_n(&flags[hart], 1, __ATOMIC_RELEASE);
    for (uint32 cpu = 0; cpu < NCPU; cpu++)
        while (__atomic_load_n(&flags[cpu], __ATOMIC_ACQUIRE) == 0)
            __asm__ volatile("nop");
}

void
lab2_test_hart(void)
{
    uint32 hart = (uint32)r_tp();
    check(r_satp() == ((8UL << 60) | ((uint64)kernel_pgtbl >> 12)), "test: per-hart satp");
    uint32 begin = hart * kernel_before / NCPU;
    uint32 end = (hart + 1) * kernel_before / NCPU;
    for (uint32 i = begin; i < end; i++) {
        void *page = pmem_alloc(true);
        (void)claim(page, hart);
        memset(page, 1, PAGE_SIZE);
        pages[i] = (uint64)page;
    }
    barrier(allocated, hart);
    check(pmem_available(true) == 0, "test: concurrent full pool count");
    /* Keep every hart past the count check before any frees begin. */
    static uint32 checked[NCPU];
    barrier(checked, hart);
    for (uint32 i = begin; i < end; i++) {
        check(*(uint32 *)pages[i] == 0x01010101U, "test: concurrent page data");
        __atomic_store_n(&owners[(pages[i] - DRAM_BASE) / PAGE_SIZE], 0, __ATOMIC_RELEASE);
        pmem_free(pages[i], true);
    }
    barrier(freed, hart);
    /* Run repeated allocate/write/free cycles on both pools simultaneously. */
    for (uint32 round = 0; round < 128; round++) {
        for (int pool = 0; pool < 2; pool++) {
            bool kernel = pool != 0;
            void *batch[8];
            for (unsigned i = 0; i < 8; i++) {
                batch[i] = pmem_alloc(kernel);
                (void)claim(batch[i], hart);
                check_zero(batch[i]);
                memset(batch[i], (int)hart + 7, PAGE_SIZE);
            }
            for (unsigned i = 0; i < 8; i++) {
                check(((uint8 *)batch[i])[PAGE_SIZE - 1] == hart + 7, "test: concurrent write");
                uint64 index = ((uint64)batch[i] - DRAM_BASE) / PAGE_SIZE;
                __atomic_store_n(&owners[index], 0, __ATOMIC_RELEASE);
                pmem_free((uint64)batch[i], kernel);
            }
        }
    }
    printf("PASS hart %d: concurrent allocation and Sv39\n", (int)hart);
    barrier(done, hart);
    if (hart == 0) {
        check(pmem_available(true) == kernel_before && pmem_available(false) == user_before,
              "test: concurrent page leak");
        printf("LAB2 TEST PASS cpus=%d kernel_free=%d user_free=%d\n",
               NCPU, (int)kernel_before, (int)user_before);
    }
}
#endif
