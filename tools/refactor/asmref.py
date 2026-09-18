"""Assembly-side cross-referencing for the refactor CLIs.

C tooling stops at the edge of the disassembly, but a symbol can be reached from
there in two ways, and they need opposite handling:

* **By name.** The disassembler resolves most operands, so a reference reads as
  a call or a hi/lo address load naming the symbol. These are exact, and a
  rename must carry them - but the assembly is *generated*, so the fix is to
  rename in the symbol map the splitter reads and re-split, never to edit the
  assembly in place.

* **By address.** A bare data word, or a hi/lo pair the splitter did not
  attribute, names nothing. A rename cannot update it, so the reference
  silently keeps pointing at whatever now lives at that location.

An address does not identify a symbol on its own. Overlays that load at the same
base address, and separately linked images that share a start address, give the
same address different meanings in different contexts, so a large minority of
addresses in the symbol maps carry more than one name. Any address-based answer
therefore has to report the scope it came from and whether it is unique.
"""

from __future__ import annotations

import collections
import glob
import os
import re
import subprocess

_SYM_RE = re.compile(r"^\s*([A-Za-z_]\w*)\s*=\s*(0x[0-9A-Fa-f]+)\s*;")


DEFAULT_VERSION = "USA"


def sym_files(root: str, version: str = DEFAULT_VERSION) -> list[str]:
    """Every symbol map for a version, at whatever depth it is nested."""
    out = []
    base = f"configs/{version}"
    for pat in (f"{base}/sym*.txt", f"{base}/sym/*.txt", f"{base}/sym/*/*.txt"):
        out += sorted(glob.glob(os.path.join(root, pat)))
    return [os.path.relpath(p, root) for p in out]


def asm_root(version: str = DEFAULT_VERSION) -> str:
    return f"asm/{version}"


def _scope_of(sym_file: str) -> str:
    """Which image a symbol map describes; imported names belong to the importer."""
    base = os.path.basename(sym_file)
    if base.startswith("sym.") and base.endswith(".txt"):
        core = base[4:-4]
        return core.replace(".imports", "") + (" (imports)" if ".imports" in core else "")
    parent = os.path.basename(os.path.dirname(sym_file))
    if base.endswith(".imports.txt"):
        return base[:-len(".imports.txt")] + " (imports)"
    return f"{parent}/{base[:-4]}"


class SymbolMap:
    """name -> addresses and address -> names, each with its originating scope."""

    def __init__(self, root: str, version: str = DEFAULT_VERSION):
        self.version = version
        self.by_name: dict[str, set[tuple[int, str]]] = collections.defaultdict(set)
        self.by_addr: dict[int, set[tuple[str, str]]] = collections.defaultdict(set)
        for f in sym_files(root, version):
            scope = _scope_of(f)
            try:
                text = open(os.path.join(root, f), errors="replace")
            except OSError:
                continue
            for line in text:
                m = _SYM_RE.match(line)
                if not m:
                    continue
                name, addr = m.group(1), int(m.group(2), 16)
                self.by_name[name].add((addr, scope))
                self.by_addr[addr].add((name, scope))

    def addresses(self, name: str) -> list[tuple[int, str]]:
        return sorted(self.by_name.get(name, ()))

    def owners(self, addr: int) -> list[tuple[str, str]]:
        return sorted(self.by_addr.get(addr, ()))

    def is_unique(self, addr: int) -> bool:
        return len({n for n, _ in self.owners(addr)}) <= 1


# --------------------------------------------------------------------------
# scanning asm/
# --------------------------------------------------------------------------

_DEF_RE = re.compile(r"^\s*(glabel|dlabel|endlabel|nonmatching|jlabel)\s+(\S+)")


def classify_asm_line(line: str, name: str) -> str:
    m = _DEF_RE.match(line)
    if m and m.group(2) == name:
        return {"glabel": "definition", "dlabel": "data-definition",
                "endlabel": "definition-end", "nonmatching": "definition-marker",
                "jlabel": "jump-target"}[m.group(1)]
    body = line.split("*/", 1)[-1]
    if re.search(rf"\bjal\b\s+{re.escape(name)}\b", body):
        return "call (jal)"
    if re.search(rf"\bj\b\s+{re.escape(name)}\b", body):
        return "tail-jump (j)"
    if re.search(rf"%hi\(\s*{re.escape(name)}\s*\)", body):
        return "address-load (%hi)"
    if re.search(rf"%lo\(\s*{re.escape(name)}\s*\)", body):
        return "address-load (%lo)"
    if re.search(rf"\.word\s+{re.escape(name)}\b", body):
        return "pointer (.word)"
    return "mention"


