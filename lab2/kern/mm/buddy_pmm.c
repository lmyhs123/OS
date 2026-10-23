// lab2 function2-3 2411080: power-of-two page allocator, no dynamic metadata.
#include <buddy_pmm.h>
#include <stdio.h>
#define MAX_ORDER 15
static list_entry_t bins[MAX_ORDER + 1];
static size_t available;
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
    cprintf("[Buddy] rounding/alignment/non-overlap/exhaustion/coalescing: PASS\n");
}
const struct pmm_manager buddy_pmm_manager={
 .name="buddy_pmm_manager", .init=buddy_init, .init_memmap=buddy_init_memmap,
 .alloc_pages=buddy_alloc, .free_pages=buddy_free,
 .nr_free_pages=buddy_count, .check=buddy_check,
};
