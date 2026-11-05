#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &best_fit_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

//lab2 2413982
static void page_init(void) {
    // end 是链接器给出的内核静态布局结束地址，使用虚拟地址表示。
    extern char end[];
    // 读取此前解析 DTB 得到的 RAM 起始物理地址和总字节数。
    uint64_t base = get_memory_base(), size = get_memory_size();
    // 保存内核虚拟地址与物理地址之间的固定偏移。
    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
    // 当前框架要求 RAM 从 DRAM_BASE 开始，同时检查零长度和地址加法溢出。
    if (base != DRAM_BASE || size == 0 || size > (~(uint64_t)0) - base)
        panic("invalid or unsupported DTB memory range");
    // 管理上界取实际 RAM 与框架上界的较小值，向下对齐到完整的 4 KiB 页。
    uintptr_t limit = ROUNDDOWN(((base + size < PHYSICAL_MEMORY_END) ? base + size : PHYSICAL_MEMORY_END), PGSIZE);
    if (limit <= KERNEL_BEGIN_PADDR) panic("RAM does not contain kernel");
    // npage 是管理上界的绝对物理页号，nbase 是 RAM 起点的物理页号。
    npage = limit / PGSIZE;
    // 把内核结束地址向上按页对齐，作为 Page 描述符数组的虚拟起点。
    uintptr_t desc = ROUNDUP((uintptr_t)end, PGSIZE);
    // 用固定偏移换算数组的物理地址，方便检查它是否落在受管 RAM 内。
    uintptr_t desc_pa = desc - va_pa_offset;
    // 每个物理页对应一个描述符，数组元素数为受管区间内的页数。
    size_t count = npage - nbase;
    // 检查数组是否放得下，用除法比较避免先计算数组字节数时溢出。
    if (desc_pa >= limit || count > (limit - desc_pa) / sizeof(struct Page))
        panic("RAM too small for Page descriptors");
    pages = (struct Page *)desc;
    // 初始化描述符并先将所有页标为保留，稍后只开放确认空闲的区域。
    for (size_t i = 0; i < count; i++) {
        pages[i].flags = 0; pages[i].property = 0;
        set_page_ref(&pages[i], 0); SetPageReserved(&pages[i]);
    }
    // 空闲起点放在描述符数组之后，向上对齐，避开前面的固件和内核占用。
    uintptr_t first = ROUNDUP(desc_pa + count * sizeof(struct Page), PGSIZE);
    // 找到 DTB 所在物理页，本机 QEMU 将 DTB 放在 RAM 顶部附近。
    uintptr_t dtb_page = ROUNDDOWN(boot_dtb, PGSIZE);
    uintptr_t free_end = limit;
    // 若 DTB 位于候选空闲区，保守保留它所在页及其后的整个尾段。
    if (dtb_page >= first && dtb_page < free_end) free_end = dtb_page;
    // 打印实际 RAM 范围、受管上界及描述符数组信息，便于现场核对。
    cprintf("physical memory: [0x%016lx, 0x%016lx), managed end=0x%016lx\n",
            base, base + size, limit);
    cprintf("Page descriptors: PA=0x%016lx count=%lu size=%lu\n", desc_pa, count, sizeof(struct Page));
    // 存在完整空闲页时，将起始描述符和页数交给当前分配器建立空闲块。
    if (first < free_end) init_memmap(pa2page(first), (free_end - first) / PGSIZE);
    cprintf("free pages: %lu, range=[0x%016lx, 0x%016lx)\n", nr_free_pages(), first, free_end);
}



/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
