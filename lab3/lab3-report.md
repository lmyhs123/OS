# 操作系统 Lab3 实验报告：中断与异常处理

## 一、基本信息与分工

实验日期：2026-11-02。我们基于课程lab3.zip完成实现，使用Codex辅助分析、实现、排错和文档整理，以实际编译、QEMU运行及GDB观察核对结果。以下为小组模块分工。

| 成员 | 模块 |
| --- | --- |
| 李珉熠 2413982（组长） | 环境、Lab2移植、构建与报告汇总 |
| 吴宇石 2411080 | Trap入口、汇编恢复和布局分析 |
| 赵亚鑫 2413715 | 中断异常处理、评分与调试分析 |

## 二、目的、环境与整体逻辑

我们使用Ubuntu虚拟机、RISC-V GCC 15.1.0、QEMU 8.2.2和OpenSBI 1.3，实验目录为/home/lmy/os/lab3。

Trap是中断与异常的统称。中断是与当前指令执行异步的事件，例如定时器到期；异常由当前指令执行引发，例如非法指令或断点。上下文是被打断时的寄存器状态，trapframe是保存在栈上的现场结构。

整体顺序如下：

```text
初始化DTB和物理内存 → 设置stvec统一入口
→ 设置时钟事件和中断使能 → 发生Trap
→ __alltraps / SAVE_ALL → trap(tf) → trap_dispatch
→ interrupt_handler或exception_handler
→ __trapret / RESTORE_ALL → sret返回原执行流
```

stvec保存S态Trap入口地址；S态为监管者模式，即本次内核运行的权限级别。硬件设置sepc、scause及相关状态，通用寄存器则由入口汇编保存，不能认为硬件自动完成全部现场保存。

## 三、Lab2基础与启动兼容

我们移植page_init和Best-Fit，保留Lab3中alloc_pages、free_pages和nr_free_pages的局部中断保护包装。分配前记录原中断状态并关闭本核中断，操作后恢复原状态，不能无条件重新开中断。这避免本核中断处理打断链表修改，但不提供多核互斥。

物理内存通过DTB取得，描述符与保留区沿用Lab2设计。我们补充PHYSICAL_MEMORY_END=0x88000000、KERNEL_BEGIN_PADDR=0x80200000两个移植需要的常量。QEMU加载改用-kernel，启动根表增加低地址过渡映射；工具链明确-march=rv64gc、-mabi=lp64d，并使用LF换行。

## 四、模块一：Trap入口和现场恢复

我们补全trapentry.S中的RESTORE_ALL及__alltraps调用部分，保留SAVE_ALL布局、trapframe结构和trap函数签名。

RV64下每个槽位8字节，32个通用寄存器槽位加4个控制状态槽位，共36×8=288字节。控制状态依次是sstatus、sepc、stval的保存值和scause。CSR是控制与状态寄存器；stval提供异常附加信息，scause记录原因，sepc记录返回执行位置。

SAVE_ALL先把原sp放到sscratch，再将sp下移288字节，原sp最终保存在第2号槽位。sp是栈指针，sscratch是辅助暂存CSR。因此我们先读出并恢复sstatus、sepc，再恢复通用寄存器，最后恢复sp，保证所有现场读取仍以当前frame为基址。cause和badvaddr供处理与诊断，不必像sepc一样写回才能返回。

__alltraps用mv a0,sp将frame指针作为第一个参数，call trap调用C函数，函数返回后进入__trapret。a0是RISC-V调用约定中的首参数寄存器，ra保存函数返回地址；旧a0、ra已在frame中，最终一并恢复。

sret是S态Trap返回指令，根据SPP恢复特权级、根据SPIE恢复中断使能，并将sepc作为继续执行的位置。SPP为之前的权限级别，SPIE为之前的中断使能状态。本实验主要由S态返回S态，不是每次都返回用户态。

## 五、模块二：中断与异常处理

### 5.1 时钟中断

trap_dispatch通过cause最高位区分中断和异常，我们在interrupt_handler中清除该位后按原因分类。S态时钟原因编号为5。

每次时钟中断调用clock_set_next_event，ticks加1；当ticks是TICK_NUM=100的整数倍时调用既有print_ticks。clock_set_next_event通过SBI接口设置未来事件；SBI是内核请求M态固件服务的接口。timebase=100000配合10MHz时间计数器，约每10ms设置下一次事件，实际间隔受处理开销影响，不能等同精确CPU周期。

指导书引用SHUTDOWN_AFTER，但下载框架未给出定义或明确数值。我们在允许实现区域定义可覆盖默认值10，含义是打印10次100 ticks后调用sbi_shutdown，而非声称老师规定必须10次。正常运行预期1000次中断后关机；时钟评分在200次时已验证持续返回。

其他软件、定时和外部中断按类型输出提示；未知原因打印编号及frame。外部中断来自设备，软件中断用于软件请求，本实验不实现完整设备中断应答协议。

