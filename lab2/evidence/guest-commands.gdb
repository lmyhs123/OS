set pagination off
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
detach
quit
