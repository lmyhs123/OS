#!/usr/bin/env python3
import subprocess,time,pathlib
root=pathlib.Path(__file__).resolve().parents[1]
cmd=['qemu-system-riscv64','-machine','virt','-nographic','-bios','default','-kernel','bin/ucore.img','-gdb','tcp:127.0.0.1:1235','-S']
script="""set pagination off
set confirm off
file bin/kernel
set architecture riscv:rv64
target remote :1235
info registers pc satp
x/6i $pc
hbreak kern_init
continue
info registers pc satp sp
p/x &boot_page_table_sv39
set $rootpa = ($satp & 0xfffffffffff) << 12
p/x $rootpa
set $va = (unsigned long)&kern_init
p/x $va
p/x ($va >> 30) & 511
x/gx $rootpa+511*8
x/gx $rootpa+2*8
x/6i $pc
si
info registers pc
continue
"""
# End at a controlled function instead of running the infinite loop.
script=script.rsplit('continue',1)[0]+'detach\nquit\n'
(root/'evidence/guest-commands.gdb').write_text(script)
with (root/'evidence/guest-qemu.log').open('w') as f:
 q=subprocess.Popen(cmd,cwd=root,stdout=f,stderr=subprocess.STDOUT)
 try:
  time.sleep(1)
  r=subprocess.run(['gdb-multiarch','-q','-batch','-x','evidence/guest-commands.gdb'],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
  (root/'evidence/guest-gdb.log').write_bytes(r.stdout)
  print(r.stdout.decode(errors='replace'));assert r.returncode==0
 finally:q.terminate();q.wait(timeout=5)
