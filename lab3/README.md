# Lab3：中断与异常处理

我们小组成员为李珉熠2413982、吴宇石2411080、赵亚鑫2413715。

我们完成Trap入口与恢复、中断异常分发、时钟计数及关机，并移植Lab2物理内存管理。课程评分100/100，具体实现和限制见[MD报告](lab3-report.md)、[模板PDF](report-template/lab3-report.pdf)及[LaTeX源码](report-template/lab3-report.tex)。AI作为辅助，报告采用我们的小组视角。

在Ubuntu中逐条执行：

```bash
cd ~/os/lab3
export PATH=/home/lmy/riscv-elf-toolchains/bin:$PATH
make -j4
make qemu
make grade
```

正常运行每100次时钟输出100 ticks，默认累计10次输出后关机；关机次数是我们选择的可配置值，不是指导书指定数字。评分检查35/25/40三部分。两个终端分别执行make debug和make gdb可手动调试。评分后重新make生成普通镜像。

我们保持课程测试源码不变，只修正加载兼容；详细讲解和演示文件保留本地。模板源码与校徽、水印放在同一目录，用XeLaTeX编译两次。
