#include <riscv.h>
/* 将旧控制寄存器名映射为当前汇编器支持的名称，两者指向同一寄存器。 */
#define sbadaddr stval

    # 保存被打断时的现场：先暂存原 sp，再在内核栈上腾出 288 字节。
    # 将通用寄存器及 sstatus、sepc、stval、scause 保存为 trapframe，
    # 使处理函数可以使用寄存器，并保留原程序继续执行所需的状态。
    .macro SAVE_ALL # 定义保存现场的汇编宏

    csrw sscratch, sp # 暂存被打断时的原栈指针，下一步会改变 sp

    addi sp, sp, -36 * REGBYTES # 栈向低地址扩展，为现场分配 36×8＝288 字节
    # 将通用寄存器存入对应槽位，每个槽位占 8 字节。
    # x2 就是 sp，它已因分配现场空间而改变，原值稍后从 sscratch 取回。
    STORE x0, 0*REGBYTES(sp) # 保存恒为零的寄存器，放入第 0 号槽位
    STORE x1, 1*REGBYTES(sp) # 保存调用返回地址，放入第 1 号槽位
    STORE x3, 3*REGBYTES(sp) # 保存全局指针，放入第 3 号槽位
    STORE x4, 4*REGBYTES(sp) # 保存线程指针，放入第 4 号槽位
    STORE x5, 5*REGBYTES(sp) # 保存临时寄存器，放入第 5 号槽位
    STORE x6, 6*REGBYTES(sp) # 保存临时寄存器，放入第 6 号槽位
    STORE x7, 7*REGBYTES(sp) # 保存临时寄存器，放入第 7 号槽位
    STORE x8, 8*REGBYTES(sp) # 保存保存寄存器及可选栈帧指针，放入第 8 号槽位
    STORE x9, 9*REGBYTES(sp) # 保存保存寄存器，放入第 9 号槽位
    STORE x10, 10*REGBYTES(sp) # 保存函数参数寄存器及返回值寄存器，放入第 10 号槽位
    STORE x11, 11*REGBYTES(sp) # 保存函数参数寄存器及返回值寄存器，放入第 11 号槽位
    STORE x12, 12*REGBYTES(sp) # 保存函数参数寄存器，放入第 12 号槽位
    STORE x13, 13*REGBYTES(sp) # 保存函数参数寄存器，放入第 13 号槽位
    STORE x14, 14*REGBYTES(sp) # 保存函数参数寄存器，放入第 14 号槽位
    STORE x15, 15*REGBYTES(sp) # 保存函数参数寄存器，放入第 15 号槽位
    STORE x16, 16*REGBYTES(sp) # 保存函数参数寄存器，放入第 16 号槽位
    STORE x17, 17*REGBYTES(sp) # 保存函数参数寄存器，放入第 17 号槽位
    STORE x18, 18*REGBYTES(sp) # 保存保存寄存器，放入第 18 号槽位
    STORE x19, 19*REGBYTES(sp) # 保存保存寄存器，放入第 19 号槽位
    STORE x20, 20*REGBYTES(sp) # 保存保存寄存器，放入第 20 号槽位
    STORE x21, 21*REGBYTES(sp) # 保存保存寄存器，放入第 21 号槽位
    STORE x22, 22*REGBYTES(sp) # 保存保存寄存器，放入第 22 号槽位
    STORE x23, 23*REGBYTES(sp) # 保存保存寄存器，放入第 23 号槽位
    STORE x24, 24*REGBYTES(sp) # 保存保存寄存器，放入第 24 号槽位
    STORE x25, 25*REGBYTES(sp) # 保存保存寄存器，放入第 25 号槽位
    STORE x26, 26*REGBYTES(sp) # 保存保存寄存器，放入第 26 号槽位
    STORE x27, 27*REGBYTES(sp) # 保存保存寄存器，放入第 27 号槽位
    STORE x28, 28*REGBYTES(sp) # 保存临时寄存器，放入第 28 号槽位
    STORE x29, 29*REGBYTES(sp) # 保存临时寄存器，放入第 29 号槽位
    STORE x30, 30*REGBYTES(sp) # 保存临时寄存器，放入第 30 号槽位
    STORE x31, 31*REGBYTES(sp) # 保存临时寄存器，放入第 31 号槽位

    # 控制寄存器需要先读到通用寄存器，再保存到栈上的现场结构。
    # s0 到 s4 的原值已在上方保存，因此现在可以借用它们。
    csrrw s0, sscratch, x0 # 把暂存的原 sp 读入 s0，同时将 sscratch 清零
    csrr s1, sstatus # 读取权限和中断使能等状态
    csrr s2, sepc # 读取处理结束后继续执行的位置
    csrr s3, sbadaddr # 读取异常附加信息，当前名称为 stval
    csrr s4, scause # 读取本次中断或异常的原因

    STORE s0, 2*REGBYTES(sp) # 把原 sp 存入属于 x2 的第 2 号槽位，现场布局仍按寄存器编号排列
    STORE s1, 32*REGBYTES(sp) # 保存处理器状态到第 32 号槽位
    STORE s2, 33*REGBYTES(sp) # 保存继续执行位置到第 33 号槽位
    STORE s3, 34*REGBYTES(sp) # 保存异常附加信息到第 34 号槽位
    STORE s4, 35*REGBYTES(sp) # 保存异常原因到第 35 号槽位
    .endm # 汇编宏定义结束

    # 恢复处理后的现场：先恢复 sstatus、sepc，再恢复通用寄存器。
    # 异常处理函数可能已调整 frame 中的 epc，以便跳过触发异常的指令。
    # 最后恢复 sp，保证此前始终能通过当前栈指针定位 frame 中的字段。
    .macro RESTORE_ALL # 定义恢复现场的汇编宏

    #lab3 function1 2411080
    LOAD t0, 32*REGBYTES(sp) # 取出保存的处理器状态，暂放 t0
    LOAD t1, 33*REGBYTES(sp) # 取出处理后的返回地址，暂放 t1
    csrw sstatus, t0 # 写回状态，供返回指令恢复权限及中断使能
    csrw sepc, t1 # 写回返回地址，异常处理可能已调整该值
    LOAD x1, 1*REGBYTES(sp) # 从第 1 号槽位恢复调用返回地址
    LOAD x3, 3*REGBYTES(sp) # 从第 3 号槽位恢复全局指针
    LOAD x4, 4*REGBYTES(sp) # 从第 4 号槽位恢复线程指针
    LOAD x5, 5*REGBYTES(sp) # 从第 5 号槽位恢复临时寄存器
    LOAD x6, 6*REGBYTES(sp) # 从第 6 号槽位恢复临时寄存器
    LOAD x7, 7*REGBYTES(sp) # 从第 7 号槽位恢复临时寄存器
    LOAD x8, 8*REGBYTES(sp) # 从第 8 号槽位恢复保存寄存器及可选栈帧指针
    LOAD x9, 9*REGBYTES(sp) # 从第 9 号槽位恢复保存寄存器
    LOAD x10, 10*REGBYTES(sp) # 从第 10 号槽位恢复函数参数寄存器及返回值寄存器
    LOAD x11, 11*REGBYTES(sp) # 从第 11 号槽位恢复函数参数寄存器及返回值寄存器
    LOAD x12, 12*REGBYTES(sp) # 从第 12 号槽位恢复函数参数寄存器
    LOAD x13, 13*REGBYTES(sp) # 从第 13 号槽位恢复函数参数寄存器
    LOAD x14, 14*REGBYTES(sp) # 从第 14 号槽位恢复函数参数寄存器
    LOAD x15, 15*REGBYTES(sp) # 从第 15 号槽位恢复函数参数寄存器
    LOAD x16, 16*REGBYTES(sp) # 从第 16 号槽位恢复函数参数寄存器
    LOAD x17, 17*REGBYTES(sp) # 从第 17 号槽位恢复函数参数寄存器
    LOAD x18, 18*REGBYTES(sp) # 从第 18 号槽位恢复保存寄存器
    LOAD x19, 19*REGBYTES(sp) # 从第 19 号槽位恢复保存寄存器
    LOAD x20, 20*REGBYTES(sp) # 从第 20 号槽位恢复保存寄存器
    LOAD x21, 21*REGBYTES(sp) # 从第 21 号槽位恢复保存寄存器
    LOAD x22, 22*REGBYTES(sp) # 从第 22 号槽位恢复保存寄存器
    LOAD x23, 23*REGBYTES(sp) # 从第 23 号槽位恢复保存寄存器
    LOAD x24, 24*REGBYTES(sp) # 从第 24 号槽位恢复保存寄存器
    LOAD x25, 25*REGBYTES(sp) # 从第 25 号槽位恢复保存寄存器
    LOAD x26, 26*REGBYTES(sp) # 从第 26 号槽位恢复保存寄存器
    LOAD x27, 27*REGBYTES(sp) # 从第 27 号槽位恢复保存寄存器
    LOAD x28, 28*REGBYTES(sp) # 从第 28 号槽位恢复临时寄存器
    LOAD x29, 29*REGBYTES(sp) # 从第 29 号槽位恢复临时寄存器
    LOAD x30, 30*REGBYTES(sp) # 从第 30 号槽位恢复临时寄存器
    LOAD x31, 31*REGBYTES(sp) # 从第 31 号槽位恢复临时寄存器
    LOAD sp, 2*REGBYTES(sp) # 最后恢复原 sp，此前的加载都依赖当前现场基址

    .endm # 汇编宏定义结束

    .globl __alltraps # 导出统一入口，供其他文件引用
    .align(2) # 将入口按 4 字节对齐
    # 中断和异常的统一入口：保存现场后，将 frame 地址通过 a0 传给 trap()。
    # C 函数完成分类和处理后返回这里，接着进入下方的现场恢复流程。
__alltraps:
    SAVE_ALL # 展开保存现场的指令

    #lab3 function1 2411080
    mv a0, sp # 将现场地址放入首参数寄存器，作为处理函数的参数
    call trap # 调用统一处理函数，分类并处理中断或异常
    # 函数返回后，sp 应恢复到调用前的位置，仍指向这份现场。

    .globl __trapret # 导出现场恢复入口
    # 返回原执行流：恢复现场后执行 sret，从 sepc 指定的位置继续执行。
    # sret 根据 sstatus 恢复权限级别和中断使能，本实验通常返回 S 态内核。
__trapret:
    RESTORE_ALL # 展开恢复现场的指令
    # 根据保存的状态和返回地址，从监管者模式的陷阱处理流程返回。
    sret # 从恢复后的返回地址继续执行，并恢复相应权限和中断状态
