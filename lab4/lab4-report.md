# Lab4 实验报告：进程管理

## 一、小组与实验目标

我们小组由组长李珉熠（2413982）、吴宇石（2411080）、赵亚鑫（2413715）组成。本次分工安排为：李珉熠负责页映射模块、集成验证与报告统稿；吴宇石负责线程创建及初始化模块；赵亚鑫负责调度与上下文切换模块。代码中保留课程 LAB4 标记，并分别填写相应成员学号。我们使用 AI 辅助分析、代码实现、故障定位与报告整理；这些分工用于小组复核与答辩准备，不将 AI 执行的操作虚构为各成员已独立完成的人工操作。

我们本次目标是让内核从“能够分配物理页、响应中断”进一步具备“维护虚拟页映射、创建内核线程、切换执行线程”的能力。我们完成三个必做模块，并单独回答两个 Challenge 思考题。

## 二、整体逻辑与运行环境

我们对整体执行顺序的理解是：先准备内存，再创建执行实体，最后交给调度器运行。实际链路为：

```text
kern_init -> pmm_init -> pic_init/idt_init -> vmm_init
          -> proc_init -> clock_init/intr_enable -> cpu_idle
          -> schedule -> proc_run -> switch_to
          -> forkret -> forkrets -> __trapret -> sret
          -> kernel_thread_entry -> init_main
```

前半段建立物理页、页映射和虚拟内存区域管理；后半段创建 idleproc 与 initproc，并使 initproc 真正得到 CPU。这里的线程仍在监管态运行，不是用户程序。

| 项目 | 本次环境或选择 |
| --- | --- |
| 实验目录 | 虚拟机 `/home/lmy/os/lab4` |
| 体系结构 | RISC-V 64 位，Sv39 分页 |
| 编译选项 | rv64gc、lp64d、medany、O2，保留调试信息 |
| 模拟平台 | QEMU virt，OpenSBI 固件 |
| 内存 | DTB 报告起点 0x80000000，大小 128 MiB |
| 底层分配器 | 沿用我们完成的 Best Fit |
| 小对象分配 | 本次框架自带 SLOB，为 PCB 等对象提供 kmalloc/kfree |

DTB 是描述硬件的二进制设备树；Best Fit 从足够大的空闲块中选择最小者；SLOB 是简单空闲块链表对象分配器。本次保留课程 SLOB，没有把它称作上次实验的 SLUB。PCB 是进程控制块，记录线程状态、栈、上下文和身份等信息。

## 三、功能模块一：虚拟页映射

### 3.1 需求规格与实现

我们的 spec 是：保持 page_insert/page_remove 接口；插入后页表项、引用计数与翻译缓存一致；允许同页重插和权限更新；替换映射时释放旧页的一次引用；删除无映射地址应安静返回；页表分配失败返回 -E_NO_MEM，并回滚本次新建的目录页。

page_insert 先调用 get_pte 定位叶项。成功后先增加目标页的引用。如果旧映射正好是同一页，就撤销这次多加的引用，避免错误释放；如果是不同页，先移除旧映射。最后写入新的物理页号、权限和有效位，并使对应地址的 TLB 失效。

page_remove 只查找、不创建页表。存在有效项时，先清项并刷新地址翻译，再减少物理页引用；引用降为零时才交回分配器。重复删除不会再次释放。

引用计数表示映射持有物理页的次数。PTE 是页表项，记录物理页号和权限；TLB 是 CPU 缓存近期地址翻译的结构。页表已改但缓存未失效时，CPU 可能继续使用旧翻译，因此我们使用 sfence.vma 对目标地址失效。

get_pte 还需要正确处理失败：若新建上级目录页后，下一级页表申请失败，我们清除新项、刷新并释放该目录页。对已经存在的大页叶项，我们返回 NULL，避免把实际数据页误当成下级页表。本次接口不负责拆分大页。

### 3.2 我们使用的提示词

