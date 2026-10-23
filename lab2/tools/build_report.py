#!/usr/bin/env python3
"""Build the NKU template edition from the canonical Markdown report.

Requires Python mistune>=3, XeLaTeX, and the supplied NKU visual assets.
The Markdown is never overwritten by this builder.
"""
from pathlib import Path
import argparse
import hashlib
import re
import shutil
import subprocess
import sys

try:
    import mistune
except ImportError:
    # Local authoring dependency; portable users can pip install mistune.
    dependency = Path(__file__).resolve().parents[3] / '.docdeps'
    sys.path.insert(0, str(dependency))
    import mistune

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'report-template'
ESC = {'\\': r'\textbackslash{}', '&': r'\&', '%': r'\%', '$': r'\$',
       '#': r'\#', '_': r'\_', '{': r'\{', '}': r'\}',
       '~': r'\textasciitilde{}', '^': r'\textasciicircum{}'}

def escape(s):
    return ''.join(ESC.get(c, c) for c in s).replace('→',r'\ensuremath{\rightarrow}').replace('≥',r'\ensuremath{\geq}').replace('≤',r'\ensuremath{\leq}')

def render(nodes):
    result = []
    for n in nodes:
        typ = n['type']
        children = n.get('children', [])
        raw = n.get('raw', '')
        if typ == 'text':
            value = escape(raw)
        elif typ in ('softbreak','linebreak'):
            value = ' '
        elif typ == 'codespan':
            value = r'\nolinkurl{' + raw + '}'
        elif typ == 'strong':
            value = r'\textbf{' + render(children) + '}'
        elif typ == 'emphasis':
            value = r'\emph{' + render(children) + '}'
        elif typ == 'link':
            url = n['attrs']['url']
            label = render(children)
            # Relative companion documents are included by name in a printed report.
            value = (r'\href{' + url.replace('%',r'\%') + '}{' + label + '}') if url.startswith('http') else label
        elif typ == 'heading':
            level = n['attrs']['level']
            if level == 1:
                value = ''
            else:
                title = render(children)
                title = re.sub(r'^(?:[一二三四五六七八九十]+、|\d+(?:\.\d+)*[.．]?\s*)', '', title)
                if title == '实验基本信息与小组分工':
                    value = r'\section*{' + title + '}\n'
                else:
                    command = {2:'section',3:'subsection'}.get(level,'subsubsection')
                    value = '\\'+command+'{'+title+'}\n'
        elif typ in ('paragraph','block_text'):
            value = render(children) + '\n\n'
        elif typ == 'blank_line':
            value = '\n'
        elif typ == 'block_code':
            value = '\\begin{lstlisting}\n'+raw+'\\end{lstlisting}\n'
        elif typ == 'list':
            env = 'enumerate' if n.get('attrs',{}).get('ordered') else 'itemize'
            value = '\\begin{'+env+'}\n'+render(children)+'\\end{'+env+'}\n'
        elif typ == 'list_item':
            value = '\\item '+render(children)
        elif typ == 'table':
            head = children[0]['children']
            rows = children[1]['children']
            count = len(head)
            widths = {2:[.25,.68],3:[.25,.29,.37],4:[.17,.24,.22,.26]}.get(count,[.9/count]*count)
            spec = ''.join(r'>{\raggedright\arraybackslash}p{'+str(w)+r'\linewidth}' for w in widths)
            row = lambda cells: ' & '.join(render(c['children']).strip() for c in cells)+r' \\'+'\n'
            value = '{\\small\n\\setlength{\\tabcolsep}{3pt}\n\\renewcommand{\\arraystretch}{1.22}\n\\begin{longtable}{'+spec+'}\n\\toprule\n'+row(head)+r'\midrule\endfirsthead'+'\n'+row(head)+r'\midrule\endhead'+'\n'
            value += ''.join(row(r['children']) for r in rows)
            value += '\\bottomrule\n\\end{longtable}\n}\n'
        elif typ == 'thematic_break':
            value = '\n'
        else:
            raise ValueError(f'Unsupported Markdown element: {typ}')
        result.append(value)
    return ''.join(result)

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--compile',action='store_true')
    args=parser.parse_args()
    source=ROOT/'lab2-report.md'
    template=OUT/'nku-style-preamble.tex'
    if not template.exists():
        raise SystemExit('Missing NKU template preamble; keep report-template assets together.')
    content=source.read_text(encoding='utf-8')
    ast=mistune.create_markdown(renderer='ast',plugins=['table'])(content)
    body=render(ast)
    source_hash=hashlib.sha256(source.read_bytes()).hexdigest()
    front=(OUT/'frontmatter.tex').read_text(encoding='utf-8')
    generated='% Generated from lab2-report.md; SHA256='+source_hash+'\n'+template.read_text(encoding='utf-8')+'\n'+front+'\n'+body+'\n\\end{document}\n'
    (OUT/'lab2-report.tex').write_text(generated,encoding='utf-8',newline='\n')
    (OUT/'markdown-sha256.txt').write_text(source_hash+'  ../lab2-report.md\n',encoding='utf-8',newline='\n')
    if args.compile:
        engine=shutil.which('xelatex')
        if not engine:raise SystemExit('XeLaTeX not found in PATH')
        for i in range(2):
            with (OUT/f'compile-{i+1}.log').open('w',encoding='utf-8') as output:
                subprocess.run([engine,'-interaction=nonstopmode','-halt-on-error','lab2-report.tex'],cwd=OUT,stdout=output,stderr=subprocess.STDOUT,check=True)
    print('Built template edition from canonical Markdown:',source_hash)

if __name__=='__main__': main()
