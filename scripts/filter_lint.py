#!/usr/bin/env python3
"""
filter_lint.py

Filters clang-tidy output to show ONLY warnings and errors that originate
in YOUR source files (under --root).
"""

import argparse
import os
import re
import sys
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# Regex patterns
# ---------------------------------------------------------------------------

DIAG_RE = re.compile(
    r"^(/[^:]+):(\d+):(\d+):\s+(warning|error|note|remark):(.*)"
)
BARE_NOTE_RE = re.compile(r"^note:\s+")
SUMMARY_RE = re.compile(
    r"^\d+ (warning|error)s?( and \d+ (warning|error)s?)? generated\."
)
INCLUDE_FROM_RE = re.compile(r"^In file included from ")

# ---------------------------------------------------------------------------
# ANSI helpers (Updated to be set at runtime)
# ---------------------------------------------------------------------------
USE_COLOR = False
RESET = ""
BOLD = ""
DIM = ""
RED = ""
YELLOW = ""
CYAN = ""
WHITE = ""


def color_header(line: str, kind: str) -> str:
    if not USE_COLOR:
        return line
    m = re.match(
        r"^(/[^:]+):(\d+):(\d+):\s+(warning|error):\s+(.*?)(\s+\[[\w.,-]+\])?$",
        line,
    )
    if not m:
        return line
    path, ln, col, kw, msg, check = m.groups()
    kw_color = RED if kw == "error" else YELLOW
    check_str = f"{DIM}{check or ''}{RESET}"
    return (
        f"{DIM}{path}{RESET}"
        f":{WHITE}{ln}{RESET}:{WHITE}{col}{RESET}: "
        f"{kw_color}{BOLD}{kw}:{RESET} "
        f"{BOLD}{msg}{RESET}"
        f"{check_str}"
    )


def color_snippet(line: str) -> str:
    if not USE_COLOR:
        return line
    if re.match(r"^\s*\|\s*[\^~]", line):
        return f"{CYAN}{line}{RESET}"
    m = re.match(r"^(\s*\d+\s*\|)(.*)", line)
    if m:
        return f"{DIM}{m.group(1)}{RESET}{m.group(2)}"
    return line


# ---------------------------------------------------------------------------
# Path classification
# ---------------------------------------------------------------------------


def _real(path: str) -> str:
    try:
        return os.path.realpath(path)
    except Exception:
        return path


def is_foreign(path: str, project_root: str, foreign_prefixes: tuple) -> bool:
    real = _real(path)
    if any(real.startswith(p) for p in foreign_prefixes):
        return True
    return not real.startswith(project_root + os.sep) and real != project_root


# ---------------------------------------------------------------------------
# Block model
# ---------------------------------------------------------------------------


@dataclass
class Diagnostic:
    """One clang diagnostic (warning/error) plus its attached body lines."""

    header: str
    path: str
    kind: str  # "warning" | "error"
    foreign: bool  # True  → drop entirely
    body: list[str] = field(default_factory=list)
    notes: list[list[str]] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main() -> None:
    global USE_COLOR, RESET, BOLD, DIM, RED, YELLOW, CYAN, WHITE

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        default=os.getcwd(),
        help="Project root — diagnostics outside it are suppressed",
    )
    parser.add_argument(
        "--force-color",
        action="store_true",
        help="Force ANSI color output even if piped",
    )
    args = parser.parse_args()

    USE_COLOR = sys.stdout.isatty() or args.force_color

    RESET = "\033[0m" if USE_COLOR else ""
    BOLD = "\033[1m" if USE_COLOR else ""
    DIM = "\033[2m" if USE_COLOR else ""
    RED = "\033[31m" if USE_COLOR else ""
    YELLOW = "\033[33m" if USE_COLOR else ""
    CYAN = "\033[36m" if USE_COLOR else ""
    WHITE = "\033[97m" if USE_COLOR else ""

    project_root = os.path.realpath(args.root)
    foreign_prefixes = (
        "/nix/store/",
        os.path.expanduser("~/.espressif/"),
        "/usr/include",
        "/usr/local/include",
        "/usr/lib/",
    )

    current: Diagnostic | None = None
    current_note: list[str] | None = None

    def flush() -> None:
        """Emit the current diagnostic if it belongs to our project."""
        nonlocal current, current_note
        if current is None:
            return
        if current_note is not None:
            current.notes.append(current_note)
            current_note = None

        if current.foreign:
            current = None
            return

        print(color_header(current.header, current.kind))
        for line in current.body:
            print(color_snippet(line))

        for note_lines in current.notes:
            if not note_lines:
                continue
            note_header = note_lines[0]
            m = DIAG_RE.match(note_header)  # <-- Fixed DIAGRE to DIAG_RE
            if m:
                note_path = m.group(1)
                if is_foreign(note_path, project_root, foreign_prefixes):
                    continue
                print(color_header(note_header, "note"))
                for line in note_lines[1:]:
                    print(color_snippet(line))
            else:
                pass

        current = None

    for raw in sys.stdin:
        line = raw.rstrip("\n")

        if SUMMARY_RE.match(line):
            continue
        if INCLUDE_FROM_RE.match(line):
            continue

        m = DIAG_RE.match(line)  # <-- Fixed DIAGRE to DIAG_RE
        if m:
            path = m.group(1)
            kind = m.group(4)

            if kind == "note":
                if current is not None:
                    if current_note is not None:
                        current.notes.append(current_note)
                    current_note = [line]
                continue

            flush()
            current_note = None
            current = Diagnostic(
                header=line,
                path=path,
                kind=kind,
                foreign=is_foreign(path, project_root, foreign_prefixes),
            )
            continue

        if BARE_NOTE_RE.match(line):
            if current_note is not None:
                current_note.append(line)
            continue

        if current_note is not None:
            current_note.append(line)
        elif current is not None:
            current.body.append(line)
        else:
            print(line)

    flush()


if __name__ == "__main__":
    main()
