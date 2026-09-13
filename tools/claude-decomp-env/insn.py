#!/usr/bin/env python3
"""Follow one instruction, or one pseudo, through GCC 2.8.1's RTL passes.

An attempt leaves ~13,000 lines of dumps across nine files, and the story of a
single instruction is one block in each of them. Reconstructing that by hand
means nine `sed -n 'START,ENDp'` calls and aligning the output yourself, which
is what the sweep logs show agents doing hundreds of times.

Insn uids are stable across passes and `build.sh` annotates the emitted asm
with them (`move $16,$4  # 4 movsi_internal2/1`), so there is an unbroken chain
from a mismatching output instruction back through every optimisation. This
walks it, and prints a pass only when that pass *changed* the insn - the
question is almost always "which pass did this to me", and printing nine
identical blocks buries the answer.

    ./insn.py 4                 # how insn 4 evolved
    ./insn.py --reg 80          # every pass that mentions pseudo 80
    ./insn.py --asm-line 5321   # start from a line of the emitted .s
"""
from __future__ import annotations
import argparse, pathlib, re, sys

# dump order as cc1 writes them; earlier passes first
PASSES = ["rtl", "jump", "cse", "loop", "cse2", "flow", "combine",
          "sched", "lreg", "greg", "sched2", "jump2", "dbr"]


def newest_base(scratch: pathlib.Path) -> pathlib.Path | None:
    cands = sorted(scratch.glob("*.i.lreg"), key=lambda p: p.stat().st_mtime)
    return pathlib.Path(str(cands[-1])[: -len(".i.lreg")]) if cands else None


def block(text: str, uid: int) -> str | None:
    """The parenthesis-balanced (insn UID …) form, as the dump wrote it."""
    m = re.search(rf"^\((insn|jump_insn|call_insn) {uid} ", text, re.M)
    if not m:
        return None
    i, depth = m.start(), 0
    for j in range(m.start(), len(text)):
        if text[j] == "(":
            depth += 1
        elif text[j] == ")":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
    return text[i:i + 2000]


def norm(s: str) -> str:
    return re.sub(r"\s+", " ", s).strip()


def uid_for_asm_line(base: pathlib.Path, line_no: int) -> int | None:
    asm = base.with_suffix(".s")
    if not asm.exists():
        return None
    lines = asm.read_text(errors="replace").splitlines()
    # search outward from the requested line for the nearest uid annotation
    for off in range(0, 40):
        for idx in (line_no - 1 + off, line_no - 1 - off):
            if 0 <= idx < len(lines):
                m = re.search(r"#\s+(\d+)\s+\w", lines[idx])
                if m:
                    return int(m.group(1))
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("uids", nargs="*", type=int)
    ap.add_argument("--reg", type=int, help="follow a pseudo instead of an insn")
    ap.add_argument("--asm-line", type=int, help="start from this line of the .s")
    ap.add_argument("--attempt", help="base name, e.g. base_3 (default: newest)")
    ap.add_argument("--scratch", default=".", help="scratch dir (default: cwd)")
    ap.add_argument("--all", action="store_true",
                    help="print every pass, not only the ones that changed it")
    args = ap.parse_args()

    scratch = pathlib.Path(args.scratch)
    base = (scratch / args.attempt) if args.attempt else newest_base(scratch)
    if base is None or not base.with_suffix(".i.lreg").exists():
        print(f"no dumps in {scratch} - run ./dump.sh <file.c> first", file=sys.stderr)
        return 1
    print(f"# {base.name}")

    dumps = {p: base.with_suffix(f".i.{p}") for p in PASSES}
    dumps = {p: f for p, f in dumps.items() if f.exists()}

    if args.reg is not None:
        pat = re.compile(rf"\(reg[:/][^)]*\b{args.reg}\b")
        print(f"\n=== pseudo {args.reg} ===")
        for p, f in dumps.items():
            hits = [l for l in f.read_text(errors="replace").splitlines() if pat.search(l)]
            if hits:
                print(f"\n--- {p} ({len(hits)} mention(s))")
                for h in hits[:6]:
                    print("   ", h.strip()[:120])
        return 0

    uids = list(args.uids)
    if args.asm_line:
        u = uid_for_asm_line(base, args.asm_line)
        if u is None:
            print(f"no insn annotation near .s line {args.asm_line}", file=sys.stderr)
            return 1
        print(f"# .s line {args.asm_line} -> insn {u}")
        uids.append(u)
    if not uids:
        ap.error("give an insn uid, --reg, or --asm-line")

    for uid in uids:
        print(f"\n=== insn {uid} ===")
        prev = None
        for p, f in dumps.items():
            b = block(f.read_text(errors="replace"), uid)
            if b is None:
                if prev is not None:
                    print(f"  {p:<8} GONE (deleted by this pass)")
                    prev = None
                continue
            if args.all or norm(b) != prev:
                head = "  " + p.ljust(8)
                print(head + b.replace("\n", "\n" + " " * 10))
                prev = norm(b)
    return 0


if __name__ == "__main__":
    sys.exit(main())
