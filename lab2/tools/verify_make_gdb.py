#!/usr/bin/env python3
import subprocess,time,pathlib,socket
root=pathlib.Path(__file__).resolve().parents[1]
with socket.socket() as s:
 if s.connect_ex(('127.0.0.1',1234))==0:raise SystemExit('1234 busy: stop the existing demo first')
commands="""set pagination off
set confirm off
hbreak kern_init
continue
info registers pc satp
set $va = (unsigned long)&kern_init
set $rootpa = ($satp & 0xfffffffffff) << 12
set $vpn2 = ($va >> 30) & 511
x/gx $rootpa+$vpn2*8
set $pte = *(unsigned long *)($rootpa+$vpn2*8)
set $pa = (($pte >> 10) << 12) | ($va & 0x3fffffff)
p/x $va
p/x $pa
p sizeof(struct Page)
p/x (unsigned long)&((struct Page *)0)->page_link
detach
quit
"""
(root/'evidence/make-gdb-commands.txt').write_text(commands)
with (root/'evidence/make-gdb-qemu.log').open('w') as out:
 q=subprocess.Popen(['qemu-system-riscv64','-machine','virt','-nographic','-bios','default','-kernel','bin/ucore.img','-s','-S'],cwd=root,stdout=out,stderr=subprocess.STDOUT)
 try:
  time.sleep(1)
  r=subprocess.run(['make','gdb'],input=commands.encode(),cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
  (root/'evidence/make-gdb.log').write_bytes(r.stdout)
  print(r.stdout.decode(errors='replace'));assert r.returncode==0
 finally:q.terminate();q.wait(timeout=5)
