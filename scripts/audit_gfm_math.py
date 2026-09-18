import sys
import re

def audit_file(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        lines = f.readlines()

    errors = []
    in_code_block = False
    in_math_fence = False

    for idx, line in enumerate(lines, 1):
        stripped = line.strip()

        # Track fenced code blocks
        if stripped.startswith('```') or stripped.startswith('~~~'):
            if stripped.startswith('```math'):
                in_math_fence = not in_math_fence
            elif in_code_block:
                in_code_block = False
            else:
                in_code_block = True
            continue

        if in_code_block:
            continue

        # 1. Backtick math
        if re.search(r'\$`|`\$', line):
            errors.append((idx, "Backtick math detected (`$ or $`)"))

        # 2. Markdown bold/italics wrapping $
        if re.search(r'\*\*[^*]*\$[^*]*\*\*', line):
            errors.append((idx, "Markdown bold wrapping math (**...$...**)"))
        if re.search(r'(?<!\*)\*[^*$\n]+\$[^*$\n]+\*(?!\*)', line):
            errors.append((idx, "Markdown italic wrapping math (*...$...*)"))

        # 3. \text{--} in math
        if re.search(r'\$[^$]*\\text\{--\}[^$]*\$', line):
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
           (line.startswith('   $$') or line.startswith('  $$')):
            errors.append((idx, "Indented display math block (must be Column-0)"))

        # 10. List items beginning with math before bold title
        if re.match(r'^\s*\d+\.\s*\$', line):
            errors.append((idx, "List item beginning with inline math before bold title"))

        # 11. \operatorname
        if re.search(r'\$[^$]*\\operatorname[^$]*\$', line):
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

        # Check unclosed $ on single line (if not display math fence $$)
        # Count non-escaped $
        clean_line = re.sub(r'\\\$', '', line)
        dollars = clean_line.count('$')
        if dollars % 2 != 0:
            errors.append((idx, f"Unclosed or odd count of $ delimiters ({dollars} found)"))

    return errors

if __name__ == "__main__":
    files = sys.argv[1:]
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
