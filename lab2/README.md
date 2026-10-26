# Lab2：物理内存与页表

我们小组成员为李珉熠（2413982，组长）、吴宇石（2411080）和赵亚鑫（2413715）。我们基于课程框架完成了物理页初始化、Best-Fit、Buddy 和简化 SLUB，并调试了 QEMU 的地址翻译路径。我们通过 `PMM=first`、`PMM=best` 和 `PMM=buddy` 分别构建三种页分配器，每次切换前先执行 `make clean`。

## 实验报告

- [Markdown 实验报告](./lab2-report.md)
- [南开模板 PDF 报告](./report-template/lab2-report.pdf)
- [南开模板 LaTeX 源码](./report-template/lab2-report.tex)

我们同步维护 Markdown 与模板版报告，正文包含小组分工、实现分析、测试结果、QEMU 分支任务和 AI 辅助记录。LaTeX 源码与同目录的校徽、水印资源可直接用于 XeLaTeX 编译。

## 我们的实现

我们在 `kern/mm` 中实现物理内存初始化和分配算法，在启动代码中补充低地址过渡映射，并修正构建与评分所需的兼容问题。我们保留了课程自带的 `basic_check`、`best_fit_check` 及评分测试源码，在其外增加独立检查。

## 编译与验证

我们的实验环境为 Ubuntu 24.04、RISC-V GCC 15.1.0、QEMU 8.2.2 和 OpenSBI 1.3；实验目录为 `/home/lmy/os/lab2`。我们使用以下命令构建和运行默认 Best-Fit 配置：

```bash
cd ~/os/lab2
export PATH=/home/lmy/riscv-elf-toolchains/bin:$PATH
make -j4
make qemu
```

运行后，我们用 Ctrl+A、松开后按 X 退出 QEMU，再执行评分及 Buddy 配置验证：

```bash
make grade
make clean
make PMM=buddy -j4
make PMM=buddy qemu
```

切回默认配置时，我们先执行 `make clean`，再执行 `make`。课程初始化与 Best-Fit 评分实测为 20/20，First-Fit、Buddy 和 SLUB 独立检查通过；Buddy 另包含16页保留页、唯一性、3/5页取整与恢复检查；详细范围与结果见报告。

## 调试验证

我们在一个终端执行 `make debug` 启动暂停的 QEMU，在另一个终端进入同一目录执行 `make gdb` 连接内核。QEMU 源码调试另使用我们独立构建的带符号版本；报告记录了 SATP、PTE、取指和数据读取的实际翻译结果。

本目录保留实验源码、必要的构建与评分文件及报告。我们将学习讲解、演示材料和完整调试日志保存在本地。
