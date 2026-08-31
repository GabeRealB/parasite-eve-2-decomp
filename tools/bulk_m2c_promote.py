#!/usr/bin/env python3
"""Promote already-matched shared bodies into their family's shared library.

Overlays in a family are separate link outputs that carry copies of the same
code. Landing a body in one overlay leaves every copy to be matched again, so a
matched body that repeats is worth promoting: one object, linked into each
carrier. `tools/overlay_dup_index.py promote` does the config half -- the span
in configs/USA/overlays.toml for each carrier and the shared symbol in their
symbol maps. This does the source half and verifies the result:

  1. run promote, and read the shared unit and symbol out of its output;
  2. move the body into src/<family>/lib/<unit>.c under that symbol, with the
     declarations it needs and the fewest includes that compile;
  3. delete the body from its own overlay's .c, and delete the now-covered
     INCLUDE_ASM line from every other carrier -- splat never rewrites an
     existing .c, so a leftover declaration breaks the link;
  4. `ninja_config.py`, then the bare unscoped build. Only that proves it, since
     a promotion changes the link layout of every carrier at once.

Anything short of a clean unscoped build reverts the whole promotion, configs
included. That matters more here than in landing: `promote` is not atomic -- a
crash partway leaves symbol maps already written -- and a promotion touches up
to twenty overlays, so there is no such thing as isolating one bad file.

Runs under the orchestrator merge lock, one promotion per commit.

Usage
-----
    python3 tools/bulk_m2c_promote.py --func func_actor_120400_801327B4 --dry-run
    python3 tools/bulk_m2c_promote.py --results .bulk_m2c/results.jsonl --limit 5 --commit
"""

from __future__ import annotations

import argparse
import contextlib
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import NamedTuple, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bulk_m2c as B  # noqa: E402
import bulk_m2c_land as L  # noqa: E402
import decomp_overlay as ovl  # noqa: E402
import overlay_dup_index as D  # noqa: E402

REPO_ROOT = B.REPO_ROOT
DUP = REPO_ROOT / "tools" / "overlay_dup_index.py"
NINJA = REPO_ROOT / "ninja_config.py"

# "ActorsShared801327b4: 9 of 17 copies share src/actors/lib/actors_shared_801327b4.c"
HEAD_RE = re.compile(
    r"^(\w+):\s+(\d+) of (\d+) copies share (\S+\.c)\s*$", re.M
)


def sh(cmd, cwd: Path = REPO_ROOT, **kw):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True, **kw)


def git(*args):
    return sh(["git", *args])


# --- C surgery ---------------------------------------------------------------

def find_definition(text: str, func: str) -> Optional[tuple[int, int]]:
    """(start, end) of a function definition in a source file, by brace match."""
    m = re.search(rf"^[A-Za-z_][\w \*]*\b{re.escape(func)}\s*\([^;{{]*\)\s*\{{",
                  text, re.M)
    if not m:
        return None
    depth, i = 0, m.end() - 1
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return m.start(), i + 1
        i += 1
    return None


def externs_used(text: str, body: str) -> list[str]:
    """File-scope declarations from the host that the body actually names."""
    names = set(L.IDENT_RE.findall(body))
    out = []
    for line in text.splitlines():
        m = L.EXTERN_RE.match(line)
        if m and m.group(1) in names:
            out.append(line)
    return out


def includes_of(text: str) -> list[str]:
    return [l for l in text.splitlines() if l.startswith("#include")]


# --- unit re-partitioning ----------------------------------------------------
#
# A shared span cuts the overlay's `.text` in three: the run before it, the
# shared object, and the run after it. gen_overlay_configs numbers those runs
# `<overlay>`, `<overlay>_2`, ... in address order, so inserting a span pushes
# the tail of the unit that held the body into a *new* unit, and renumbers every
# unit after it. splat never rewrites an existing `.c`, so without doing that
# ourselves the old file keeps INCLUDE_ASM lines naming a directory splat no
# longer writes, and the assembler fails with
#
#   Error: can't open asm/USA/<family>/nonmatchings/<ov>/<ov>/func_….s
#
# which the link then reports only as a missing `.o`. Deleting just the promoted
# body's own line is not enough.

# An item is anything that occupies address space in the unit: a stub or a body.
ITEM = re.compile(
    r"^(?:INCLUDE_ASM|INCLUDE_RODATA)\(|^[A-Za-z_][\w \*]*\b\w+\s*\([^;{]*\)\s*\{",
    re.M)


