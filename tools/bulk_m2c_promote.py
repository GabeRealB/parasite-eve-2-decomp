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
from typing import Optional

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


# --- one promotion -----------------------------------------------------------

class Promotion:
    def __init__(self, func: str):
        self.func = func
        self.note = ""
        self.symbol = ""
        self.unit_path: Optional[Path] = None
        self.sharers: list[str] = []

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

        # Drop it from its own overlay, leaving the file otherwise intact.
        origin.write_text((text[:span[0]] + text[span[1]:]).replace("\n\n\n\n", "\n\n"))

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
            if stub:
                host.write_text(ht.replace(stub + "\n", "").replace(stub, ""))
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


def verify_full(tag: str = "run") -> tuple[bool, str]:
    py = sys.executable
    venv = REPO_ROOT / "venv" / "bin" / "python3"
    if venv.is_file():
        py = str(venv)
    proc = sh([py, str(NINJA)])
    path = _log(f"{tag}.ninja.log", proc)
    if proc.returncode != 0:
        return False, f"ninja_config failed ({path}):\n" + signal_lines(
            proc.stdout + "\n" + proc.stderr)
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
