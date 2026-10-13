#ifndef KERNEL_MEM_H
#define KERNEL_MEM_H

#include <stdbool.h>
#include "spinlock.h"
#include "types.h"

#define PAGE_SIZE 4096UL
#define KERNEL_PAGES 1024
#define VA_MAX (1UL << 38)
#define PA_MAX (1UL << 56)
#define PAGE_DOWN(a) ((uint64)(a) & ~(PAGE_SIZE - 1))
#define PAGE_UP(a) (((uint64)(a) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PTE_V 1UL
#define PTE_R 2UL
#define PTE_W 4UL
#define PTE_X 8UL
#define PTE_U 16UL
#define PTE_G 32UL
#define PTE_A 64UL
#define PTE_D 128UL
#define PA_TO_PTE(pa) (((uint64)(pa) >> 12) << 10)
#define PTE_TO_PA(pte) (((uint64)(pte) >> 10) << 12)
#define VPN(va, level) (((va) >> (12 + 9 * (level))) & 511)

typedef uint64 pte_t;
typedef pte_t *pgtbl_t;
typedef struct page_node { struct page_node *next; } page_node_t;
typedef struct {
    uint64 begin, end;
    struct spinlock lk;
    uint32 allocable;
    page_node_t list_head;
} alloc_region_t;

extern pgtbl_t kernel_pgtbl;
extern char _kernel_start[], text_end[], rodata_end[], kernel_end[];
void pmem_init(void);
void *pmem_alloc(bool kernel);
void pmem_free(uint64 pa, bool kernel);
uint32 pmem_available(bool kernel);
pte_t *vm_getpte(pgtbl_t root, uint64 va, bool alloc);
void vm_mappages(pgtbl_t root, uint64 va, uint64 pa, uint64 len, uint64 perm);
void vm_unmappages(pgtbl_t root, uint64 va, uint64 len, bool free_pages);
void vm_freewalk(pgtbl_t root);
void vm_print(pgtbl_t root);
void kvm_init(void);
void kvm_inithart(void);

#endif
