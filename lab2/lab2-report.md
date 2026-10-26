# 操作系统 Lab2 实验报告：物理内存与页表

## 实验基本信息与小组分工

实验日期：2026-10-23。我们使用的代码来源为：[课程 labcode](http://oslab.mobisys.top/lab2026/labcode.html) 的 lab2.zip。我们保留了课程自带 Best-Fit 两个测试函数和评分测试源码，关键测试结果与调试数值记录在本报告中，完整运行日志保留在本地。

| 成员 | 模块与报告分工 |
| --- | --- |
| 2413982 李珉熠（组长） | 环境、物理内存初始化、构建与评分、汇总报告 |
| 2411080 吴宇石 | First/Best-Fit、Buddy 与简化 SLUB 算法及测试分析 |
| 2413715 赵亚鑫 | 分页切换、内核 GDB 与 QEMU 源码调试、证据整理 |

我们按以上模块组织小组分工，并使用 Codex 作为 AI 辅助工具，协助阅读框架、实现算法、定位错误和整理文档。我们以实际编译、评分和调试输出核对实验结果，并分别理解和复现所负责的内容；报告中的实现与测试结论以实际运行记录为依据。

## 一、实验目的

1. 从 DTB 获取 RAM 范围，建立 Page 描述符，隔离固件、内核、元数据和 DTB 占用。
2. 理解 First-Fit，完成 Best-Fit；从零实现 Buddy 和简化 SLUB，并验证分配、释放及回收。
3. 理解 Sv39、SATP、叶子 PTE、TLB 与页表遍历；同时调试客体内核与宿主 QEMU。

## 二、实验环境与整体逻辑

我们的实验环境为 Windows 下的 VMware Ubuntu 24.04，RISC-V GCC 15.1.0、GNU make、系统 QEMU 8.2.2、OpenSBI v1.3、gdb-multiarch。我们还在个人目录中构建了 Ubuntu 源码包中的 QEMU 8.2.2 调试版，用于宿主源码调试；构建后直接运行该二进制，没有执行 make install。

```text
QEMU 预装固件/镜像 → ROM → OpenSBI → entry.S
→ 建立 Sv39 映射、写 satp、sfence.vma → 高地址 kern_init
→ DTB 解析 → 选择 pmm_manager → page_init → 分配器 check
→ SLUB 对象测试 → 无限循环
```

内核链接 VA 起点为 0xffffffffc0200000，加载 PA 为 0x80200000，偏移量为 0xffffffff40000000。VA=PA+offset；PADDR 进行反向转换。在实现和调试中，我们分别区分 Page *（描述符指针）、物理地址 PA、虚拟地址 VA 和物理页号 PPN。

我们的实验目录为 `/home/lmy/os/lab2`，工具链位于 `/home/lmy/riscv-elf-toolchains/bin`。

## 三、实验内容与实现

### 3.1 物理内存初始化 page_init

我们实现了接口 `static void page_init(void)`。该函数无参数、无返回值，通过全局量 pages、npage 和 va_pa_offset 提供初始化结果。

1. 验证 DTB 的 base、size、加法溢出；当前框架 page2ppn 固定使用 DRAM_BASE，因此明确拒绝其他基址，避免悄悄错误映射。
2. 设置 va_pa_offset，取设备树上界和 PHYSICAL_MEMORY_END 的较小值，再向下按 4096 字节对齐。
3. npage 是管理上界的绝对物理页号，不是 RAM 页数；描述符数量为 npage-nbase。
4. 将描述符数组放在链接符号 end 之后并按页向上对齐，先检查整个数组能否放入受管 RAM；每个描述符先标 reserved、ref=0。
5. 空闲起点放在描述符数组之后，向上对齐。保留 OpenSBI、内核及元数据。当前 DTB 在 RAM 顶部附近，为避免覆盖，保守保留从 DTB 所在页至管理上界的尾段；这是安全但可能浪费部分内存的策略。
6. 仅当存在完整空闲页时调用 init_memmap。框架要求 n>0，因此没有空闲整页时不调用它。

我们在默认 128 MiB RAM 下的实测结果为：管理起点为 `0x80000000`，上界为 `0x88000000`，npage-nbase=32768，sizeof(Page)=40；描述符 PA=0x80207000；完整空闲页区间从 `0x80347000` 到 `0x87e00000`（不含上界），共 31417 页。64 MiB 实测自动缩小管理上界；256 MiB 实测按框架 0x88000000 上界裁剪。评分脚本关于 npage 的预期针对默认内存，不能把默认 20/20 推广为任意内存配置的评分结论。

### 3.2 First-Fit 的理解及改进

我们首先分析了课程提供的 First-Fit：default_init 初始化双向链表和空闲计数；init_memmap 将可用区间组成块，头页 property 保存块长。alloc_pages 从链表头开始选第一块长度≥n 的块，分割余块；free_pages 按地址插入，并与前后物理相邻块合并。

查找时间最坏 O(空闲块数)，地址有序链表便于合并，但不保证最小碎片。可使用分桶、平衡树、位图或最近搜索位置改善搜索成本；也可减少频繁分割。不能保证任何策略都能消除外部碎片。

### 3.3 Best-Fit

我们实现 Best-Fit 时复用框架约定的地址有序空闲链表；遍历所有满足 property≥n 的块，选择 property 最小者。同长时保留首先遇到的低地址块。分配后移除原头、为剩余块设置新头，减少 nr_free；释放时插入并合并相邻空闲块。init_pmm_manager 默认选择 best_fit_pmm_manager。

我们修正了参考链表插入逻辑时发现的尾插入风险：在尾部插入后增加 break，避免循环继续访问刚插入的节点甚至重复插入。除课程原测试外，增加32页隔离池测试：分配32个单页、耗尽返回NULL、释放偶数页形成碎片时不能分配2页、释放奇数页后重新合并成32页，最终计数恢复。

原 basic_check 与 best_fit_check 内容保持不变；新增 best_fit_verify 包装调用二者之外的附加测试。普通运行和注入评分都真正使用 Best-Fit。改进可采用按长度索引的树/分桶，同时保留按地址合并结构；当前线性查找为 O(k)，仍可能制造小碎片。

### 3.4 Buddy System

我们新增 buddy_pmm.c/h，使用0到15阶空闲链表，每阶块大小为2^order页。初始化把任意长度初始区间拆成物理页号对齐的二次幂块。申请 n 页向上取整到最小二次幂，从足够大的一阶拆分；伙伴页号由 ppn XOR (1<<order) 得到。释放仅合并同阶、真正的伙伴，不能仅凭地址相邻就合并。

我们的 alloc_pages(3) 实际占4页；释放时使用原始申请长度3，使 free_pages 按同样的阶数回收。各阶头标记由 PageProperty 与 property 保存；available 统计实际剩余页数。我们通过 `make PMM=buddy` 切换管理器，并在切换前执行 make clean，避免复用另一配置的目标文件。

测试覆盖：零长度和超最大阶请求、3页向4页取整、4页/8页对齐、并存块无重叠、释放前后计数恢复；隔离一个8页块，拆成8个单页至耗尽，逆序释放，重新申请8页必须恢复原块。这能检查分裂、伙伴合并和计数，而不仅是“申请返回非空”。

### 3.5 简化 SLUB 对象分配

我们新增 kmalloc(size_t n)、kfree(void *p)、slub_check()。这是体现 SLUB 主体思想的单CPU教学实现，不等同于 Linux 生产级 SLUB。

下层使用当前 pmm_manager 的页分配；上层提供16、32、64、128、256、512、1024字节七种缓存。每个slab一页，页首为元数据，后面切分为固定长度对象；空闲对象首部存放下一个空闲对象指针。对象16字节对齐。超过1024字节走连续页大对象路径，申请页数为 ceil((header+n)/4096)，最大接受1 MiB。大对象仍保留首部元数据，使 kfree 能还原页及长度。

kfree(NULL) 无操作。释放小对象时检查页头、位置、对齐及重复释放，把对象放回空闲链；整slab空闲后从缓存链移除并归还底层页。Buddy 模式下底层页请求可能取整，但仍保存原请求页数，以相同请求释放。

测试覆盖各大小类边界1/16/17/32/33/.../1024/1025、4096和9000字节，逐字节写入和核对防止重叠，逆序释放后空闲页计数恢复；300个32字节对象跨多个slab，验证数据、同一可用slab中的对象复用及所有页回收。我们在早期测试中错误假定任意slab释放的对象都是下次首选；核对缓存链选择顺序后，我们改为释放当前优先缓存slab中的对象再检验复用。

### 3.6 任务规格与 AI 辅助过程

我们在持续协作中使用 Codex 辅助阅读、编码、排错和解释，围绕接口、约束与验收标准组织任务。以下提示词是我们对协作需求的整理，便于复现实现和测试思路。

**物理页初始化与 Best-Fit。** 我们要求基于 DTB 初始化 pages、npage 和 va_pa_offset，检查溢出及内存边界，保留固件、内核、描述符和 DTB 占用；Best-Fit 遍历全部合适块选择最小者，保持地址有序及相邻块合并。我们保留课程 basic_check、best_fit_check 与评分测试，另加耗尽、碎片和合并测试。

**Buddy 实现提示。** “实现独立 pmm_manager：0到15阶，每块为2^order页；初始化按绝对物理页号对齐分解区间；申请向上取整并逐阶拆分，释放按 ppn XOR (1<<order) 寻找同阶伙伴并递归合并。支持 PMM=buddy，明确原始申请长度的释放约定。”

**Buddy 测试提示。** “覆盖零与过大请求、3页申请实际占4页、对齐和不重叠；隔离8页，拆成8个单页至耗尽，逆序释放后重新申请完整8页，恢复原状态并核对空闲计数。”

**SLUB 实现提示。** “在页分配器上实现 kmalloc/kfree，提供16到1024字节七种缓存，每slab一页、对象16字节对齐、空闲对象内嵌next指针；大对象以带头部的连续页分配，限制1MiB，完全空闲时归还页，兼容Best-Fit和Buddy。”

**SLUB 测试提示。** “覆盖大小类边界、零与过大请求、跨页大对象；逐字节检查数据，300个32字节对象跨多个slab，检查当前可用slab内的对象复用，全部释放后核对底层页数，在两种页分配器下分别运行。”

**QEMU 调试提示。** “在个人目录构建带符号的QEMU，用宿主GDB运行模拟器，客体GDB连接stub；分别观察kern_init取指和boot_dtb数据读，检查TLB miss、页表项读取与最终物理地址，排除is_debug访问，并对照Bare与Sv39。”

我们通过实际构建与运行核对 AI 辅助生成的代码，发现不符合框架或测试前提的内容后继续修改；测试结果以第五节的实际验证范围为准。

我们在实验中实际修复的问题包括：QEMU 通用loader缺少入口交接信息，改用-kernel；入口缺失低地址过渡映射，增加顶层第2项恒等映射；工具链明确rv64gc/lp64d并改正旧sfence.vm别名；课程脚本CRLF在sh下报错，统一LF；Best-Fit尾插入增加break；纠正SLUB复用测试前提；QEMU源码包缺少可安装BIOS资源时使用disable-install-blobs，运行时-L /usr/share/qemu复用系统固件。

## 四、分支任务：实际调试 QEMU 的 MMU 路径

### 4.1 调试架构

我们用客体 gdb-multiarch 经 QEMU stub 观察 RISC-V 寄存器和内核，用宿主普通 gdb 运行 x86-64 的 QEMU 进程，并在其C源码中设置断点。宿主GDB作为QEMU父进程，不需要sudo attach，也避免ptrace限制。QEMU调试版位于 /home/lmy/oslab-tools/qemu-ubuntu-build/qemu-system-riscv64，源码在相邻qemu-source。我们分别检查了内核启动、页表设置、取指和数据读取的翻译路径。

### 4.2 SATP 与大页的客体观察

我们在客体 GDB 中读到 SATP=0x8000000000080205：MODE=8表示Sv39，ASID=0，根表PPN=0x80205，即物理地址0x80205000。根表第511项位于0x80205ff8，PTE=0x200000cf；恒等映射第2项也为相同PTE。

PTE的PPN=0x80000，对应PA基址0x80000000；低位0xcf表示V/R/W/X/A/D为1、U/G为0。根表叶子映射1 GiB。三级页表是最多三级查询，遇到叶子可提前结束。本实验两项覆盖低地址0x80000000起1 GiB，以及高地址0xffffffffc0000000起1 GiB。真实受管RAM只有其中一部分，大页映射范围不等于可分配RAM范围。

### 4.3 宿主断点与真实翻译

我们分别选择 kern_init 的取指和 boot_dtb 的数据读取做两次独立运行。宿主条件断点位于 riscv_cpu_tlb_fill、get_physical_address、PTE读取后的分支和tlb_set_page前。is_debug=0用于排除调试器自身的内存查询。

我们记录到以下关键输出：

```text
WALK_ENTER addr=0x80200000 satp=0 is_debug=0
WALK_ENTER addr=0xffffffffc02000d6 satp=0x8000000000080205 is_debug=0
PTE_READ ... i=0 ptshift=18 base=0x80205000 pte_addr=0x80205ff8 pte=0x200000cf
TRANSLATED VA=0xffffffffc02000d6 PA=0x802000d6 prot=7 cached_size=0x1000
TLB_MISS address=0xffffffffc0206008 access=0 mmu_idx=1
TRANSLATED VA=0xffffffffc0206008 PA=0x80206008 prot=7 cached_size=0x1000
```

取指access=2，数据读取access=0，mmu_idx=1对应此次S模式访问。数据读调用栈真实出现 do_ld8_mmu → mmu_lookup → mmu_lookup1 → tlb_fill → riscv_cpu_tlb_fill。

### 4.4 页表遍历与TLB原理

QEMU 8.2的 target/riscv/cpu_helper.c 中 get_physical_address 先检查权限模式和SATP模式；本次Bare阶段直接使用原地址。Sv39时以根表物理地址为base，计算当前层index和pte_addr=base+index*8，使用address_space_ldq读取8字节PTE，再检查合法性。V=0失败；R/W/X有叶子权限则转到leaf；否则提取PPN，将base更新为下级表物理地址，继续循环。因此循环的职责是层级遍历，不是重复猜地址。框架顶层即为叶子，所以这次循环i=0时结束。

此处ptshift=18是还要保留的下级VPN位数，两层各9位，共18位；不是虚拟地址总偏移。1 GiB叶子的物理地址由PPN高位、虚拟地址的低级VPN和12位字节偏移组成，等价于 PA=0x80000000+(VA & 0x3fffffff)。对数据读得到0x80206008。

软件TLB查找位于accel/tcg/cputlb.c。mmu_lookup1读取并比较TLB标签，不命中时先尝试victim_tlb_hit，再通过tlb_fill调用架构的riscv_cpu_tlb_fill；成功后tlb_set_page缓存结果。取指另由get_page_addr_code_hostp/probe_access_internal路径进入。get_physical_address不是每次访问必经函数：命中缓存时可以绕过页表遍历，TCG翻译块缓存也会影响取指路径。

QEMU软件TLB承担客体地址转换及访问权限缓存，并保存加速宿主访问需要的信息；它不等于实体RISC-V处理器某一型号的硬件TLB，也不等于宿主x86 CPU自己的TLB。本次大页翻译虽然使用1 GiB叶子，软件缓存填充实测cached_size为0x1000，不能把二者混为相同页大小。进入内核前Bare模式虽然不做Sv39遍历，QEMU仍可能调用软件TLB填充，这正是日志中SATP=0时也有TLB_MISS的原因。

## 五、测试结果

| 测试 | 实测结果 | 验证内容 |
| --- | --- | --- |
| 课程 page_init 与 Best-Fit 评分 | 20/20 | 默认128MiB配置下的课程评分 |
| Best-Fit原测试与附加32页碎片/合并 | PASS | 耗尽、连续性、释放后合并和计数恢复 |
| Buddy取整/对齐/耗尽/合并 | PASS | 非二次幂申请、拆分及伙伴回收 |
| SLUB尺寸边界/数据/多slab/复用/回收 | Best-Fit、Buddy两种下层均PASS | 数据不重叠、对象复用、底层页归还 |
| 64MiB、256MiB启动与内存边界 | 启动及测试成功 | 64MiB自动缩小，256MiB按128MiB上界裁剪 |
| 客体GDB与SATP/PTE | 高地址断点及根表值已确认 | SATP=0x8000000000080205，PTE=0x200000cf |
| 宿主QEMU取指与数据读翻译 | 条件断点命中并确认PA | 分别得到PA=0x802000d6与0x80206008 |

我们在限时验证中使用 timeout 结束运行，在宿主跟踪完成后由调试脚本主动结束 QEMU，因此相应的 timeout 和 killed 信息属于验证流程结束。我们在本地保留了真实文本日志；测试范围为上表所列场景，尚未覆盖生产级并发、任意恶意 DTB 和全部硬件配置。

## 六、总结、理论联系与扩展思考

页级分配器解决“哪些连续物理页可以使用”，SLUB解决“如何把页切成对象”，页表解决“CPU地址如何翻译”，TLB解决“如何缓存翻译”；这些是不同层次。reserved防止把固件/内核/描述符作为空闲内存发出；property只在空闲块头解释，不能拿任意页的值当块长。

未知硬件RAM范围可通过DTB、UEFI内存映射或平台固件协议获取，并结合保留区域及硬件窗口；本框架DTB解析只识别一个memory节点的reg，不应盲探任意物理地址，也不宣称支持多段RAM和完整reserved-memory语义。未来可扩展多个物理区间、FDT保留表与启动参数保留区。

我们本次尚未实现进程地址空间隔离、按需分页、页面置换、系统调用、调度、文件系统，以及多核/中断环境下的分配器同步。过渡恒等映射和宽权限1GiB启动页适合教学启动，不是最终内核应长期采用的最小权限布局。

通过本次实验，我们完成了可管理物理页的建立、Best-Fit接入、Buddy和对象分配，并在QEMU源码中观察了真实地址转换的关键路径。

参考：[课程练习](http://8.135.34.58/lab2026/_book/lab2/lab2_2_1_exercise.html)、[分支任务](http://8.135.34.58/lab2026/_book/lab2/lab2_4_qemu_mmu_debug.html)、[QEMU v8.2.2 MMU源码](https://github.com/qemu/qemu/blob/v8.2.2/target/riscv/cpu_helper.c)、[QEMU软件TLB](https://github.com/qemu/qemu/blob/v8.2.2/accel/tcg/cputlb.c)。我们实际调试时采用Ubuntu重打包的8.2.2源码。

## 七、关键代码的调试验证

我们进一步通过实际`make gdb`入口检查启动状态，并用断点观察分配算法的区别。`make gdb`内部调用工具链GDB加载符号、设置RISC-V架构并连接1234端口；我们读到的SATP和PTE与前述数值一致。

我们在best_fit_check第237行申请1页之前，通过GDB确认两个空闲块相对p0偏移分别为1和4，长度分别为2和1，总空闲3页；执行该行后p1-p0=4、剩余2页。因此第239行assert(p0 + 4 == p1)明确验证最小合适块选择，与First-Fit的首次合适块选择不同。我们还通过GDB确认描述符大小为40字节，page_link偏移为24字节。

Page管理标记不是PTE硬件权限；单张Sv39表为4KiB、512项，而完整多级结构按实际映射分配；启动根表使用1GiB叶子所以不走满三级。测试断言属于覆盖特定行为的证据，不能代替任意非法释放或并发正确性的证明。这些观察帮助我们把分配策略、链表结构和实际执行结果对应起来。