### 5.2 非法指令与断点异常

我们识别CAUSE_ILLEGAL_INSTRUCTION和CAUSE_BREAKPOINT，输出Illegal instruction at或Breakpoint at。读取epc指向指令的低16位：低2位不全为1，按2字节跳过；否则低5位不全为1，按4字节跳过；更长编码在本教学实现中停止并报告不支持，不猜测跳过。

这适用于本实验RV64GC的16/32位指令。若不修改epc，返回后会再次执行同一异常指令，形成循环。异步时钟中断不主动增加epc，以免跳过正常代码。其他异常按框架要求打印frame，不宣称已经恢复缺页等故障；当前读取故障指令的方式也不适用于任意不可访问epc。

## 六、Spec与两份Prompt

我们将本次协作需求整理为下面两份提示，AI作为辅助，生成结果通过框架约束和实测核对。

### 6.1 模块一Spec与Prompt

Spec：输入现有SAVE_ALL和36槽位frame；输出与其严格对应的恢复汇编，以及将frame传入trap的调用逻辑；不得修改保存布局、结构体或函数签名。

Prompt：阅读trap.h、trapentry.S和RISC-V调用约定，仅补全RESTORE_ALL和__alltraps留空处。逐项列出32个通用寄存器及4个状态槽位的偏移，识别原sp经sscratch保存到第2槽位的特殊关系。先恢复返回需要的CSR和通用寄存器，最后恢复sp，避免破坏frame访问基址。以a0传入当前frame，调用trap后接入既有__trapret和sret；保持16字节栈对齐。解释每步为何与SAVE_ALL对应，并保留lab3注释、替换学号标记。

### 6.2 模块二Spec与Prompt

Spec：接收frame，根据原因分类；定时器持续重设和计数；异常按实际16/32位长度推进epc；保留其余框架函数和课程评分条件。

Prompt：在trap.c规定区域实现interrupt_handler和exception_handler。时钟事件每次重设，ticks每次加1，每100次使用已有函数输出，定义并说明可配置关机次数默认值，保证评分可到200次。其他中断分类输出，未知原因打印frame。非法指令和断点按要求输出地址，通过指令低位编码判断2/4字节长度后推进epc，未知异常仅打印现场，异步中断不增加epc。使用现有sbi_shutdown；不要修改trap_dispatch、trap签名、SAVE_ALL或课程测试。

## 七、实际排错与验证

我们编译时实际遇到两处错误：当前汇编器拒绝旧CSR名sbadaddr，因此在汇编文件头用预处理别名映射到同一CSR的现代名称stval，保持SAVE_ALL主体不变；移植page_init后缺少两个内存布局常量，核对Lab2与Lab3映射后补充。启动加载参数和过渡映射问题则是根据前次实验经验预先适配，没有将其写成本次已经出现的运行故障。

课程测试源码和注入脚本保留原样，评分脚本只适配QEMU加载方式，未改变通过条件及分数。

| 验证 | 结果 | 能支持的结论 |
| --- | --- | --- |
| 现场恢复及断点返回 | 35/35 | 课程预置的多组寄存器经ebreak后保持正确 |
| 非法指令及兜底处理 | 25/25 | 4字节非法指令可返回，软件中断与未知原因输出符合要求 |
| 持续时钟 | 40/40 | 初始化计数、重新设定和反复返回支持运行到200次 |
| 总分 | 100/100 | 本次课程三项检查均通过 |
| 普通启动 | 10次100 ticks后正常退出 | 非评分构建的时钟与关机路径可运行 |
| 客体GDB | frame=288字节，cause=0x8000000000000005 | 首次时钟进入trap时ticks=0，之后进入__trapret |

完整的16位非法指令、全部扩展指令、任意嵌套异常和用户态栈切换不在上述评分覆盖中，不能凭100分推断全部正确。我们将实际启动和GDB记录留在本地，GitHub只保留代码与报告。

## 八、原理联系、局限和总结

本实验中的现场保存与恢复对应操作系统的上下文管理，但尚未切换进程；时钟提供调度的基础事件，但本次没有进程调度器；非法指令处理体现同步异常分发，但按需分页、页面置换和用户系统调用尚未实现。关闭本核中断保护临界区不等于多核锁，volatile使ticks按要求进行可见访问，也不等于原子读改写。

通过本次实验，我们接通了从汇编入口、C处理函数到sret返回的完整链路，理解了硬件记录控制状态与软件保存通用寄存器的分工。

参考：[课程练习](http://oslab.mobisys.top/lab2026/_book/lab3/lab3_2_1_exercise.html)、[Trap入口](http://oslab.mobisys.top/lab2026/_book/lab3/lab3_3_2_trapentry.html)、[时钟](http://oslab.mobisys.top/lab2026/_book/lab3/lab3_3_4_clock.html)、[报告要求](http://oslab.mobisys.top/lab2026/_book/lab3/lab3_4_lab_requirement.html)。
