#!/usr/bin/env python3
import subprocess,time,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
script=r"""set pagination off
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
"""
(root/'evidence/defense-commands.gdb').write_text(script)
with (root/'evidence/defense-qemu.log').open('w') as out:
 q=subprocess.Popen(['qemu-system-riscv64','-machine','virt','-nographic','-bios','default','-kernel','bin/ucore.img','-gdb','tcp:127.0.0.1:1237','-S'],cwd=root,stdout=out,stderr=subprocess.STDOUT)
 try:
  time.sleep(1)
  r=subprocess.run(['gdb-multiarch','-q','-batch','-x','evidence/defense-commands.gdb'],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
  (root/'evidence/defense-gdb.log').write_bytes(r.stdout)
  print(r.stdout.decode(errors='replace'));assert r.returncode==0
 finally:q.terminate();q.wait(timeout=5)
