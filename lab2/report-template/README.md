# 南开模板排版版

版式来源：用户提供的 `D:/南开大学实验报告模版/report-template.tex`、`NKU.png`、`NKU-watermark.pdf`。

保留A4版心、校徽、封面层级、水印、页眉页脚及目录风格。原模板的编译原理示例正文、成员、教师和代码未沿用；封面替换为操作系统Lab2与本小组三位成员，未填写未经提供的教师信息。

原版Markdown报告在上级目录 `lab2-report.md`，继续作为正文编辑源，不删除、不以PDF替代。生成后的 `lab2-report.tex` 和 `lab2-report.pdf` 为同一正文的模板排版版本。

## 更新顺序

1. 修改 `../lab2-report.md`。
2. 若实验目标或结论改变，核对 `frontmatter.tex` 中摘要、封面信息。
3. 在Lab2目录执行 `python3 tools/build_report.py --compile`，自动生成LaTeX并运行两次XeLaTeX，更新目录。
4. 检查PDF排版与 `markdown-sha256.txt` 的正文哈希，最后刷新提交包校验清单。

Python依赖：`mistune>=3,<4`；排版依赖XeLaTeX及ctex、xurl等TeX Live宏包。模板版PDF本次已逐页渲染检查。生成的正文不应单独编辑，否则下次生成会被覆盖。

`nku-style-preamble.tex`保存从提供模板提取并调整的样式，`frontmatter.tex`保存封面与摘要，`build_report.py`位于上级tools目录。