```text
请基于现有 RISC-V Sv39 内核实现 page_insert 与 page_remove，保留函数签名、课程标记和 Page 布局。插入必须处理空项、同页重插、权限更新和替换其他物理页；引用计数不得重复增加或提前释放。删除无映射地址必须幂等。页表项变化后调用 tlb_invalidate。get_pte 创建中间页表失败时返回 -E_NO_MEM，回滚本次新建目录页，不能影响原有映射。中间项只置 PTE_V，叶项权限按参数保留，不擅自添加用户权限；遇到已有大页叶项不得当目录指针。用中文简要说明关键逻辑，保留课程原始测试。
```

叶项权限必须结合用途理解：课程部分检查只从软件读取页表项，传入 perm=0 或仅 PTE_W；这些组合不代表都能用于真实硬件访存。实际字符串别名检查使用 PTE_R | PTE_W。Sv39 可写叶项应同时可读，只有 V 而没有 R/W/X 的项会被解释为非叶项。

## 四、功能模块二：线程创建与初始化

### 4.1 核心函数

alloc_proc 用 kmalloc 分配 PCB 并整体清零，然后设为 PROC_UNINIT、PID=-1，默认页表指向启动根页表，同时初始化两个链表节点。零值使父线程、用户地址空间、运行次数和现场指针处于安全初始状态。

proc_init 初始化全局链表与 PID 哈希桶，把当前执行流包装为 idleproc：PID=0，状态可运行，复用 bootstack，设置 need_resched，并令 current 指向它。随后通过 kernel_thread 创建 initproc，查找并命名为 init。idleproc 不经 do_fork 创建，也不作为普通线程挂入 proc_list。

kernel_thread 构造临时 trapframe：s0 保存函数入口，s1 保存参数，epc 指向 kernel_thread_entry。status 设置 SPP，表示返回到监管态；设置 SPIE，令 sret 返回后开启中断；清除 SIE，保证现场恢复期间不提前开启中断。

do_fork 按“检查数量、分配 PCB、分配内核栈、处理地址空间、复制现场、登记并唤醒”的顺序组合资源。PID 分配、哈希挂接、进程链表挂接、计数与唤醒在关本核中断的临界区内完成。申请内核栈失败时释放 PCB；地址空间处理失败时先释放栈再释放 PCB，未完成的线程不会出现在全局链表中。

copy_mm 保留地址空间处理接口，本次断言 current->mm 为空，不复制用户地址空间；线程通过 PCB 的 pgdir 共用内核页表。copy_thread 把完整 trapframe 放到独立内核栈顶部，设置 a0=0、返回后使用的 sp，并把 context.ra 设为 forkret、context.sp 设为现场位置。

KSTACKPAGE=2，PGSIZE=4096，所以每个新内核线程有 8192 字节的栈。trapframe 有 36 个八字节槽位，共 288 字节。context 仅保存 ra、sp、s0 至 s11，共 14 个八字节字段，即 112 字节。

### 4.2 我们使用的提示词

```text
请在本次 proc.c 的 LAB4 function2 区域实现 alloc_proc、proc_init、kernel_thread、do_fork、copy_thread、copy_mm。不替换课程 PCB、trapframe、context 与已有辅助函数。idleproc 必须复用当前 bootstack，PID 为 0，current 指向 idleproc；创建 PID 为 1 的 initproc，参数为 Hello world!!。分配失败逐层清理，登记成功后才唤醒。copy_mm 只提供内核线程阶段接口，不虚构用户内存复制。按 s0=函数、s1=参数、epc=kernel_thread_entry 设置首次现场，context 通过 forkret、forkrets 和 __trapret 启动。分别解释 SPP、SPIE、SIE 和两个不同现场结构的用途。
```

### 4.3 首次启动为何能成立

switch_to 恢复新线程的 context 后，ret 从预设 ra 进入 forkret。forkret 将 current->tf 交给 forkrets；后者把 sp 指向该现场并跳到 __trapret。恢复寄存器、执行 sret 后，CPU 到达预设 epc，也就是 kernel_thread_entry。该入口把 s1 移到参数寄存器 a0，再调用 s0 中的函数，从而执行 init_main。

