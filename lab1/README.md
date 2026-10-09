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
报告按三名成员分别负责环境与构建、入口代码分析和 GDB 调试的分工组织。
三名成员的 AI 工具统一记录为 Codex 桌面应用，辅助内容对应各自负责的模块；具体模型版本未单独记录。
报告中的环境为 VMware Ubuntu，未混入另一套 WSL 环境的运行证据。
校验清单已按本包全部其他文件重新生成。
实验材料保存在 GitHub 仓库 https://github.com/lmyhs123/OS 的 lab1 目录；未上传课程平台。
