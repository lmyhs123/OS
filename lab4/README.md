# Lab4：进程管理

我们完成页映射、内核线程创建和调度切换，课程评分实测 30/30。

- [Markdown 实验报告](lab4-report.md)
- [南开模板 PDF](report-template/lab4-report.pdf)
- [LaTeX 报告源码](report-template/lab4-report.tex)

我们在 Ubuntu 虚拟机的 `~/os/lab4` 中执行：

```bash
export PATH=/home/lmy/riscv-elf-toolchains/bin:$PATH
make -j4
make grade
make qemu
```

双终端调试：A 执行 `make debug`，B 在同目录执行 `make gdb`。具体观察命令与原理见报告第七节。

框架的 `do_exit` 保留 panic 占位，init 结束后进入调试监视器。课程测试源码和注入器保持原样，GCC 15 对测试中函数指针类型不匹配的诊断降为告警。我们使用课程自带 SLOB，底层沿用 Best Fit。AI 辅助实现和验证；报告记录需求、提示词、实际问题与验证范围。