我们理解，首次切换的“旧现场”并非真的由新线程执行过，而是创建线程时提前布置好的启动现场。

## 五、功能模块三：调度与切换

### 5.1 核心处理逻辑

cpu_idle 持续检查 need_resched；需要调度时调用 schedule。schedule 先保存并关闭本核中断，清除当前线程的调度标记，从当前节点后方循环扫描可运行线程。遍历时跳过链表头；没有候选时使用 idleproc，选中后累计 runs 并调用 proc_run。

proc_run 在目标不是 current 时，进入中断保护区，记录旧线程、更新 current、写入目标页表、刷新 TLB，再调用 switch_to。切回旧线程时，switch_to 后的语句才继续执行，恢复该线程当时保存的中断状态。这里的函数调用会跨越一次线程切换。

我们按 `(8 << 60) | (pgdir >> 12)` 设置 satp。satp 是保存分页模式、地址空间编号和根页表物理页号的寄存器。模式值 8 表示 Sv39；右移 12 是把根页表的物理字节地址转换为 4 KiB 页号。本次未使用 ASID，即地址空间编号字段保持零。不能把根页表虚拟地址写入该字段，也不能调用框架遗留的 Sv32 lsatp。

switch_to 通过 a0 指向旧 context、a1 指向新 context，先保存 ra、sp、s0 至 s11，再恢复另一组同名寄存器，最后 ret。这些是调用约定要求跨函数调用保留的寄存器；其余临时或参数寄存器由调用方负责。本次正常切换发生在函数调用边界，因此不需要像任意时刻发生的 Trap 那样保存全部通用寄存器。

### 5.2 我们使用的提示词

```text
请实现 cpu_idle、schedule、proc_run 和 switch_to，保留所有现有接口。schedule 从当前线程之后循环查找 PROC_RUNNABLE，跳过表头，无候选则使用 idleproc；清 need_resched，增加选中线程 runs。切换 current、satp 和 context 时保存并关闭本核中断，返回后恢复原中断状态。使用 Sv39 MODE=8 和根页表物理页号构造 satp，不能用遗留 Sv32 lsatp。switch_to 只按 context 字段顺序保存/恢复 ra、sp、s0-s11，不实现调度策略。必须使新线程通过 forkret 恢复链首次运行，并通过课程多线程轮换测试。
```

本实验的调度是简单循环选择；课程测试线程主动设置 need_resched 并调用 schedule。我们没有把时钟计数功能宣称为已实现抢占式时间片调度，也没有增加后续实验的调度策略或完整退出回收。

## 六、实际遇到的问题与修复

| 问题与实际表现 | 我们的定位及处理 |
| --- | --- |
| 原包 default_pmm.c 缺少前序实验实现，free_list、nr_free 未定义 | 补入我们已有的完整实现，实际选择 Best Fit，未绕过编译错误 |
| kmalloc 中整数地址传给 kva2page，在 GCC 15 下报类型错误 | 增加显式 void 指针转换，不改变分配算法 |
| check_pgdir 通过，随后真实访问 0x100 导致再次进入启动路径 | GDB 观察 scause=15、stval=0x100，定位为存储页故障；去除中间项错误的 PTE_U，并把框架旧的中间用户位断言改为检查有效且用户位为零；课程 tools/test_lab4.c 保持原样 |
| 评分脚本出现回车字符和 shell 语法错误 | 文本统一改为 LF 换行后重新评分 |
| 课程测试函数为 void 返回，却传给接收 int 返回函数指针的接口 | 保留课程测试源码，以 -Wno-error=incompatible-pointer-types 将 GCC 15 的该诊断降为告警；没有改变正确接口。此测试函数一直轮换而不返回，仍保留类型不匹配的局限，不建议在真实项目照搬 |
| 原始 QEMU loader 启动方式及分页过渡不可靠 | qemu/debug/评分命令使用 -kernel；保留低地址临时恒等映射与高地址内核映射，确保启用 satp 后能继续取指 |

