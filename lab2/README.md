# Lab2：物理内存与页表

成员：2413982 李珉熠（组长）、2411080 吴宇石、2413715 赵亚鑫。

- [实验报告](./lab2-report.md)
- [演示命令与参数解释](./docs/演示命令.md)
- [从零原理解释和助教追问](./docs/原理详解与答辩追问.md)
- [任务规格与辅助提示词](./prompts.md)
- [课程评分输出](./evidence/grade.log)
- [QEMU取指翻译记录](./evidence/host-console.log)
- [QEMU数据读翻译记录](./evidence/host-data-console.log)

## 构建

Ubuntu安装RISC-V裸机工具链、make、QEMU riscv64系统模拟器、OpenSBI、gdb-multiarch、python3和bc后：

```bash
export PATH=/home/lmy/riscv-elf-toolchains/bin:$PATH
make -j4
make qemu
```

默认Best-Fit运行后会输出检查与SLUB PASS；Ctrl+A、松开按X退出。使用其他工具链时调整PATH或指定GCCPREFIX；Makefile默认GCCPREFIX为riscv64-unknown-elf-。

```bash
make grade
make clean
make PMM=buddy -j4
make PMM=buddy qemu
```

Buddy结束后恢复默认前先make clean再make。切换编译选项不应复用旧目标文件。

课程page_init与Best-Fit评分实测20/20；Buddy与SLUB的独立测试也运行通过，详见报告。测试使用当前128MiB默认配置，另有64/256MiB边界运行记录。不包含生产级并发或完整Linux SLUB。

## QEMU分支任务

系统QEMU不替换。带符号QEMU在实验虚拟机的`/home/lmy/oslab-tools/qemu-ubuntu-build/`。其他机器可参考`tools/build-debug-qemu.sh`构建，宿主需有GCC、glib2开发库、zlib开发库、Python venv等构建依赖。源码来自Ubuntu的QEMU 8.2.2重打包源代码，未上传庞大源码和二进制；摘录及GPL许可证保存在evidence，完整来源链接在报告中。

```bash
python3 tools/verify_guest.py
python3 tools/verify_host.py
python3 tools/verify_host.py boot_dtb
```

这些命令已经在本实验虚拟机运行验证。客体自动验证用1235端口，两层自动验证用1236，手工演示用1234；避免同端口启动多个实例。宿主脚本在HOST_TRACE_COMPLETE后主动结束它启动的QEMU。修改源码后自动脚本会通过nm查目标符号，QEMU版本/源码行号变化需重新核对断点。

日志为实际运行文本，没有用模拟截图替代。课程原始basic_check/best_fit_check与注入评分测试未修改。`changes/course-skeleton.patch`记录对原框架文件的改动，新算法文件可在kern/mm中查看。
