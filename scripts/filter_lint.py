#!/usr/bin/env python3
"""
filter_lint.py

Filters clang-tidy output to show ONLY warnings and errors from YOUR source
files.  Notes are dropped entirely.  Output is colorized with ANSI codes when
writing to a terminal (auto-detected; stripped when piped to a file or CI log
that does not support color).

Usage:
    run-clang-tidy ... 2>&1 | python3 scripts/filter_lint.py --root /path/to/project
    run-clang-tidy ... 2>&1 | python3 scripts/filter_lint.py   # uses cwd

Exit codes (propagated from run-clang-tidy via the Makefile pipefail):
    0  — no findings
    1  — at least one warning or error
"""

import argparse
import os
import re
import sys
from dataclasses import dataclass, field

DIAG_RE    = re.compile(r"^(/[^:]+):(\d+):(\d+):\s+(warning|error|note|remark):(.*)")
SUMMARY_RE = re.compile(r"^\d+ (warning|error)s?( and \d+ (warning|error)s?)? generated\.")

# ── ANSI helpers ──────────────────────────────────────────────────────────────
USE_COLOR = sys.stdout.isatty()

RESET  = "\033[0m"  if USE_COLOR else ""
BOLD   = "\033[1m"  if USE_COLOR else ""
DIM    = "\033[2m"  if USE_COLOR else ""

RED    = "\033[31m" if USE_COLOR else ""
YELLOW = "\033[33m" if USE_COLOR else ""
CYAN   = "\033[36m" if USE_COLOR else ""
WHITE  = "\033[97m" if USE_COLOR else ""


def color_header(line: str, kind: str) -> str:
    """Re-color the diagnostic header line."""
    if not USE_COLOR:
        return line
    m = re.match(
        r"^(/[^:]+):(\d+):(\d+):\s+(warning|error):\s+(.*?)(\s+\[[\w.,-]+\])?$",
        line,
    )
    if not m:
        return line
    path, ln, col, kw, msg, check = m.groups()
    kw_color  = RED if kw == "error" else YELLOW
    check_str = f"{DIM}{check or ''}{RESET}"
    return (
        f"{DIM}{path}{RESET}"
        f":{WHITE}{ln}{RESET}:{WHITE}{col}{RESET}: "
        f"{kw_color}{BOLD}{kw}:{RESET} "
        f"{BOLD}{msg}{RESET}"
        f"{check_str}"
    )


def color_snippet(line: str) -> str:
    """Dim the line-number gutter and color the caret line."""
    if not USE_COLOR:
        return line
    # Caret/tilde line:  "      |     ^~~~"
    if re.match(r"^\s*\|\s*[\^~]", line):
        return f"{CYAN}{line}{RESET}"
    # Source line:       "  359 |     code here"
    m = re.match(r"^(\s*\d+\s*\|)(.*)", line)
    if m:
        return f"{DIM}{m.group(1)}{RESET}{m.group(2)}"
    return line


# ── Domain helpers ────────────────────────────────────────────────────────────

@dataclass
class Block:
    header: str
    path:   str
    kind:   str
    body:   list[str] = field(default_factory=list)


def is_foreign(path: str, project_root: str, foreign_prefixes: tuple) -> bool:
    real = os.path.realpath(path)
    if any(real.startswith(p) for p in foreign_prefixes):
        return True
    return not real.startswith(project_root)


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=os.getcwd())
    args = parser.parse_args()

    project_root     = os.path.realpath(args.root)
    foreign_prefixes = (
        "/nix/store/",
        os.path.expanduser("~/.espressif/"),
    )

    primary: Block | None = None
    in_note               = False

    def flush() -> None:
        if primary is None:
            return
        if not is_foreign(primary.path, project_root, foreign_prefixes):
            print(color_header(primary.header, primary.kind))
            for line in primary.body:
                print(color_snippet(line))

    for raw in sys.stdin:
        line = raw.rstrip("\n")

        m = DIAG_RE.match(line)
        if m:
            kind = m.group(4)
            if kind == "note":
                in_note = True
            else:
                flush()
                primary = Block(header=line, path=m.group(1), kind=kind)
                in_note = False
        elif SUMMARY_RE.match(line):
            pass
        elif in_note:
            pass
        elif primary is not None:
            primary.body.append(line)
        else:
            print(line)  # progress lines: [N/M] clang-tidy ...

    flush()


if __name__ == "__main__":
    main()
