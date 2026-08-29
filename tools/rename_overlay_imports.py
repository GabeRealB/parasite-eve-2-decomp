#!/usr/bin/env python3
"""
Name overlay imports after the overlay that defines them.

`src/main` called 53 gameplay functions by ad-hoc `D_800xxxxx` labels while the
gameplay overlay defines the same addresses as `func_800xxxxx`. Nothing broke -
each binary has its own symbol map, and main's reference resolves through
`linkers/USA/undefined_syms_auto.main.txt` - but the two names for one function
mean a search from either side misses the other. That is how the TMD draw
handlers stayed "unmatched overlay asm" in my notes while being decompiled C in
`src/gameplay/gameplay.c` the whole time.

The rule: an import is named after the *defining* overlay. The reverse
direction (overlay code importing main symbols) already followed it - 0
mismatches - and is documented in DECOMPILATION_LEARNINGS.md.

Scoped to `src/main` and `include/main`. Main's asm and the auto-undef list
regenerate from these on the next split, and no overlay source mentions the old
names.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

RENAMES: list[tuple[str, str]] = [
    ("D_80099994", "func_80099994"),  # gameplay
    ("D_80099B94", "func_80099B94"),  # gameplay
    ("D_80099D40", "func_80099D40"),  # gameplay
    ("D_80099FF4", "func_80099FF4"),  # gameplay
    ("D_8009A348", "func_8009A348"),  # gameplay
    ("D_8009A57C", "func_8009A57C"),  # gameplay
    ("D_8009AF90", "func_8009AF90"),  # gameplay
    ("D_8009B2F4", "func_8009B2F4"),  # gameplay
    ("D_8009B500", "func_8009B500"),  # gameplay
    ("D_8009BD00", "func_8009BD00"),  # gameplay
    ("D_8009C024", "func_8009C024"),  # gameplay
    ("D_8009C414", "func_8009C414"),  # gameplay
    ("D_8009CED0", "func_8009CED0"),  # gameplay
    ("D_8009D0DC", "func_8009D0DC"),  # gameplay
    ("D_8009D388", "func_8009D388"),  # gameplay
    ("D_8009D518", "func_8009D518"),  # gameplay
    ("D_8009D718", "func_8009D718"),  # gameplay
    ("D_8009D900", "func_8009D900"),  # gameplay
    ("D_8009DB00", "func_8009DB00"),  # gameplay
    ("D_8009DCB8", "func_8009DCB8"),  # gameplay
    ("D_8009DE48", "func_8009DE48"),  # gameplay
    ("D_8009E048", "func_8009E048"),  # gameplay
    ("D_8009E274", "func_8009E274"),  # gameplay
    ("D_8009E4A0", "func_8009E4A0"),  # gameplay
    ("D_8009E770", "func_8009E770"),  # gameplay
    ("D_8009EAA4", "func_8009EAA4"),  # gameplay
    ("D_8009EB84", "func_8009EB84"),  # gameplay
    ("D_8009EC1C", "func_8009EC1C"),  # gameplay
    ("D_8009ECC0", "func_8009ECC0"),  # gameplay
    ("D_8009ED28", "func_8009ED28"),  # gameplay
    ("D_8009ED90", "func_8009ED90"),  # gameplay
    ("D_8009EE28", "func_8009EE28"),  # gameplay
    ("D_8009EECC", "func_8009EECC"),  # gameplay
    ("D_8009EF64", "func_8009EF64"),  # gameplay
    ("D_8009EFFC", "func_8009EFFC"),  # gameplay
    ("D_8009F0A0", "func_8009F0A0"),  # gameplay
    ("D_8009F144", "func_8009F144"),  # gameplay
    ("D_8009F1DC", "func_8009F1DC"),  # gameplay
    ("D_8009F280", "func_8009F280"),  # gameplay
    ("D_8009F360", "func_8009F360"),  # gameplay
    ("D_8009F3F8", "func_8009F3F8"),  # gameplay
    ("D_8009F49C", "func_8009F49C"),  # gameplay
    ("D_8009F504", "func_8009F504"),  # gameplay
    ("D_8009F56C", "func_8009F56C"),  # gameplay
    ("D_8009F670", "func_8009F670"),  # gameplay
    ("D_8009F708", "func_8009F708"),  # gameplay
    ("D_8009F824", "func_8009F824"),  # gameplay
    ("D_8009F8C8", "func_8009F8C8"),  # gameplay
    ("D_8009F970", "func_8009F970"),  # gameplay
    ("D_8009FA24", "func_8009FA24"),  # gameplay
    ("D_8009FB28", "func_8009FB28"),  # gameplay
    ("D_8009FC44", "func_8009FC44"),  # gameplay
    ("D_8009FC90", "func_8009FC90"),  # gameplay
]

TREES = ("src/main", "include/main")
EXTS = {".c", ".h"}


def main() -> int:
    pairs = sorted(RENAMES, key=lambda p: len(p[0]), reverse=True)
    files = subs = 0
    for tree in TREES:
        for path in (ROOT / tree).rglob("*"):
            if not path.is_file() or path.suffix not in EXTS:
                continue
            raw = path.read_text(encoding="utf-8")
            new = raw
            n = 0
            for old, repl in pairs:
                pat = re.compile(rf"(?<![A-Za-z0-9_]){re.escape(old)}(?![A-Za-z0-9_])")
                new, k = pat.subn(repl, new)
                n += k
            if n:
                path.write_text(new, encoding="utf-8")
                files += 1
                subs += n
    print(f"Updated {files} files ({subs} substitutions)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
