set pagination off
set confirm off
handle SIGPIPE nostop noprint pass
handle SIGUSR1 nostop noprint pass
break riscv_cpu_tlb_fill if address == 0xffffffffc0206008
break get_physical_address if addr == 0xffffffffc0206008 && !is_debug
break /home/lmy/oslab-tools/qemu-source/target/riscv/cpu_helper.c:973 if addr == 0xffffffffc0206008 && !is_debug
break /home/lmy/oslab-tools/qemu-source/target/riscv/cpu_helper.c:1404 if address == 0xffffffffc0206008
run
