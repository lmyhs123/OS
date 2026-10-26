set pagination off
set confirm off
file bin/kernel
target remote :1237
hbreak kern/mm/best_fit_pmm.c:237
continue
printf "DEFENSE_BEST_FIT_BEFORE\n"
p/x p0
p sizeof(struct Page)
p/x (unsigned long)&((struct Page *)0)->page_link
p free_area.nr_free
set $node = free_area.free_list.next
set $offset = (unsigned long)&((struct Page *)0)->page_link
set $block = (struct Page *)((char *)$node - $offset)
p $block-p0
p $block->property
set $node = $node->next
set $block = (struct Page *)((char *)$node - $offset)
p $block-p0
p $block->property
next
printf "DEFENSE_BEST_FIT_AFTER\n"
p p1-p0
p free_area.nr_free
p/x p0->flags
p/x p0[1].flags
p/x p0[4].flags
detach
quit
