#!/usr/bin/env python3
"""Fetch decomp-permuter as pinned, with the repository's fixes (tools/config/fetch_tools.py applies
tools/patches/decomp-permuter-*.patch as it fetches), and refresh its cached parser tables."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'config'))
import fetch_tools  # noqa: E402


def main():
    try:
        permuter = fetch_tools.ensure('decomp-permuter')
    except fetch_tools.FetchError as error:
        sys.exit(f'prepare_permuter: {error}')
    # optimize=True silently accepts stale yacc tables even after grammar edits.
    # False checks the grammar signature and regenerates only when necessary.
    sys.path.insert(0, str(permuter))
    from perm_pycparser.c_parser import CParser
    CParser(yacc_optimize=False, taboutputdir=str(permuter / 'perm_pycparser'))


if __name__ == '__main__':
    main()
