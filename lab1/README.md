# Lab1 提交材料

组长：2413982 李珉熠；成员：2411080 吴宇石、2413715 赵亚鑫。

## 文件说明

- lab1-report.md：按课程模板编写的实验报告。
- kern/、libs/、tools/、Makefile：实验源码及构建配置。
- images/：四张终端截图（编译、启动及 GDB 调试），报告使用相对路径引用。
- lab1-gdb.log：原始 GDB 调试日志。
- changes/Makefile.patch：启动参数修复记录。
- SHA256SUMS.txt：文件校验清单。

## 构建与运行

在安装了 RISC-V 工具链、GNU make 和 QEMU 的 Linux 中，进入本目录：

```bash
make
make qemu
```

应输出 `(THU.CST) os is loading ...`，随后进入无限循环。
QEMU 退出方式为 Ctrl+A，松开后按 X。
调试时在两个终端分别执行 make debug 与 make gdb。

当前实验包缺少 tools/grade.sh，不支持直接通过 make grade 评分。
未包含 bin/、obj/、编辑器设置或过程备份，make 可重新生成编译产物。
保留报告和 images 的目录关系，以免图片失效。

## 整理与提交说明

已补入清理后重新编译的终端截图，并更新报告中的验证记录。
小组分工按模块拟定了建议安排，需要成员认可后作为小组分工使用。
AI 信息按已有报告记录填写，缺少个人记录和模型版本时明确注明未记录，未虚构使用经历。
报告中的环境为 VMware Ubuntu，未混入另一套 WSL 环境的运行证据。
校验清单已按本包全部其他文件重新生成。
本压缩包仅整理本地材料，未上传课程平台或 Git 仓库。
