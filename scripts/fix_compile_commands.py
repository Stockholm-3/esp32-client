#!/usr/bin/env python3
"""Rewrites compile_commands.json for clang-tidy on ESP-IDF projects."""
import json
import re
import sys
from pathlib import Path

STRIP_FLAGS = {
    '-mlongcalls', '-fno-malloc-dce', '-fno-tree-switch-conversion',
    '-fstrict-volatile-bitfields', '-fzero-init-padding-bits=all',
    '-mdisable-hardware-atomics', '-fno-jump-tables',
}

STRIP_PREFIXES = (
    '-fzero-init-padding-bits',
)

def fix_command(command: str) -> str:
    # Replace xtensa compiler with clang
    command = re.sub(r'\S+xtensa\S+-(gcc|g\+\+)', 'clang', command)
    # Remove response files (@/path/to/file)
    command = re.sub(r'@\S+', '', command)
    # Tokenize and filter flags
    tokens = command.split()
    result = []
    skip_next = False
    for tok in tokens:
        if skip_next:
            skip_next = False
            continue
        if tok in STRIP_FLAGS:
            continue
        if any(tok.startswith(p) for p in STRIP_PREFIXES):
            continue
        result.append(tok)
    return ' '.join(result)

def process(input_path: str, output_path: str):
    with open(input_path) as f:
        db = json.load(f)
    for entry in db:
        if 'command' in entry:
            entry['command'] = fix_command(entry['command'])
    with open(output_path, 'w') as f:
        json.dump(db, f, indent=2)
    print(f"Written {output_path} ({len(db)} entries)")

if __name__ == '__main__':
    process(sys.argv[1], sys.argv[2])
