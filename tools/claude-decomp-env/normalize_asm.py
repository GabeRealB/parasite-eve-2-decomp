#!/usr/bin/env python3
"""Normalize assembly for easier comparison by converting jumptable symbols to .rodata"""

import re
import sys


def normalize_jumptable_references(line: str) -> str:
    """Normalize jump table references to .rodata for consistent comparison"""
    # Splat names and actor tables renamed by their overlay-relative offset.
    jtbl_pattern = r"\b(?:jtbl_\w+|Actor[0-9A-Fa-f]+_Jt[0-9A-Fa-f]+)\b"

    # Replace jump table references with .rodata
    normalized = re.sub(jtbl_pattern, ".rodata", line)
    return normalized


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: normalize_asm.py <file>")
        sys.exit(1)

    with open(sys.argv[1], "r") as f:
        for line in f:
            print(normalize_jumptable_references(line), end="")
