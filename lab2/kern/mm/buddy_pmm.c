// lab2 function2-3 2411080: power-of-two page allocator, no dynamic metadata.
#include <buddy_pmm.h>
#include <stdio.h>
#define MAX_ORDER 15
static list_entry_t bins[MAX_ORDER + 1];
static size_t available;
static void buddy_reserved_check(void);
static unsigned order_for(size_t n) {
    unsigned o = 0;
    while (((size_t)1 << o) < n && o <= MAX_ORDER) o++;
    return o;
}
static void insert(struct Page *p, unsigned o) {
    p->property = o; SetPageProperty(p);
    list_add(&bins[o], &p->page_link);
}
static void buddy_init(void) {
    for (unsigned o = 0; o <= MAX_ORDER; o++) list_init(&bins[o]);
    available = 0;
}
static void release(struct Page *p, unsigned o) {
    while (o < MAX_ORDER) {
        size_t ppn = page2ppn(p), other = ppn ^ ((size_t)1 << o);
        if (other < nbase || other >= npage) break;
        struct Page *b = &pages[other - nbase];
        if (!PageProperty(b) || b->property != o) break;
        list_del(&b->page_link); ClearPageProperty(b);
        if (b < p) p = b;
        o++;
    }
    insert(p, o);
}
static void buddy_init_memmap(struct Page *p, size_t n) {
    assert(n > 0);
    for (size_t i = 0; i < n; i++) {
        assert(PageReserved(p + i));
        p[i].flags = 0; p[i].property = 0; set_page_ref(p+i, 0);
    }
    available += n;
    while (n) {
        unsigned o = MAX_ORDER;
        while (((size_t)1 << o) > n || (page2ppn(p) & (((size_t)1 << o)-1))) o--;
        release(p, o); p += (size_t)1 << o; n -= (size_t)1 << o;
    }
}
static struct Page *buddy_alloc(size_t n) {
    if (!n || n > ((size_t)1 << MAX_ORDER)) return NULL;
    unsigned want = order_for(n), o = want;
    while (o <= MAX_ORDER && list_empty(&bins[o])) o++;
    if (o > MAX_ORDER) return NULL;
    struct Page *p = le2page(list_next(&bins[o]), page_link);
    list_del(&p->page_link); ClearPageProperty(p);
    while (o > want) { o--; insert(p + ((size_t)1 << o), o); }
    p->property = want;
    available -= (size_t)1 << want;
    return p;
}
static void buddy_free(struct Page *p, size_t n) {
    assert(n > 0 && n <= ((size_t)1 << MAX_ORDER));
    unsigned o = order_for(n);
    assert(!PageReserved(p) && !PageProperty(p) && p->property == o);
    assert((page2ppn(p) & (((size_t)1 << o)-1)) == 0);
    for (size_t i=0; i<((size_t)1<<o); i++) { p[i].flags=0; set_page_ref(p+i,0); }
    available += (size_t)1 << o; release(p, o);
}
static size_t buddy_count(void) { return available; }
static void buddy_check(void) {
    size_t before = available;
    assert(buddy_alloc(0) == NULL);
    assert(buddy_alloc(((size_t)1 << MAX_ORDER)+1) == NULL);
    struct Page *a=buddy_alloc(3), *b=buddy_alloc(1), *c=buddy_alloc(8);
    assert(a && b && c);
    assert((page2ppn(a) & 3)==0 && (page2ppn(c)&7)==0);
    assert(a+4<=b || b+1<=a);
    assert(a+4<=c || c+8<=a);
    assert(b+1<=c || c+8<=b);
    assert(available==before-13);
    buddy_free(b,1); buddy_free(a,3); buddy_free(c,8);
    assert(available==before);
    /* Isolate an aligned 8-page block: split to exhaustion, then coalesce. */
    struct Page *block=buddy_alloc(8), *v[8];
    assert(block);
    list_entry_t saved[MAX_ORDER+1];
    for (unsigned o=0;o<=MAX_ORDER;o++) { saved[o]=bins[o]; list_init(&bins[o]); }
    size_t saved_n=available; available=0;
    buddy_free(block,8);
    for (int i=0;i<8;i++) { v[i]=buddy_alloc(1); assert(v[i]); }
    assert(!buddy_alloc(1) && available==0);
    for (int i=7;i>=0;i--) buddy_free(v[i],1);
    assert(available==8);
    assert(buddy_alloc(8)==block && available==0);
    for (unsigned o=0;o<=MAX_ORDER;o++) bins[o]=saved[o];
    available=saved_n; buddy_free(block,8);
    assert(available==before);
    buddy_reserved_check();
    cprintf("[Buddy] rounding/alignment/non-overlap/exhaustion/coalescing: PASS\n");
}
const struct pmm_manager buddy_pmm_manager={
 .name="buddy_pmm_manager", .init=buddy_init, .init_memmap=buddy_init_memmap,
 .alloc_pages=buddy_alloc, .free_pages=buddy_free,
 .nr_free_pages=buddy_count, .check=buddy_check,
};

/* A reserved hole must remain unavailable until explicitly returned. */
static void buddy_reserved_check(void) {
    size_t before = available;
    struct Page *block = buddy_alloc(16);
    assert(block && (page2ppn(block) & 15) == 0);
    list_entry_t saved[MAX_ORDER + 1];
    size_t saved_n = available;
    for (unsigned o = 0; o <= MAX_ORDER; o++) {
        saved[o] = bins[o];
        list_init(&bins[o]);
    }
    available = 0;
    for (size_t i = 0; i < 16; i++) {
        block[i].flags = 0;
        block[i].property = 0;
        set_page_ref(block + i, 0);
    }
    SetPageReserved(block + 7);
    for (size_t i = 0; i < 16; i++) {
        if (i != 7) buddy_free(block + i, 1);
    }
    assert(available == 15 && PageReserved(block + 7));
    cprintf("[Buddy16] reserved=1 free=%lu\n", available);

    struct Page *v[15];
    for (size_t i = 0; i < 15; i++) {
        v[i] = buddy_alloc(1);
        assert(v[i] && v[i] >= block && v[i] < block + 16);
        assert(v[i] != block + 7 && !PageReserved(v[i]));
        for (size_t j = 0; j < i; j++) assert(v[i] != v[j]);
    }
    assert(available == 0 && buddy_alloc(1) == NULL);
    cprintf("[Buddy16] exhausted free=%lu\n", available);

    for (int i = 14; i >= 0; i--) buddy_free(v[i], 1);
    assert(available == 15 && PageReserved(block + 7));
    ClearPageReserved(block + 7);
    buddy_free(block + 7, 1);
    assert(available == 16 && PageProperty(block) && block->property == 4);
    cprintf("[Buddy16] unreserved free=%lu\n", available);

    struct Page *three = buddy_alloc(3);
    struct Page *five = buddy_alloc(5);
    assert(three && five && three->property == 2 && five->property == 3);
    assert(three + 4 <= five || five + 8 <= three);
    assert(available == 4);
    cprintf("[Buddy16] alloc(3)=4 alloc(5)=8 free=%lu\n", available);
    buddy_free(three, 3);
    buddy_free(five, 5);
    assert(available == 16 && PageProperty(block) && block->property == 4);
    cprintf("[Buddy16] restored=%lu\n", available);
    assert(buddy_alloc(16) == block && available == 0);

    for (unsigned o = 0; o <= MAX_ORDER; o++) bins[o] = saved[o];
    available = saved_n;
    buddy_free(block, 16);
    assert(available == before);
    cprintf("[Buddy16] reservation/non-duplicate/rounding/full-merge: PASS\n");
}