def asm_name_refs(root: str, name: str,
                  version: str = DEFAULT_VERSION) -> list[tuple[str, int, str, str]]:
    """(file, line, kind, text) for every assembly file naming the symbol."""
    try:
        out = subprocess.run(
            ["grep", "-rnw", "--include=*.s", name, asm_root(version)],
            cwd=root, capture_output=True, text=True, timeout=600,
        ).stdout.splitlines()
    except Exception:
        return []
    refs = []
    for row in out:
        try:
            path, lineno, text = row.split(":", 2)
        except ValueError:
            continue
        refs.append((path, int(lineno), classify_asm_line(text, name), text.strip()))
    return refs


_WORD_RE = re.compile(r"\.word\s+(0x[0-9A-Fa-f]{8})\b")


def asm_addr_refs(root: str, addr: int,
                  version: str = DEFAULT_VERSION) -> list[tuple[str, int, str, str]]:
    """Literal whole-word uses of an address that the splitter left unnamed.

    Only data words are reported. An unresolved hi/lo instruction pair splits
    the address across two half-immediates that cannot be recombined without
    decoding the instruction stream, so those are out of scope - which is why
    this check reports a possible reference rather than proving one.
    """
    pat = f"0x{addr:08X}"
    try:
        out = subprocess.run(
            ["grep", "-rniF", "--include=*.s", f".word {pat}", asm_root(version)],
            cwd=root, capture_output=True, text=True, timeout=600,
        ).stdout.splitlines()
    except Exception:
        return []
    refs = []
    for row in out:
        try:
            path, lineno, text = row.split(":", 2)
        except ValueError:
            continue
        refs.append((path, int(lineno), "pointer (.word literal)", text.strip()))
    return refs


def report(root: str, name: str, smap: SymbolMap, show_lines: bool = True,
           version: str = DEFAULT_VERSION) -> None:
    addrs = smap.addresses(name)
    if not addrs:
        print(f"\nassembly: {name} is in no symbol map for {version} "
              f"(so nothing there can reference it by address)")
    else:
        print(f"\naddress of {name}:")
        for addr, scope in addrs:
            owners = [n for n, _ in smap.owners(addr) if n != name]
            if owners:
                print(f"  {addr:#010x}  [{scope}]  SHARED with {', '.join(sorted(set(owners)))}")
            else:
                print(f"  {addr:#010x}  [{scope}]  unique")

    named = asm_name_refs(root, name, version)
    if named:
        kinds = collections.Counter(k for _, _, k, _ in named)
        files = {f for f, _, _, _ in named}
        print(f"\nassembly references by name: {len(named)} in {len(files)} file(s)")
        for k, n in kinds.most_common():
            print(f"  {k:<24} {n}")
        if show_lines:
            for f, ln, k, text in named[:40]:
                print(f"    {f}:{ln}  {k:<24} {text[:90]}")
            if len(named) > 40:
                print(f"    ... {len(named) - 40} more")
        print(f"  NOTE: files under {asm_root(version)} are generated. Rename in the")
        print(f"        symbol map under configs/{version} and re-split; do not edit them.")

    for addr, scope in addrs:
        lit = asm_addr_refs(root, addr, version)
        if not lit:
            continue
        print(f"\nassembly references to {addr:#010x} as a literal: {len(lit)}")
        if not smap.is_unique(addr):
            print(f"  WARNING: this address is shared by "
                  f"{', '.join(sorted({n for n, _ in smap.owners(addr)}))} -")
            print("  a literal cannot be attributed to one of them without knowing")
            print("  which image the referring file belongs to.")
        for f, ln, k, text in lit[:20]:
            print(f"    {f}:{ln}  {text[:90]}")
        if len(lit) > 20:
            print(f"    ... {len(lit) - 20} more")
