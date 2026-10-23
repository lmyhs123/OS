
set pagination off
set confirm off
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
set logging file evidence/host-gdb.log
set logging overwrite on
set logging enabled on
break riscv_cpu_tlb_fill if address == 18446744072637907158 || address == 0x80200000
commands
silent
printf "TLB_MISS address=%#lx access=%d mmu_idx=%d\n", address, access_type, mmu_idx
bt 5
continue
end
break get_physical_address if addr == 18446744072637907158 || addr == 0x80200000
commands
silent
printf "WALK_ENTER addr=%#lx satp=%#lx is_debug=%d\n",addr,env->satp,is_debug
continue
end
break /home/lmy/oslab-tools/qemu-source/target/riscv/cpu_helper.c:973 if addr == 18446744072637907158 && !is_debug
commands
silent
printf "PTE_READ addr=%#lx i=%d ptshift=%d base=%#lx pte_addr=%#lx pte=%#lx\n",addr,i,ptshift,base,pte_addr,pte
continue
end
break /home/lmy/oslab-tools/qemu-source/target/riscv/cpu_helper.c:1404 if address == 18446744072637907158
commands
silent
printf "TRANSLATED VA=%#lx PA=%#lx prot=%d cached_size=%#lx\n",address,pa,prot,tlb_size
printf "HOST_TRACE_COMPLETE\n"
kill
quit
end
run