def unit_component(overlay: str, index: int) -> str:
    """The unit's name in an INCLUDE_ASM path; unit 1 carries no suffix."""
    return overlay if index == 1 else f"{overlay}_{index}"


def unit_index(path: Path, overlay: str) -> int:
    if path.stem == overlay:
        return 1
    m = re.fullmatch(rf"{re.escape(overlay)}_(\d+)", path.stem)
    return int(m.group(1)) if m else 0


def renumber(text: str, overlay: str, old: int, new: int) -> str:
    return text.replace(f'/{overlay}/{unit_component(overlay, old)}"',
                        f'/{overlay}/{unit_component(overlay, new)}"')


def renumber_manifest(overlay: str, after: int) -> None:
    """Shift the overlay's `rodata` cuts to match the units that just moved.

    A rodata cut names the `c` unit that owns the block, so inserting a shared
    span ahead of that unit renames it and the cut has to follow. Without this
    the renamed unit's INCLUDE_RODATA lines point at a directory the split does
    not write, and the assembler reports a missing `D_<overlay>_….s`.
    """
    path = REPO_ROOT / "configs" / "USA" / "overlays.toml"
    text = path.read_text(encoding="utf-8")
    m = re.search(rf"^{re.escape(overlay)} = \{{(.*)\}}$", text, re.M)
    if not m:
        return

    def bump(hit: re.Match) -> str:
        i = int(hit.group(1))
        return (f'unit = "{unit_component(overlay, i + 1)}"' if i > after
                else hit.group(0))

    body = re.sub(rf'unit = "{re.escape(overlay)}_(\d+)"', bump, m.group(1))
    if body == m.group(1):
        return
    path.write_text(text[: m.start()] + f"{overlay} = {{ {body} }}"
                    + text[m.end():], encoding="utf-8")


class UnitCut(NamedTuple):
    """A run of a unit that a shared span pushed into a new unit."""
    family: str
    overlay: str
    index: int          # the new unit's number
    head: list[str]     # functions that stayed
    tail: list[str]     # functions that moved


def functions_in(text: str) -> list[str]:
    """Every function the fragment occupies address space for, in order."""
    out = []
    for line in text.splitlines():
        m = ovl.INCLUDE_ASM_RE.search(line)
        if m:
            out.append(m.group(2))
            continue
        m = re.match(r"^[A-Za-z_][\w \*]*\b(\w+)\s*\([^;{]*\)\s*\{", line)
        if m:
            out.append(m.group(1))
    return out


def cut_unit(host: Path, start: int, end: int,
             pending: "list[Pending] | None" = None,
             cuts: "list[UnitCut] | None" = None) -> tuple[bool, str]:
    """Remove [start, end) from `host` and give what follows its own unit.

    Whatever the tail already had decompiled is appended to `pending`, to be put
    back once the split has written the new unit's file.
    """
    overlay = host.parent.name
    text = host.read_text()
    head, tail = text[:start], text[end:]
    first = ITEM.search(text)
    preamble = text[: first.start()] if first else text

    # Nothing after the span stays in this overlay's own code, or nothing
    # before it does: either way the span does not split a run in two, so the
    # numbering is unchanged and the body just goes away.
    if not ITEM.search(tail) or not ITEM.search(head):
        host.write_text((head + tail).replace("\n\n\n\n", "\n\n"))
        return True, ""
    k = unit_index(host, overlay)
    src_dir = host.parent
    if pending is not None:
        target = src_dir / f"{unit_component(overlay, k + 1)}.c"
        for m in re.finditer(r"^[A-Za-z_][\w \*]*\b(\w+)\s*\([^;{]*\)\s*\{",
                             tail, re.M):
            span = find_definition(tail, m.group(1))
            if span:
                body = tail[span[0]:span[1]]
                # The file-scope declarations too, not just the #includes: a
                # body that names `extern D_<overlay>_…` stops compiling the
                # moment it moves to a file whose preamble does not declare it.
                pending.append(Pending(target, m.group(1), body,
                                       includes_of(preamble)
                                       + externs_used(text, body)))
    sibs = {unit_index(q, overlay): q for q in src_dir.glob("*.c")}
    if k == 0 or 0 in sibs:
        return False, f"{src_dir}: a source file is not named <overlay>[_N].c"

    renumber_manifest(overlay, k)
    # Highest first, so a rename never lands on a file still to be moved.
    for i in sorted((i for i in sibs if i > k), reverse=True):
        old_path = sibs[i]
        new_path = src_dir / f"{unit_component(overlay, i + 1)}.c"
        new_path.write_text(renumber(old_path.read_text(), overlay, i, i + 1))
        if new_path != old_path:
            old_path.unlink()

    # The tail file is *not* written here. splat has to author it: rodata a
    # function owns follows the function, so the block that used to sit in this
    # unit is re-migrated across the cut, and only splat knows the new
    # INCLUDE_RODATA lines. Writing the file ourselves left the moved functions
    # with `undefined reference to D_<overlay>_…`. Leave the slot empty, let the
    # split fill it, and put the decompiled bodies back afterwards.
    host.write_text(head.rstrip() + "\n")
    if cuts is not None:
        cuts.append(UnitCut(src_dir.parent.name, overlay, k + 1,
                            functions_in(head), functions_in(tail)))
    return True, ""


