#!/usr/bin/env python3
"""Build the shared imports file for an overlay family.

An overlay references main and gameplay by absolute address. splat resolves
those addresses against `global_vram_start`/`global_vram_end`, so it does not
list them as undefined - which means the link fails unless every one of them is
named in the family's imports file and appended to the undef script
(`append_overlay_absolute_imports` in ninja_config.py).

The set of referenced addresses is not guessed here. splat has already done the
analysis: any address it could not name appears in the split output verbatim as
`func_<VRAM>` or `D_<VRAM>`, so scanning the generated sources recovers exactly
the reference set, relocation handling included.

Names come from the map of the overlay that *defines* the symbol - main
symbols from sym.main.txt, gameplay symbols from sym.gameplay.txt - so an
overlay calls a function by the same name its owner uses. Addresses with no
entry in either map keep their `func_`/`D_` form; they are real references to
still-unnamed code, and naming them is a separate job.

This is a maintenance step, not a build step: run it after adding a family (or
after a naming pass), then re-split so the generated sources pick up the names.
"""

from __future__ import annotations

import argparse
import re
import sys
import tomllib
from pathlib import Path

MANIFEST = Path("configs/USA/overlays.toml")
SYM_MAPS = [
    ("main", Path("configs/USA/sym.main.txt")),
    ("gameplay", Path("configs/USA/sym.gameplay.txt")),
]
TOKEN = re.compile(r"\b(func|D)_([0-9A-F]{8})\b")
NAME_TOKEN = re.compile(r"\b[A-Za-z_]\w*\b")
SYM_LINE = re.compile(r"^\s*(\w+)\s*=\s*(0x[0-9A-Fa-f]+)\s*;(.*)$")


def load_sym_maps() -> tuple[dict[int, tuple[str, str]], dict[str, int]]:
    """(address -> (name, owner), name -> address)."""
    out: dict[int, tuple[str, str]] = {}
    by_name: dict[str, int] = {}
    for owner, path in SYM_MAPS:
        if not path.is_file():
            continue
        for line in path.read_text(encoding="utf-8").splitlines():
            m = SYM_LINE.match(line)
            if m:
                addr = int(m.group(2), 16)
                out.setdefault(addr, (m.group(1), owner))
                by_name.setdefault(m.group(1), addr)
    return out, by_name


def referenced(family: str, by_name: dict[str, int]) -> dict[int, str]:
    """Addresses the family's split output references outside itself.

    Two forms have to be collected, and missing either one breaks the tool:

    * ``func_<VRAM>`` / ``D_<VRAM>`` - splat could not name the address. splat
      prefixes an overlay's *own* symbols with the segment name
      (``func_m93r_8011D1C4``), so a bare token is by construction external.
      Where it points is irrelevant: rooms reference ``D_80188888`` in the
      dialogue block packed *after* the overlay, and two of them call addresses
      past the end of 2 MB RAM.
    * a **named** main or gameplay symbol. Once this file gives an address a
      name, the next split emits that name instead of ``func_<VRAM>``. Scanning
      only for the raw form would therefore drop every entry the previous run
      added - the tool would erase its own output on the second run.

    The ``func_`` / ``D_`` prefix is kept for the raw form: it is splat's own
    call on whether the address is code or data, and an import emitted under the
    wrong one would rename a data symbol into a function.
    """
    found: dict[int, str] = {}
    for root in (Path("src") / family, Path("asm/USA") / family):
        if not root.is_dir():
            continue
        for path in list(root.rglob("*.c")) + list(root.rglob("*.s")):
            text = path.read_text(encoding="utf-8", errors="replace")
            for prefix, hit in TOKEN.findall(text):
                found[int(hit, 16)] = prefix
            for word in NAME_TOKEN.findall(text):
                addr = by_name.get(word)
                if addr is not None:
                    found.setdefault(addr, "func")
    return found


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("family", nargs="?", help="family to regenerate (default: all)")
    args = ap.parse_args()

    manifest = tomllib.loads(MANIFEST.read_text(encoding="utf-8"))
    syms, by_name = load_sym_maps()
    rc = 0
    for family, spec in manifest.items():
        if args.family and family != args.family:
            continue
        roots = [Path("src") / family, Path("asm/USA") / family]
        if not any(r.is_dir() for r in roots):
            print(f"{family}: no split output to scan - split first", file=sys.stderr)
            rc = 1
            continue
        addrs = referenced(family, by_name)
        # A family with no references is normal, not a failure: several are
        # pure data (all 24 map pictures and the debug name table contain no
        # code at all), so they import nothing and still need the file to exist
        # for the config to load.

        named = [(a, *syms[a]) for a in sorted(addrs) if a in syms]  # noqa: E501
        unnamed = sorted(a for a in addrs if a not in syms)

        lines = [
            f"// main and gameplay symbols referenced by the {family} overlays.",
            "// splat 0.50: these are not in the overlay binary, so they must be absolute.",
            "//",
            "// GENERATED by tools/gen_overlay_imports.py - do not edit by hand.",
            "// Names follow the overlay that defines each symbol, so a call reads the",
            "// same here as it does in main/ or gameplay/.",
            "",
        ]
        for owner, _p in SYM_MAPS:
            group = [(a, n) for a, n, o in named if o == owner]
            if not group:
                continue
            lines.append(f"// --- from {owner} ({len(group)} symbols)")
            lines += [f"{n} = 0x{a:08X}; // absolute:True" for a, n in group]
            lines.append("")
        if not addrs:
            lines.append("// This family contains no code, so it imports nothing.")
            lines.append("")
        if unnamed:
            lines.append(f"// --- still unnamed in their own overlay ({len(unnamed)})")
            lines += [f"{addrs[a]}_{a:08X} = 0x{a:08X}; // absolute:True" for a in unnamed]
            lines.append("")

        dest = Path(spec["imports"])
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text("\n".join(lines), encoding="utf-8")
        print(
            f"{family}: {len(addrs)} references -> {dest} "
            f"({len(named)} named, {len(unnamed)} unnamed)"
        )
    return rc


if __name__ == "__main__":
    sys.exit(main())
