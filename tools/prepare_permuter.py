#!/usr/bin/env python3
"""Apply repository-owned permuter fixes and refresh its cached parser tables."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
PERMUTER = ROOT / 'tools/decomp-permuter'


def main():
    for name in ('decomp-permuter-objdump.patch', 'decomp-permuter-sizeof.patch'):
        patch = ROOT / 'tools' / name
        command = ['git', '-C', str(PERMUTER), 'apply']
        ready = subprocess.run(command + ['--check', str(patch)], capture_output=True)
        if ready.returncode == 0:
            subprocess.run(command + [str(patch)], check=True)
            print(f'Applied {name}')
        else:
            applied = subprocess.run(command + ['--reverse', '--check', str(patch)], capture_output=True)
            if applied.returncode:
                sys.exit(f'Cannot apply {name}; inspect local permuter changes before searching.\n'
                         + ready.stderr.decode(errors='replace'))
    # optimize=True silently accepts stale yacc tables even after grammar edits.
    # False checks the grammar signature and regenerates only when necessary.
    sys.path.insert(0, str(PERMUTER))
    from perm_pycparser.c_parser import CParser
    CParser(yacc_optimize=False, taboutputdir=str(PERMUTER / 'perm_pycparser'))


if __name__ == '__main__':
    main()