class Pending(NamedTuple):
    """A decompiled body the split is about to hand back as an INCLUDE_ASM."""
    target: Path
    func: str
    body: str
    preamble: list[str]   # #includes and the file-scope decls the body names


def stub_of(text: str, func: str) -> Optional[re.Match]:
    return re.search(rf"^INCLUDE_ASM\([^\n]*?\b{re.escape(func)}\s*\);[ \t]*$",
                     text, re.M)


def reinject(pending: "list[Pending]") -> tuple[bool, str]:
    """Put the decompiled bodies back into the unit files the split just wrote.

    A body already matched has no INCLUDE_ASM to replace: splat writes a stub
    only for what is still in `nonmatchings`, so the file it just authored has a
    hole exactly where such a body belongs. Its place is still known - the order
    the bodies had in the unit they came from - so anchor it on the next body
    that *does* have a stub, and fall back to the end of the file.
    """
    by_target: dict[Path, list[Pending]] = {}
    for item in pending:
        by_target.setdefault(item.target, []).append(item)

    for target, items in by_target.items():
        if not target.is_file():
            return False, f"the split did not write {target}"
        text = target.read_text()
        for i, item in enumerate(items):
            # The split can carry a matched body over into the file it writes,
            # already formatted. Re-injecting it then defines the function
            # twice, which GCC reports as `redefinition of …` far from here.
            if find_definition(text, item.func):
                continue
            here = stub_of(text, item.func)
            if here is not None:
                text = text[: here.start()] + item.body + text[here.end():]
                continue
            anchor = next(
                (m.start() for later in items[i + 1:]
                 for m in [stub_of(text, later.func)] if m), None)
            if anchor is None:
                text = text.rstrip() + "\n\n" + item.body + "\n"
            else:
                text = text[:anchor] + item.body + "\n\n" + text[anchor:]
        missing = []
        for item in items:
            for line in item.preamble:
                if line not in text and line not in missing:
                    missing.append(line)
        if missing:
            lines = text.splitlines()
            at = max((n for n, l in enumerate(lines) if l.startswith("#include")),
                     default=-1) + 1
            text = "\n".join(lines[:at] + missing + lines[at:]) + "\n"
        target.write_text(text)
    return True, ""


# --- leading-rodata ownership ------------------------------------------------
#
# The leading rodata is one subsegment owned by the overlay's first code unit.
# Cutting a unit in two leaves the functions that moved referencing symbols
# that still belong to the unit they left, and the link says
# `undefined reference to D_<overlay>_…`. The manifest's `rodata` key cuts the
# block where ownership changes; the cut point is the lowest leading-rodata
# address the moved functions reach, and it is only sound when everything the
# functions that stayed reach sits below it.

RODATA_REF = re.compile(r"%(?:hi|lo)\((?:D|jtbl)_(\w+?)_([0-9A-F]{8})\)")


def overlay_geometry(overlay: str) -> tuple[int, int] | None:
    """(load vram, offset of the first code byte) from the generated config."""
    path = REPO_ROOT / "configs" / "USA" / "generated" / f"{overlay}.yaml"
    if not path.is_file():
        return None
    text = path.read_text()
    vram = re.search(r"^\s*vram:\s*(0x[0-9A-Fa-f]+)", text, re.M)
    code = re.search(r"^\s*- \[(0x[0-9A-Fa-f]+), c, ", text, re.M)
    if not vram or not code:
        return None
    return int(vram.group(1), 16), int(code.group(1), 16)


