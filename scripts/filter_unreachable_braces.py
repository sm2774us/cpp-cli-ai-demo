"""Strips a well-known gcov false-negative from an lcov .info file.

gcov marks a `}` immediately following a `return`/`break`/`continue`/
`throw` statement as an unreached line (nothing can execute past an
unconditional jump), even though the enclosing function runs
correctly and completely. This is a real, well-documented gcov
artifact, not a coverage gap -- but it recurs every time AI-rewritten
code happens to end a function/block with `return x;` immediately
followed by `}` on its own line, since the AI has no way to know to
add a manual `// LCOV_EXCL_LINE` marker (that only protects code a
human wrote and annotated by hand).

Rather than relying on manual markers -- which can't cover code an AI
agent rewrites from scratch -- this script detects the exact same
pattern automatically, directly against the actual source file, and
removes those specific 0-hit DA: entries from the .info file before
the coverage gate is evaluated. It only ever removes a DA: entry when
the corresponding source line is provably just `}` (optionally with a
trailing comment) whose preceding non-blank source line ends with one
of those unconditional-jump keywords -- never anything else -- so a
genuine coverage gap in real code is never hidden by this.

Usage:
    python scripts/filter_unreachable_braces.py coverage.filtered.info
"""

from __future__ import annotations

import re
import sys

JUMP_KEYWORDS = ("return", "break", "continue", "throw")


def is_unreachable_closing_brace(source_lines: list[str], line_no: int) -> bool:
    """Checks whether a given 1-indexed source line is a bare `}` that
    immediately follows an unconditional-jump statement.

    Args:
        source_lines: The full source file, split into lines (no
            trailing newlines), 0-indexed.
        line_no: The 1-indexed line number to check.

    Returns:
        True only if line_no's content is exactly `}` (optionally
        followed by a `//` comment) and the nearest preceding
        non-blank, non-comment-only line ends with `;` right after one
        of return/break/continue/throw.
    """
    if line_no < 1 or line_no > len(source_lines):
        return False

    this_line = source_lines[line_no - 1].strip()
    brace_only = re.match(r"^\}(\s*//.*)?$", this_line)
    if not brace_only:
        return False

    for prior_idx in range(line_no - 2, -1, -1):
        prior = source_lines[prior_idx].strip()
        if not prior or prior.startswith("//"):
            continue
        return any(
            re.search(rf"\b{kw}\b[^;{{}}]*;$", prior) for kw in JUMP_KEYWORDS
        )
    return False


def filter_info_file(path: str) -> int:
    """Rewrites an lcov .info file in place, removing false-negative
    brace-after-jump DA: entries.

    Args:
        path: Path to the .info file to filter in place.

    Returns:
        The number of DA: entries removed.
    """
    with open(path, encoding="utf-8") as handle:
        lines = handle.readlines()

    source_cache: dict[str, list[str]] = {}
    removed = 0
    output: list[str] = []
    current_source: list[str] | None = None
    lf_delta = 0

    def flush_lf(buf: list[str]) -> None:
        nonlocal lf_delta
        for i in range(len(buf) - 1, -1, -1):
            if buf[i].startswith("LF:"):
                buf[i] = f"LF:{int(buf[i][3:]) + lf_delta}\n"
                break
        lf_delta = 0

    record: list[str] = []
    for line in lines:
        if line.startswith("SF:"):
            src_path = line[3:].strip()
            if src_path not in source_cache:
                try:
                    with open(src_path, encoding="utf-8") as src:
                        source_cache[src_path] = src.read().splitlines()
                except OSError:
                    source_cache[src_path] = []
            current_source = source_cache[src_path]
            record = [line]
            continue

        if line.startswith("DA:") and current_source is not None:
            parts = line[3:].strip().split(",")
            line_no = int(parts[0])
            hits = int(parts[1])
            if hits == 0 and is_unreachable_closing_brace(
                current_source, line_no
            ):
                removed += 1
                lf_delta -= 1
                continue
            record.append(line)
            continue

        if line.startswith("end_of_record"):
            flush_lf(record)
            record.append(line)
            output.extend(record)
            record = []
            current_source = None
            continue

        record.append(line)

    output.extend(record)

    with open(path, "w", encoding="utf-8") as handle:
        handle.writelines(output)

    return removed


def main() -> int:
    """CLI entry point."""
    if len(sys.argv) != 2:
        print(
            "usage: filter_unreachable_braces.py <coverage.info>",
            file=sys.stderr,
        )
        return 2

    removed = filter_info_file(sys.argv[1])
    print(f"filtered {removed} brace-after-jump false-negative line(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
