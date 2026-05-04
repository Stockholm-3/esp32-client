#!/usr/bin/env python3
"""
filter_lint.py

Filters clang-tidy output to show ONLY warnings and errors that originate
in YOUR source files (under --root).

What gets dropped:
  • Any diagnostic (warning/error/note/remark) whose path is outside the
    project root, under ~/.espressif, or under /nix/store.
  • note: lines that are continuations of a dropped primary diagnostic.
  • note: lines that are macro-expansion chains ("expanded from macro X")
    pointing into system headers — even when the *primary* diagnostic is
    in your code, these notes are noise.
  • The "N warnings and M errors generated." summary lines.

What is kept:
  • warnings and errors in your files.
  • note: lines that point back into YOUR code (e.g. "previous declaration
    here" in one of your headers).
  • Source-snippet / caret lines (the │ and ^ lines) that belong to a
    kept diagnostic.

Output is ANSI-colorized when stdout is a TTY.

Usage:
    run-clang-tidy ... 2>&1 | python3 scripts/filter_lint.py --root /path/to/project
    run-clang-tidy ... 2>&1 | python3 scripts/filter_lint.py   # uses cwd
"""

import argparse
import os
import re
import sys
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# Regex patterns
# ---------------------------------------------------------------------------

# Primary diagnostic line: /abs/path:line:col: kind: message [check-name]
DIAG_RE = re.compile(
    r"^(/[^:]+):(\d+):(\d+):\s+(warning|error|note|remark):(.*)"
)

# The bare "note: expanded from macro 'X'" lines that have no path prefix
BARE_NOTE_RE = re.compile(r"^note:\s+")

# clang summary line — always drop
SUMMARY_RE = re.compile(
    r"^\d+ (warning|error)s?( and \d+ (warning|error)s?)? generated\."
)

# "In file included from …" context lines — always drop
INCLUDE_FROM_RE = re.compile(r"^In file included from ")

# ---------------------------------------------------------------------------
# ANSI helpers
# ---------------------------------------------------------------------------
USE_COLOR = sys.stdout.isatty()

RESET  = "\033[0m"  if USE_COLOR else ""
BOLD   = "\033[1m"  if USE_COLOR else ""
DIM    = "\033[2m"  if USE_COLOR else ""
RED    = "\033[31m" if USE_COLOR else ""
YELLOW = "\033[33m" if USE_COLOR else ""
CYAN   = "\033[36m" if USE_COLOR else ""
WHITE  = "\033[97m" if USE_COLOR else ""


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
    # Must be *inside* the project root (with trailing sep so we don't
    # accidentally match a project root that's a prefix of another path).
    return not real.startswith(project_root + os.sep) and real != project_root


# ---------------------------------------------------------------------------
# Block model
# ---------------------------------------------------------------------------

@dataclass
class Diagnostic:
    """One clang diagnostic (warning/error) plus its attached body lines."""
    header:  str
    path:    str
    kind:    str                      # "warning" | "error"
    foreign: bool                     # True  → drop entirely
    body:    list[str] = field(default_factory=list)
    # notes attached to this diagnostic (each is a sub-list: [header, *body])
    notes:   list[list[str]] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=os.getcwd(),
                        help="Project root — diagnostics outside it are suppressed")
    args = parser.parse_args()

    project_root     = os.path.realpath(args.root)
    foreign_prefixes = (
        "/nix/store/",
        os.path.expanduser("~/.espressif/"),
        "/usr/include",
        "/usr/local/include",
        "/usr/lib/",
    )

    current: Diagnostic | None = None
    current_note: list[str] | None = None  # lines for the note being accumulated

    def flush() -> None:
        """Emit the current diagnostic if it belongs to our project."""
        nonlocal current, current_note
        if current is None:
            return
        # Flush any in-progress note
        if current_note is not None:
            current.notes.append(current_note)
            current_note = None

        if current.foreign:
            current = None
            return

        # Print primary
        print(color_header(current.header, current.kind))
        for line in current.body:
            print(color_snippet(line))

        # Print notes that point into OUR code (not system headers)
        for note_lines in current.notes:
            if not note_lines:
                continue
            note_header = note_lines[0]
            m = DIAG_RE.match(note_header)
            if m:
                note_path = m.group(1)
                if is_foreign(note_path, project_root, foreign_prefixes):
                    continue   # drop system-header note
                print(color_header(note_header, "note"))
                for line in note_lines[1:]:
                    print(color_snippet(line))
            else:
                # bare "note: ..." without a path — these are macro expansions,
                # drop them to avoid the Xtensa macro chain noise
                pass

        current = None

    for raw in sys.stdin:
        line = raw.rstrip("\n")

        # ── Always-dropped line types ──────────────────────────────────────
        if SUMMARY_RE.match(line):
            continue
        if INCLUDE_FROM_RE.match(line):
            continue

        # ── Diagnostic header? ─────────────────────────────────────────────
        m = DIAG_RE.match(line)
        if m:
            path = m.group(1)
            kind = m.group(4)

            if kind == "note":
                # note belonging to the current diagnostic
                if current is not None:
                    if current_note is not None:
                        current.notes.append(current_note)
                    current_note = [line]
                # If no current diagnostic, discard orphaned notes
                continue

            # New primary diagnostic — flush the old one first
            flush()
            current_note = None
            current = Diagnostic(
                header=line,
                path=path,
                kind=kind,
                foreign=is_foreign(path, project_root, foreign_prefixes),
            )
            continue

        # ── Bare "note:" without a path (macro expansion chains) ──────────
        if BARE_NOTE_RE.match(line):
            # Attach to the current note accumulator, but we'll drop it at
            # flush time anyway since it has no path to classify.
            if current_note is not None:
                current_note.append(line)
            continue

        # ── Body line (source snippet / caret / blank) ────────────────────
        if current_note is not None:
            current_note.append(line)
        elif current is not None:
            current.body.append(line)
        else:
            # Progress lines from run-clang-tidy: "[1/9] clang-tidy ..."
            print(line)

    flush()


if __name__ == "__main__":
    main()