def rodata_offsets(cut: UnitCut, funcs: list[str], load: int, limit: int) -> set[int]:
    """Leading-rodata offsets these functions reference, from their asm."""
    out: set[int] = set()
    root = REPO_ROOT / "asm" / "USA" / cut.family
    for func in funcs:
        for marker in ("nonmatchings", "matchings"):
            for path in (root / marker / cut.overlay).rglob(f"{func}.s"):
                for _name, vram in RODATA_REF.findall(path.read_text(errors="ignore")):
                    off = int(vram, 16) - load
                    if 0 <= off < limit:
                        out.add(off)
    return out


def plan_rodata(cuts: list[UnitCut]) -> tuple[dict[str, tuple[int, str]], str]:
    """The `rodata` cut each overlay needs, or a reason it cannot have one."""
    plan: dict[str, tuple[int, str]] = {}
    for cut in cuts:
        geom = overlay_geometry(cut.overlay)
        if geom is None:
            return {}, f"{cut.overlay}: no generated config to read the layout from"
        load, text_start = geom
        if text_start == 0:
            continue  # no leading rodata at all
        tail = rodata_offsets(cut, cut.tail, load, text_start)
        if not tail:
            continue
        head = rodata_offsets(cut, cut.head, load, text_start)
        at = min(tail)
        if head and max(head) >= at:
            return {}, (f"{cut.overlay}: leading rodata is interleaved across the "
                        f"cut (stays reaches 0x{max(head):X}, moves reaches "
                        f"0x{at:X}); it needs a hand-placed rodata/units cut")
        plan[cut.overlay] = (at, f"{cut.overlay}_{cut.index}")
    return plan, ""


def write_rodata_cuts(plan: dict[str, tuple[int, str]]) -> tuple[bool, str]:
    path = REPO_ROOT / "configs" / "USA" / "overlays.toml"
    text = path.read_text(encoding="utf-8")
    for overlay, (at, unit) in plan.items():
        m = re.search(rf"^{re.escape(overlay)} = \{{(.*)\}}$", text, re.M)
        if not m:
            return False, f"{overlay}: no manifest entry to add a rodata cut to"
        body = m.group(1).strip()
        entry = f'{{ start = "0x{at:X}", unit = "{unit}" }}'
        existing = re.search(r"rodata = \[(.*?)\]", body)
        if existing:
            items = existing.group(1).strip()
            body = body.replace(existing.group(0),
                                f"rodata = [{items}, {entry}]" if items
                                else f"rodata = [{entry}]")
        else:
            body = f"{body}, rodata = [{entry}]" if body else f"rodata = [{entry}]"
        text = text[: m.start()] + f"{overlay} = {{ {body} }}" + text[m.end():]
    path.write_text(text, encoding="utf-8")
    return True, ""


# --- one promotion -----------------------------------------------------------

