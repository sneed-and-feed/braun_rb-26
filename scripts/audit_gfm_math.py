#!/usr/bin/env python3
"""
Automated GitHub Flavored Markdown (GFM) & KaTeX Math Compliance Linter.
Validates markdown files against the strict rules in the github-math-compliance skill.
"""

import glob
import os
import sys
import re


def audit_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    errors = []
    in_code_block = False
    in_math_fence = False
    in_display_dollars = False

    for idx, line in enumerate(lines, 1):
        stripped = line.strip()

        # Track fenced code blocks and ```math fences independently
        if stripped.startswith('```') or stripped.startswith('~~~'):
            if in_math_fence:
                in_math_fence = False
            elif in_code_block:
                in_code_block = False
            elif stripped.startswith('```math'):
                in_math_fence = True
                if not line.startswith('```math'):
                    errors.append((idx, "Display math fence ```math must start at Column 0 (no indentation)"))
                if idx > 1 and lines[idx - 2].strip() != '':
                    errors.append((idx, "Display math fence ```math must be preceded by an empty blank line"))
            else:
                in_code_block = True
            continue

        if in_code_block:
            continue

        # Check display math $$
        if stripped.startswith('$$'):
            if line.startswith('   $$') or line.startswith('  $$') or line.startswith(' $$') or line.startswith('\t$$'):
                errors.append((idx, "Display math $$ block must start at Column 0 (no indentation)"))
            # Check blank line before if idx > 1
            if idx > 1 and lines[idx - 2].strip() != '':
                errors.append((idx, "Display math $$ block must be preceded by an empty blank line"))
            # Check if one-line display math
            if stripped.endswith('$$') and len(stripped) > 2:
                # one-line $$...$$
                if idx < len(lines) and lines[idx].strip() != '':
                    errors.append((idx, "Display math $$ block must be followed by an empty blank line"))
            else:
                in_display_dollars = not in_display_dollars

        # 1. Backtick math
        if re.search(r'\$`|`\$', line):
            errors.append((idx, "Backtick math detected (`$ or $`)"))

        # 2. Markdown bold/italics wrapping $
        if re.search(r'\*\*[^*$\n]*\$[^*$\n]*\*\*', line):
            errors.append((idx, "Markdown bold wrapping math (**...$...**)"))
        if re.search(r'(?<!\*)\*[^*$\n]+\$[^*$\n]+\*(?!\*)', line):
            errors.append((idx, "Markdown italic wrapping math (*...$...*)"))

        # 3. \text{--} in math
        if re.search(r'\$[^$]*\\text\{--\}[^$]*\$', line) or ((in_math_fence or in_display_dollars) and r'\text{--}' in line):
            errors.append((idx, r"\text{--} in math mode"))

        # 5. Raw | inside table math cells
        if line.startswith('|'):
            cells = line.split('|')[1:-1]
            for cell in cells:
                clean_cell = re.sub(r'\\\$', '', cell)
                if clean_cell.count('$') % 2 != 0:
                    errors.append((idx, "Raw pipe | inside table math cell (splits cell; use \\mid, \\lvert, \\rvert)"))
                    break

        # 7. \hline in tables
        if '\\hline' in line:
            errors.append((idx, r"\hline in markdown table"))

        # 8. Indented math fences
        if (line.startswith('   ```math') or line.startswith('  ```math') or line.startswith('    ```math')) or \
           (line.startswith('   $$') or line.startswith('  $$') or line.startswith(' $$') or line.startswith('\t$$')):
            errors.append((idx, "Indented display math block (must be Column-0)"))

        # 10. List items beginning with math before bold title
        if re.match(r'^\s*\d+\.\s*\$', line):
            errors.append((idx, "List item beginning with inline math before bold title"))

        # 11. \operatorname
        if r'\operatorname' in line:
            errors.append((idx, r"\operatorname macro detected (use \mathrm)"))

        # 13. Lie group subscripts
        if re.search(r'\\mathrm\{[SsGg][LUu][234n]\}', line) or re.search(r'\\mathrm\{SL\}_', line):
            errors.append((idx, "Lie group underscore subscript detected"))

        # 14. Parenthesized math with internal parentheses
        if re.search(r'\(\$[^$]*\([^$]*\)[^$]*\$\)', line):
            errors.append((idx, "Outer parenthesized math with internal parentheses (($...()$))"))

        # 15. Unescaped \left\{ or \right\}
        if re.search(r'\\left\\\{|\\right\\\}', line):
            errors.append((idx, r"Unescaped \left\{ or \right\} (use \left\lbrace and \right\rbrace)"))

        # 16. Brace-preceded font macro subscripts
        if re.search(r'\\(mathcal|mathbb|mathbf|mathfrak|vec)\{[A-Za-z]\}_', line):
            errors.append((idx, "Brace-preceded font macro subscript (e.g. \\mathcal{H}_F)"))

        # 17. Math in link anchor text
        if re.search(r'\[[^\]]*\$[^\]]*\]\(.*?\)', line):
            errors.append((idx, "Math in markdown link anchor text"))

        # 18. Prime-preceded subscripts
        if re.search(r'\\(bigotimes|prod|coprod|bigoplus)\'_', line):
            errors.append((idx, "Prime-preceded subscript operator (use {\\bigotimes_p}')"))

        # 9. Multiple inline math spans with subscripts on same line
        inline_spans = re.findall(r'(?<!\\)\$([^$\n]+)\$', line)
        if len(inline_spans) >= 2:
            subscript_spans = [s for s in inline_spans if '_' in s]
            if len(subscript_spans) >= 2:
                errors.append((idx, "Multiple separate inline math spans with subscripts on same line (risk of CommonMark _ hijack)"))

        # Check unclosed $ on single line (if not inside display math fence or multi-line $$)
        if not in_display_dollars and not in_math_fence and not line.strip().startswith('$$'):
            clean_line = re.sub(r'\\\$', '', line)
            dollars = clean_line.count('$')
            if dollars % 2 != 0:
                errors.append((idx, f"Unclosed or odd count of $ delimiters ({dollars} found)"))

    return errors


if __name__ == "__main__":
    raw_args = sys.argv[1:]
    if not raw_args:
        files = []
        repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        skip_dirs = {".git", "build", "node_modules", ".agents"}
        for root, dirs, filenames in os.walk(repo_root):
            dirs[:] = sorted(d for d in dirs if d not in skip_dirs)
            for fname in sorted(filenames):
                if fname.lower().endswith(".md"):
                    files.append(os.path.relpath(os.path.join(root, fname), repo_root))
    else:
        files = []
        for arg in raw_args:
            if any(ch in arg for ch in "*?["):
                matched = sorted(glob.glob(arg, recursive=True))
                if matched:
                    files.extend(matched)
                else:
                    files.append(arg)
            elif os.path.isdir(arg):
                files.extend(sorted(glob.glob(os.path.join(arg, "**", "*.md"), recursive=True)))
            else:
                files.append(arg)

    total_errors = 0
    for fpath in files:
        errs = audit_file(fpath)
        if errs:
            print(f"FAILED: {fpath} ({len(errs)} issues found):")
            for line_no, msg in errs:
                print(f"  Line {line_no}: {msg}")
            total_errors += len(errs)
        else:
            print(f"PASSED: {fpath} (0 math compliance issues)")

    if total_errors > 0:
        sys.exit(1)
    else:
        print("ALL AUDITED FILES 100% GFM / KATEX COMPLIANT!")