此外，我们沿用 Lab3 的现场保存恢复，保留 forkrets 供新线程首次进入。移除上次演示用的定次数自动关机行为，避免把 Lab3 的展示配置带入本次线程实验。

## 七、验证过程与结果

### 7.1 编译与课程评分

我们在虚拟机中执行：

```bash
cd ~/os/lab4
export PATH=/home/lmy/riscv-elf-toolchains/bin:$PATH
make -j4
make grade
```

最终实际结果：

```text
-check PMM:              OK
-check Process Create:   OK
-check Schedule:         OK
Total Score: 30/30
```

课程 PMM 测试检查新映射、替换、同页权限更新、重复删除和引用计数；线程创建测试检查 idle/init 的身份、状态、父子关系及首次上下文；调度测试再创建两个线程，主动多次切换，确认两者实际运行。三个测试各 10 分，测试源码与注入器均来自课程。

我们另加了内存不足回滚自检：分配目标页后保留其余大部分内存，只留一页；对空根目录的地址插入映射需要两页页表，所以第二次分配失败。断言返回 -E_NO_MEM、根项恢复为零、目标引用仍为零、空闲页数恢复为一；最后释放保留内存，核对总数恢复。实测输出 `[Extra PMM] allocation failure rollback: PASS`。

### 7.2 正常运行结果与边界

执行 make qemu，页表自检、SLOB 初始化及虚拟内存区域自检均通过，然后打印：

```text
this initproc, pid = 1, name = "init"
To U: "Hello world!!".
To U: "en.., Bye, Bye. :)"
```

随后框架的 kernel_thread_entry 调用 do_exit，而本次提供的 do_exit 仍是 报告 process exit!! 的 panic 调用，所以进入内核调试监视器。这是框架的退出占位行为，不能据此宣称完整进程退出、资源回收或父进程等待已经实现。我们保留了这个边界。

### 7.3 GDB 观察首次切换

终端 A 在实验目录执行 make debug；终端 B 在同目录执行 make gdb，然后逐条输入：

```text
hbreak switch_to
continue
p current->pid
set $ctx = (struct context *)$a1
p $ctx->ra == (uintptr_t)forkret
p (((unsigned long)$satp >> 60) & 15) == 8
hbreak init_main
continue
p current == initproc
p $sp >= current->kstack && $sp < current->kstack + 8192
detach
quit
```

首次 switch_to 处 current->pid 为 1，新 context 的 ra 等于 forkret；到 init_main 后 current 等于 initproc，sp 位于该线程的独立内核栈内。临时变量从入口参数寄存器 a1 取得，避免直接读取被 O2 优化掉的 C 局部参数。检查 satp 时先转无符号数，避免最高位为一而算术右移带来的误判。

本次日志留在虚拟机 evidence 目录，GitHub 只提交源码和报告。测试证明上述指定场景通过，并不代表多核、用户态隔离或所有资源耗尽路径均已覆盖。

## 八、Challenge 一：保存和恢复中断状态

local_intr_save 通过 __intr_save 读取 sstatus.SIE。原先开启就关闭并返回 1；原先关闭就返回 0。local_intr_restore 仅在保存值为 1 时重新开启，从而不在内层临界区结束时提前解除外层保护。

宏用 do-while(0) 把操作包装成单条语句：先执行一次，再因条件为零结束，方便放在 if/else 内。需要保护的内容包括空闲链表、线程登记，以及 current、页表、寄存器上下文的一致性。每层使用自己的保存变量，按嵌套顺序恢复。

我们理解，“关中断”只影响本核的可屏蔽中断响应；不能阻止同步异常，也不能替代多核锁。应缩短关中断区间，降低中断延迟。

## 九、Challenge 二：分页模式与 get_pte 设计

### 9.1 两段遍历代码为何相像