class Promotion:
    def __init__(self, func: str):
        self.func = func
        self.note = ""
        self.symbol = ""
        self.unit_path: Optional[Path] = None
        self.sharers: list[str] = []
        self.pending: list[Pending] = []
        self.cuts: list[UnitCut] = []

    # -- helpers
    def _carriers(self) -> list[dict]:
        data = D.load(rebuild=True, families=None)
        cl = D.classes(data, "text")
        hit = next((f for f in data["functions"] if f["name"] == self.func), None)
        if hit is None:
            return []
        fam = hit["overlay"].split("/")[1]
        return [c for c in cl[hit["text"]] if c["overlay"].split("/")[1] == fam]

    def run(self, dry_run: bool) -> bool:
        proc = sh([sys.executable, str(DUP), "promote", self.func])
        out = proc.stdout + proc.stderr
        m = HEAD_RE.search(out)
        if proc.returncode != 0 or not m:
            first = next((l for l in out.strip().splitlines() if l.strip()), "")
            self.note = f"promote failed: {first[:150]}"
            return False

        self.symbol, shared_n, total_n, unit = m.group(1), m.group(2), m.group(3), m.group(4)
        self.unit_path = REPO_ROOT / unit
        self.note = f"{shared_n}/{total_n} copies"

        # Which overlays actually gained the span. Taken from the manifest git
        # diff rather than the copy list, because promote excludes carriers that
        # contain the body twice and those stay stubbed.
        diff = git("diff", "-U0", "--", "configs/USA/overlays.toml").stdout
        unit_name = self.unit_path.stem
        self.sharers = [
            mm.group(1) for line in diff.splitlines() if line.startswith("+")
            for mm in [re.match(r"\+(\w+) = \{.*" + re.escape(unit_name), line)]
            if mm
        ]
        if not self.sharers:
            self.note = "promote wrote no spans"
            return False
        if dry_run:
            return True
        return self._move_body()

    def _move_body(self) -> bool:
        loc = ovl.find_function(self.func)
        if loc is None:
            self.note = "cannot locate origin"
            return False
        hosts = B.include_asm_index()
        origin = hosts.get(self.func)
        if origin is None:
            # Already decompiled: its stub is gone, so find the file by body.
            for p in (REPO_ROOT / "src").rglob("*.c"):
                if find_definition(p.read_text(errors="ignore"), self.func):
                    origin = p
                    break
        if origin is None or not origin.is_file():
            self.note = "cannot find the origin source"
            return False

        text = origin.read_text()
        span = find_definition(text, self.func)
        if span is None:
            self.note = f"no definition of {self.func} in {origin}"
            return False
        body = text[span[0]:span[1]]

        # The shared unit needs the body's declarations too; the origin keeps
        # its own copies for whatever else in the file uses them.
        decls = externs_used(text, body)
        shared_body = re.sub(rf"\b{re.escape(self.func)}\b", self.symbol, body, count=1)
        header = "\n".join(includes_of(text) or ['#include "common.h"'])
        content = header + "\n\n" + ("\n".join(decls) + "\n\n" if decls else "") \
            + shared_body + "\n"
        self.unit_path.parent.mkdir(parents=True, exist_ok=True)
        self.unit_path.write_text(content)

        # Drop it from its own overlay, re-cutting the unit around the span.
        ok, why = cut_unit(origin, span[0], span[1], self.pending, self.cuts)
        if not ok:
            self.note = why
            return False

        # And drop every other carrier's INCLUDE_ASM, which the span now covers.
        by_overlay = {c["overlay"].split("/")[-1]: c["name"] for c in self._carriers()}
        for overlay in self.sharers:
            name = by_overlay.get(overlay)
            if not name or name == self.func:
                continue
            host = hosts.get(name)
            if host is None or not host.is_file():
                continue
            ht = host.read_text()
            stub = L.find_stub(host, name)
            if not stub:
                continue
            at = ht.find(stub)
            ok, why = cut_unit(host, at, at + len(stub) + 1, self.pending, self.cuts)
            if not ok:
                self.note = why
                return False
        return True


# --- driver ------------------------------------------------------------------

# Not under build/: `build-and-verify.sh` clears that tree, and any other
# session running a build clears it too, so a log written there is gone
# before it can be read.
LOGDIR = Path(os.environ.get("PROMOTE_LOG_DIR", REPO_ROOT / ".promote-logs"))

# splat prints an "Error: Unable to determine a segment for the following
# user-declared symbols" block, at warn level, for every hardware address in a
# symbol map (scratchpad, I/O registers). `log.error` under it is commented out
# in splat 0.50, so it is noise, and it is the *last* thing on stderr. Reporting
# only the stderr tail therefore reported that block for every failure, whatever
# the failure was. Keep the whole log and quote the line that actually failed.
NOISE = re.compile(
    r"Unable to determine a segment|Suspected segments:|"
    r"Try specifying the segment|globally visible and take priority|"
    r"user attribute instead|not part of any segment|it/s\]|"
    r"Adding user-declared symbol|address of the symbol is outside|"
    r"accepted for now, but may become|^\s*$"
)

# What a real failure looks like, as opposed to splat talking about symbol maps.
FAIL = re.compile(
    r"\berror\b|FAILED|Traceback|undefined reference|multiple definition|"
    r"No rule to make|Assertion|does not match|MISMATCH|ninja: build stopped|"
    r"KeyError|Exception", re.I)


def _log(name: str, proc) -> Path:
    LOGDIR.mkdir(parents=True, exist_ok=True)
    path = LOGDIR / name
    path.write_text(f"$ rc={proc.returncode}\n--- stdout ---\n{proc.stdout}"
                    f"\n--- stderr ---\n{proc.stderr}")
    return path


def signal_lines(text: str, n: int = 10) -> str:
    """The lines that describe the failure, not splat's per-symbol noise.

    Quoting the tail is what hid the real error: splat ends a split whose symbol
    map holds hardware addresses with a warn-level block about them, so the tail
    of stderr was that block whatever went wrong. Prefer lines that look like a
    failure, and fall back to the tail only when none do.
    """
    keep = [l for l in text.splitlines() if l.strip() and not NOISE.search(l)]
    hits = [l for l in keep if FAIL.search(l)]
    return "\n".join((hits or keep)[-n:])


