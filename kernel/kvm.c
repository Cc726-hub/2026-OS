#include "mem.h"
#include "memlayout.h"
#include "printf.h"
#include "riscv.h"

pgtbl_t kernel_pgtbl;

pte_t *
vm_getpte(pgtbl_t root, uint64 va, bool alloc)
{
    if (root == 0 || (uint64)root % PAGE_SIZE || va >= VA_MAX)
        panic("vm_getpte: invalid root or VA");
    for (int level = 2; level > 0; level--) {
        pte_t *pte = &root[VPN(va, level)];
        if (*pte & PTE_V) {
            if (*pte & (PTE_R | PTE_W | PTE_X))
                panic("vm_getpte: unexpected huge page");
            root = (pgtbl_t)PTE_TO_PA(*pte);
        } else {
            if (!alloc)
                return 0;
            root = pmem_alloc(true);
            *pte = PA_TO_PTE(root) | PTE_V;
        }
    }
    return &root[VPN(va, 0)];
}

/* Return the rounded coverage without ever adding an unchecked length. */
static uint64
mapping_size(uint64 va, uint64 len)
{
    if (va % PAGE_SIZE || va >= VA_MAX || len == 0 || len > VA_MAX - va)
        panic("vm range: invalid VA or length");
    return PAGE_UP(len);
}

void
vm_mappages(pgtbl_t root, uint64 va, uint64 pa, uint64 len, uint64 perm)
{
    uint64 size = mapping_size(va, len);
    uint64 allowed = PTE_R | PTE_W | PTE_X | PTE_U | PTE_G | PTE_A | PTE_D;
    if (pa % PAGE_SIZE || pa >= PA_MAX || size > PA_MAX - pa)
        panic("vm_mappages: invalid PA range");
    if ((perm & ~allowed) || !(perm & (PTE_R | PTE_X)) ||
        ((perm & PTE_W) && !(perm & PTE_R)))
        panic("vm_mappages: invalid permissions");
    for (uint64 offset = 0; offset < size; offset += PAGE_SIZE) {
        pte_t *pte = vm_getpte(root, va + offset, true);
        if ((*pte & PTE_V) &&
            (!(*pte & (PTE_R | PTE_X)) || PTE_TO_PA(*pte) != pa + offset))
            panic("vm_mappages: remap to different PA");
        *pte = PA_TO_PTE(pa + offset) | perm | PTE_V | PTE_A | PTE_D;
    }
    /* Local TLB only. Callers must arrange remote shootdowns if shared
       active page tables are ever modified while other harts use them. */
    sfence_vma();
}

void
vm_unmappages(pgtbl_t root, uint64 va, uint64 len, bool free_pages)
{
    uint64 size = mapping_size(va, len);
    for (uint64 offset = 0; offset < size; offset += PAGE_SIZE) {
        pte_t *pte = vm_getpte(root, va + offset, false);
        if (pte == 0 || !(*pte & PTE_V) || !(*pte & (PTE_R | PTE_X)))
            panic("vm_unmappages: missing leaf");
        uint64 pa = PTE_TO_PA(*pte);
        *pte = 0;
        sfence_vma();
        if (free_pages)
            pmem_free(pa, false);
    }
}

static void
free_level(pgtbl_t root, int level)
{
    for (unsigned i = 0; i < 512; i++) {
        if (!(root[i] & PTE_V))
            continue;
        if (level == 0 || (root[i] & (PTE_R | PTE_W | PTE_X)))
            panic("vm_freewalk: leaf still mapped");
        free_level((pgtbl_t)PTE_TO_PA(root[i]), level - 1);
        root[i] = 0;
    }
    pmem_free((uint64)root, true);
}

void
vm_freewalk(pgtbl_t root)
{
    /* Only for an inactive table whose leaves have already been removed. */
    free_level(root, 2);
}

void
kvm_init(void)
{
    if (kernel_pgtbl != 0)
        panic("kvm_init: already initialized");
    kernel_pgtbl = pmem_alloc(true);
    vm_mappages(kernel_pgtbl, UART_BASE, UART_BASE, PAGE_SIZE, PTE_R | PTE_W);
    vm_mappages(kernel_pgtbl, PLIC_BASE, PLIC_BASE, PLIC_SIZE, PTE_R | PTE_W);
    uint64 begin = (uint64)_kernel_start;
    vm_mappages(kernel_pgtbl, begin, begin, (uint64)text_end - begin, PTE_R | PTE_X);
    begin = (uint64)text_end;
    vm_mappages(kernel_pgtbl, begin, begin, (uint64)rodata_end - begin, PTE_R);
    begin = (uint64)rodata_end;
    vm_mappages(kernel_pgtbl, begin, begin, DRAM_END - begin, PTE_R | PTE_W);
}

void
kvm_inithart(void)
{
    if (kernel_pgtbl == 0)
        panic("kvm_inithart: missing root");
    sfence_vma();
    w_satp((8UL << 60) | ((uint64)kernel_pgtbl >> 12));
    sfence_vma();
}

static void
print_level(pgtbl_t root, unsigned level)
{
    for (unsigned i = 0; i < 512; i++) {
        pte_t pte = root[i];
        if (!(pte & PTE_V))
            continue;
        for (unsigned n = level; n < 3; n++)
            printf(".. ");
        printf("level=%d index=%d pa=%p flags=%x\n", (int)level, (int)i,
               (void *)PTE_TO_PA(pte), (unsigned)(pte & 1023));
        if (level && !(pte & (PTE_R | PTE_W | PTE_X)))
            print_level((pgtbl_t)PTE_TO_PA(pte), level - 1);
    }
}

void
vm_print(pgtbl_t root)
{
    printf("root pgtbl: pa=%p\n", (void *)root);
    print_level(root, 2);
}
