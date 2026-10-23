// lab2 function2-4 2411080: educational single-CPU slab-style object caches.
#include <slub.h>
#include <pmm.h>
#include <string.h>
#include <stdio.h>
#define MAGIC 0x51ab2026UL
#define ALIGN16(x) (((x)+15)&~15UL)
struct slab {
    uint64_t magic;
    struct slab *next;
    void *free;
    size_t unit, used, capacity;
    struct Page *page;
    size_t npages;
};
static struct slab *caches[7];
static const size_t sizes[7]={16,32,64,128,256,512,1024};
void *kmalloc(size_t n) {
    if (!n || n > 1024*1024) return NULL;
    unsigned c=0;
    while(c<7 && sizes[c]<n) c++;
    size_t header=ALIGN16(sizeof(struct slab));
    struct slab *s=NULL;
    if(c<7) for(s=caches[c];s && !s->free;s=s->next) ;
    if(!s) {
        size_t np=c<7?1:ROUNDUP(header+n,PGSIZE)/PGSIZE;
        struct Page *page=alloc_pages(np);
        if(!page) return NULL;
        s=(struct slab *)(page2pa(page)+va_pa_offset);
        s->magic=MAGIC;s->unit=c<7?sizes[c]:n;s->used=0;s->page=page;s->npages=np;
        s->free=NULL;s->next=NULL;
        if(c<7) {
            s->capacity=(PGSIZE-header)/s->unit;
            for(size_t i=0;i<s->capacity;i++) {
                void **obj=(void **)((uintptr_t)s+header+i*s->unit);
                *obj=s->free;s->free=obj;
            }
            s->next=caches[c];caches[c]=s;
        } else {s->capacity=1;s->used=1;return (void *)((uintptr_t)s+header);}
    }
    void *result=s->free; s->free=*(void **)result; s->used++;
    return result;
}
void kfree(void *p) {
    if(!p) return;
    struct slab *s=(struct slab *)ROUNDDOWN((uintptr_t)p,PGSIZE);
    assert(s->magic==MAGIC && s->used>0);
    uintptr_t start=(uintptr_t)s+ALIGN16(sizeof(*s));
    assert((uintptr_t)p>=start && ((uintptr_t)p-start)%s->unit==0);
    assert(((uintptr_t)p-start)/s->unit<s->capacity);
    if(s->unit<=1024) {
        for(void *f=s->free;f;f=*(void **)f) assert(f!=p);
        *(void **)p=s->free;s->free=p;
    }
    if(--s->used==0) {
        if(s->unit<=1024) {
            unsigned c=0;while(sizes[c]!=s->unit)c++;
            struct slab **link=&caches[c];while(*link!=s)link=&(*link)->next;
            *link=s->next;
        }
        struct Page *page=s->page;size_t np=s->npages;s->magic=0;free_pages(page,np);
    }
}
void slub_check(void) {
    size_t before=nr_free_pages();
    size_t requested[]={1,16,17,32,33,64,65,128,129,256,257,512,513,1024,1025,4096,9000};
    void *p[17];
    assert(!kmalloc(0) && !kmalloc(1024*1024+1));kfree(NULL);
    for(int i=0;i<17;i++) {
        p[i]=kmalloc(requested[i]);assert(p[i] && ((uintptr_t)p[i]&15)==0);
        memset(p[i],i+1,requested[i]);
    }
    for(int i=0;i<17;i++) for(size_t j=0;j<requested[i];j++)
        assert(((unsigned char *)p[i])[j]==i+1);
    for(int i=16;i>=0;i--) kfree(p[i]);
    assert(nr_free_pages()==before);
    void *many[300];
    for(int i=0;i<300;i++){many[i]=kmalloc(32);assert(many[i]);memset(many[i],i,32);}
    for(int i=0;i<300;i++)for(int j=0;j<32;j++)assert(((unsigned char*)many[i])[j]==(unsigned char)i);
    void *saved=many[299];kfree(saved);many[299]=kmalloc(32);assert(many[299]==saved);
    for(int i=0;i<300;i++)kfree(many[i]);
    assert(nr_free_pages()==before);
    cprintf("[SLUB] size-boundaries/alignment/data/multi-slab/reuse/reclaim: PASS\n");
}