def run_split(tag: str = "run") -> tuple[bool, str]:
    py = sys.executable
    venv = REPO_ROOT / "venv" / "bin" / "python3"
    if venv.is_file():
        py = str(venv)
    proc = sh([py, str(NINJA)])
    path = _log(f"{tag}.ninja.log", proc)
    if proc.returncode != 0:
        return False, f"ninja_config failed ({path}):\n" + signal_lines(
            proc.stdout + "\n" + proc.stderr)
    return True, ""


def verify_full(tag: str = "run") -> tuple[bool, str]:
    proc = sh([str(REPO_ROOT / "tools" / "build-and-verify.sh")])
    path = _log(f"{tag}.build.log", proc)
    out = proc.stdout + proc.stderr
    ok = proc.returncode == 0 and "BUILD SUCCEEDED" in out
    return ok, (signal_lines(out) if not ok else "") + (f"\n({path})" if not ok else "")


def revert_all() -> None:
    git("checkout", "--", "configs", "src")
    git("clean", "-fd", "--", "src")


def candidates(results: Path) -> list[str]:
    rows = [json.loads(l) for l in results.read_text().splitlines() if l.strip()]
    shared = [r for r in rows
              if r["status"] == "matched" and r["grade"] == "clean"
              and r.get("dup_count", 1) > 1]
    shared.sort(key=lambda r: -r["dup_count"])
    out = []
    for r in shared:
        # Only bodies that actually landed; a still-stubbed one is not matched.
        if sh(["grep", "-rq", f"INCLUDE_ASM.*{r['func']})", "src"]).returncode != 0:
            out.append(r["func"])
    return out


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--func", help="promote just this body")
    ap.add_argument("--results", type=Path, help="results.jsonl to take bodies from")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--commit", action="store_true")
    ap.add_argument("--lock-wait", type=int, default=3600)
    ap.add_argument("--session", help="orchestrator session that already holds "
                    "the merge lock; with it, this run neither takes nor "
                    "releases the lock")
    args = ap.parse_args()

    os.chdir(REPO_ROOT)
    if args.func:
        names = [args.func]
    elif args.results:
        names = candidates(args.results)
    else:
        ap.error("need --func or --results")
    if args.limit:
        names = names[: args.limit]
    print(f"{len(names)} body/bodies to promote")

    if args.session:
        # The caller holds the lock for a longer span than this run; taking it
        # again here would be fine (the orchestrator reuses a same-session
        # acquire) but the release on the way out would drop the caller's hold.
        lock = contextlib.nullcontext()
    else:
        session = f"bulk-m2c-promote-{os.getpid()}"
        try:
            lock = L.held_merge_lock(session, args.lock_wait)
        except RuntimeError as exc:
            print(exc, file=sys.stderr)
            return 1

    done = 0
    with lock:
        ok, how = L.refresh_to_trunk()
        if not ok:
            print(f"could not fast-forward onto trunk: {how}", file=sys.stderr)
            return 1
        dirty = git("status", "--porcelain", "--", "src", "configs").stdout.strip()
        if dirty:
            print("tree is dirty while we hold the lock; refusing:\n" + dirty[:300],
                  file=sys.stderr)
            return 1

        for name in names:
            p = Promotion(name)
            if not p.run(args.dry_run):
                print(f"  skip {name}: {p.note}")
                revert_all()
                continue
            if args.dry_run:
                print(f"  [dry] {name}: {p.note} -> {p.symbol} "
                      f"({len(p.sharers)} overlays)")
                revert_all()
                continue

            plan, why = plan_rodata(p.cuts)
            ok, tail = (True, "") if not why else (False, why)
            if ok and plan:
                ok, tail = write_rodata_cuts(plan)
            if ok:
                ok, tail = run_split(name)
            if ok:
                ok, tail = reinject(p.pending)
            if ok:
                ok, tail = verify_full(name)
            if not ok:
                print(f"  REVERTED {name}:\n    " + tail.replace("\n", "\n    "))
                revert_all()
                continue
            git("add", "-A", "--", "src", "configs")
            if args.commit:
                git("commit", "-m", f"promote {name} into {p.symbol}")
            done += 1
            print(f"  promoted {name} -> {p.symbol} across {len(p.sharers)} overlays")

    print(f"\n{done} promoted")
    return 0


if __name__ == "__main__":
    sys.exit(main())
