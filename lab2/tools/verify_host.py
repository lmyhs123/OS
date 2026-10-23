#!/usr/bin/env python3
import subprocess,time,pathlib,sys
root=pathlib.Path(__file__).resolve().parents[1]
qemu='/home/lmy/oslab-tools/qemu-ubuntu-build/qemu-system-riscv64'
source='/home/lmy/oslab-tools/qemu-source/target/riscv/cpu_helper.c'
# Observe first fetch at high kernel address; different run from the guest trace.
symbol=subprocess.check_output(['riscv64-unknown-elf-nm','bin/kernel'],cwd=root,text=True)
name=sys.argv[1] if len(sys.argv)>1 else 'kern_init'
va=int(next(l.split()[0] for l in symbol.splitlines() if l.endswith(' '+name)),16)
prefix='host' if name=='kern_init' else 'host-data'
script=rf"""
set pagination off
set confirm off
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
set logging file evidence/host-gdb.log
set logging overwrite on
set logging enabled on
break riscv_cpu_tlb_fill if address == {va} || address == 0x80200000
commands
silent
printf "TLB_MISS address=%#lx access=%d mmu_idx=%d\n", address, access_type, mmu_idx
bt 5
continue
end
break get_physical_address if addr == {va} || addr == 0x80200000
commands
silent
printf "WALK_ENTER addr=%#lx satp=%#lx is_debug=%d\n",addr,env->satp,is_debug
continue
end
break {source}:973 if addr == {va} && !is_debug
commands
silent
printf "PTE_READ addr=%#lx i=%d ptshift=%d base=%#lx pte_addr=%#lx pte=%#lx\n",addr,i,ptshift,base,pte_addr,pte
continue
end
break {source}:1404 if address == {va}
commands
silent
printf "TRANSLATED VA=%#lx PA=%#lx prot=%d cached_size=%#lx\n",address,pa,prot,tlb_size
printf "HOST_TRACE_COMPLETE\n"
kill
quit
end
run
"""
script=script.replace('evidence/host-gdb.log',f'evidence/{prefix}-gdb.log')
(root/f'evidence/{prefix}-commands.gdb').write_text(script)
args=['gdb','-q','-batch','-x',f'evidence/{prefix}-commands.gdb','--args',qemu,'-L','/usr/share/qemu','-machine','virt','-nographic','-bios','default','-kernel','bin/ucore.img','-gdb','tcp:127.0.0.1:1236','-S']
with (root/f'evidence/{prefix}-console.log').open('w') as out:
 host=subprocess.Popen(args,cwd=root,stdout=out,stderr=subprocess.STDOUT)
 try:
  time.sleep(3)
  guest=subprocess.run(['gdb-multiarch','-q','-batch','-ex','set confirm off','-ex','file bin/kernel','-ex','target remote :1236','-ex','continue'],cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=50)
  (root/f'evidence/{prefix}-driver-guest.log').write_bytes(guest.stdout)
  host.wait(timeout=10)
 finally:
  if host.poll() is None:host.terminate();host.wait(timeout=5)
log=(root/f'evidence/{prefix}-console.log').read_text()
print(log[-12000:])
assert 'HOST_TRACE_COMPLETE' in log