| 模式 | 虚拟地址有效位 | 层数与每级索引 | 页表项与表大小 |
| --- | --- | --- | --- |
| Sv32 | 32 位 | 两级，每级 10 位 | 4 字节，每表 1024 项，共 4 KiB |
| Sv39 | 39 位规范地址 | 三级，每级 9 位 | 8 字节，每表 512 项，共 4 KiB |
| Sv48 | 48 位规范地址 | 四级，每级 9 位 | 8 字节，每表 512 项，共 4 KiB |

三者基本页的页内偏移均为 12 位，即 4 KiB。Sv39/Sv48 的高位须按最高有效地址位符号扩展，并非任意高位组合都有效。

我们把两段代码对应到 Sv39 的实际查找路径。第一段用 `PDX1(la)` 选根表的 `upper` 项，取得中间页表。第二段用 `PDX0(la)` 选中间表的 `lower` 项，取得最低级页表。最后用 `PTX(la)` 返回最低级页表项的地址，由 `page_insert()` 写入映射，或由 `page_remove()` 清除映射。

两段代码相像，是因为每向下一层都执行同样的操作。先检查目录项的 `PTE_V`。如果下一层表不存在且允许创建，就申请并清零一页，用其物理页号和 `PTE_V` 填写目录项。之后用 `PDE_ADDR()` 取出下一级表的物理基址，再用 `KADDR()` 转成内核可访问的虚拟地址。完整走到最低级时，Sv32 走一次这样的目录跳转，Sv39 走两次，Sv48 走三次，因此都可以用按层循环表达。

Sv39 的叶项还能提前出现在高层，分别形成 1 GiB 或 2 MiB 大页，需要满足相应物理地址对齐要求。本次启动高地址映射使用 1 GiB 叶项，get_pte 的普通映射接口只维护三级遍历后的 4 KiB 映射，不能把大页误当页表页。

### 9.2 查找与分配是否应拆分

我们认为这次实验没必要拆成两份遍历代码。`page_remove()` 传 `create=0`，只查已有页表，缺失就返回。`page_insert()` 传 `create=1`，允许补建缺失的中间页表。两者共用索引和逐层检查逻辑，接口也足够清楚。

创建路径的代价是要处理部分成功。比如只剩一页时，第一段可能建好中间表，第二段却建不了最低级页表。我们的实现会清除本次写入的根表项并释放刚申请的页，使失败前后的页表和空闲页数量一致，第七节的回滚自检验证了这一点。若以后要支持并发修改或大页拆分，我们倾向把对外接口分为只读查找和按需创建，让创建接口负责加锁与回滚，内部仍复用逐层遍历。

## 十、与操作系统原理的对应及尚未覆盖内容

进程控制块对应抽象的执行实体；独立内核栈保存每个线程的调用链；context 保存切换必需的调用现场；trapframe 保存中断或首次启动需要的完整现场。调度决定谁运行，上下文切换执行选择结果，二者职责不同。

虚拟内存区域 VMA 描述地址区间及用途，PTE 负责具体页映射，物理页分配器负责底层空间，TLB 缓存翻译结果。它们共同实现内存管理，但不是同一种数据结构。

本次所有线程共用内核地址空间，仅实现基础可运行线程选择，没有用户态进程隔离、独立用户页表、按需缺页分配、写时复制、完整退出等待、时间片抢占、多核同步、优先级或公平性保障。我们把这些作为后续实验与原理学习的内容。

## 十一、参考资料与复现位置

我们使用课程 Lab4 原始代码包、[Lab4 练习](http://oslab.mobisys.top/lab2026/_book/lab4/lab4_2_1_exercises.html)和[报告要求](http://oslab.mobisys.top/lab2026/_book/lab4/lab4_5_labs_requirement.html)，并沿用前序实验完成的物理页管理与 Trap 机制。

仓库 lab4 目录包含代码、本文 Markdown、南开模板生成的 LaTeX 与 PDF。Markdown 为正文来源，两版正文同步生成。AI 协助形成需求、实现与验证记录；我们需在答辩前按分工复核代码与原理，不能用测试通过替代理解。
