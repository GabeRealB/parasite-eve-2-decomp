#!/usr/bin/env python3
"""Batch m2c seeding: compile every unmatched function's m2c output and keep the
ones that already score 100%.

Why this exists
---------------
Half of this project's matches (1046 of 2014 `matched <fn> <attempts>` commits)
landed on attempt 1, and attempt 1 is the m2c seed with the minimal edits needed
to compile it. On the easy tail -- two-instruction stubs, `return 0;` leaves,
single load/store accessors -- those edits are often *none*. Running that seed
through the compiler costs a second of CPU and no tokens, so it is worth doing
for all ~12k remaining functions before spending an agent on any of them.

What it does NOT do
-------------------
It does not write `src/`, touch `linkers/`, commit, or claim anything. It emits
a report plus staged candidate files; landing them is a separate, reviewed step
(`--report` shows what a landing pass would have to work with). Two reasons:

* Raw m2c output is exactly the code CLAUDE.md bans -- `*(s16*)((u8*)a + 0xC)`,
  `void*` parameters, `goto`. A 100% score is necessary but not sufficient, so
  every candidate is graded `clean` / `dirty` and only `clean` is proposed for
  an unattended landing.
* Landing thousands of functions at once is precisely when a shared struct
  change silently breaks an overlay the scoped build never looks at. That needs
  a batched, unscoped `./tools/build-and-verify.sh`, which is the landing
  script's job, not this one's.

Scoring is the same cc1 -> maspsx -> dist.py path `tools/claude-decomp-env/build.sh`
uses, so a 100% here means what a 100% means in a scratch env.

Usage
-----
    python3 tools/bulk_m2c.py --jobs 8 --limit 200
    python3 tools/bulk_m2c.py --overlay rooms --out .bulk_m2c
    python3 tools/bulk_m2c.py --report .bulk_m2c/results.jsonl
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import multiprocessing
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Iterator, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))

import decomp_overlay as ovl  # noqa: E402
import m2ctx  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parent.parent
ENV_DIR = REPO_ROOT / "tools" / "claude-decomp-env"
M2C = REPO_ROOT / "tools" / "m2c" / "m2c.py"
MASPSX = REPO_ROOT / "tools" / "maspsx" / "maspsx.py"
CC1 = REPO_ROOT / "tools" / "linux" / "gcc-2.8.1-psx" / "cc1"
GIVEUPS = REPO_ROOT / "tools" / "giveups"
DIFFICULT = REPO_ROOT / "tools" / "difficult_functions"
DIST = ENV_DIR / "dist.py"

CROSS = "mips-linux-gnu"
CPP = f"{CROSS}-cpp"
AS = f"{CROSS}-as"
NM = f"{CROSS}-nm"

# Verbatim from tools/claude-decomp-env/build.sh. If that file changes these
# must follow, or a "100%" here stops meaning a 100% there.
INCLUDE_FLAGS = [
    "-I", str(REPO_ROOT / "include"),
    "-I", str(REPO_ROOT / "build"),
    "-I", str(REPO_ROOT / "include" / "psyq"),
    "-I", str(REPO_ROOT / "include" / "decomp"),
]
CPP_FLAGS = [
    *INCLUDE_FLAGS,
    "-D_LANGUAGE_C", "-DUSE_INCLUDE_ASM", "-P", "-undef", "-Wall",
    "-lang-c", "-nostdinc", "-DVER_USA", "-DSKIP_ASM", "-DNON_MATCHING",
    # For m2c_macros.h, which only --valid-syntax seeds include. Adding the
    # directory defines nothing on its own, so codegen matches build.sh.
    "-I", str(REPO_ROOT / "tools" / "m2c"),
]
CC_FLAGS = [
    "-O2", "-mips1", "-mcpu=3000", "-w", "-funsigned-char", "-fpeephole",
    "-ffunction-cse", "-fpcc-struct-return", "-fcommon", "-fverbose-asm",
    "-msoft-float", "-mgas", "-fgnu-linker", "-quiet", "-gcoff", "-dp", "-G0",
]
AS_FLAGS = ["-EL", *INCLUDE_FLAGS, "-O2", "-march=r3000", "-mtune=r3000",
            "-no-pad-sections"]
MASPSX_FLAGS = ["--aspsx-version=2.77", "--run-assembler", "--expand-div"]

SCORE_RE = re.compile(r"Score:\s*([0-9.]+)%")
PENALTY_RE = re.compile(r"Penalties:\s*(.*)")
JTBL_RE = re.compile(r"jtbl_\w+")

# --- quality gate -----------------------------------------------------------
# Textual, deliberately. check_pointer_arithmetic.py is the project's real
# definition of the offence, but it preprocesses and parses with pycparser,
# which neither survives m2c's `?` pseudo-types nor is worth 12k invocations.
# The landing script runs the real checker on files that actually compile
# in-tree; this gate only has to be strict enough that `clean` is trustworthy.
DIRTY_PATTERNS = [
    (r"\bgoto\b", "goto"),
    (r"\*\s*\(\s*[A-Za-z_][A-Za-z0-9_ ]*\*+\s*\)\s*\(", "cast-deref pointer arithmetic"),
    (r"\(\s*(?:u8|s8|u16|s16|u32|s32|char|void)\s*\*+\s*\)\s*[A-Za-z_(]\w*\s*\+", "cast + offset"),
    (r"\?", "m2c unknown type"),
    (r"\bM2C_[A-Z]", "m2c placeholder macro"),
    (r"m2c_macros\.h", "needs m2c macro header"),
    (r"\bMULT_HI\b|\bMULTU_HI\b|\bDMULT_HI\b", "m2c arithmetic builtin"),
    (r"\bbitwise\b", "m2c bitwise cast"),
    (r"\bunksp\w*|\bsp[0-9A-Fa-f]{2,}\b", "raw stack slot"),
    (r"\bphi_\w+", "m2c phi temporary"),
    (r"\btemp_(?:hi|lo)\b", "m2c hi/lo temporary"),
    (r"\bD_[0-9A-Fa-f]{6,}\b", "unnamed data symbol"),
    (r"__asm__|asm\s*\(|asm\s+volatile", "inline asm"),
]
DIRTY_RES = [(re.compile(p), why) for p, why in DIRTY_PATTERNS]


@dataclass
class Result:
    func: str
    overlay: str
    unit: str
    asm_file: str
    seed: str                 # "m2c" | "giveup"
    status: str               # matched | mismatch | no_compile | not_a_function | no_seed | error
    score: Optional[float] = None
    penalties: str = ""
    grade: str = ""           # clean | dirty (only when status == matched)
    dirty_reasons: list = None
    instructions: int = 0
    context: str = ""           # headers | none -- which typing m2c had
    dup_group: Optional[str] = None
    dup_count: int = 1
    seconds: float = 0.0
    detail: str = ""

    def to_json(self) -> str:
        d = asdict(self)
        d["dirty_reasons"] = self.dirty_reasons or []
        return json.dumps(d)


def run(cmd, **kw) -> subprocess.CompletedProcess:
    return subprocess.run(
        cmd, cwd=kw.pop("cwd", REPO_ROOT), capture_output=True, text=True, **kw
    )


def instruction_count(asm_file: Path) -> int:
    n = 0
    for line in asm_file.read_text(errors="ignore").splitlines():
        if "/*" in line and "*/" in line and not line.lstrip().startswith("glabel"):
            n += 1
    return n


# --- candidate enumeration --------------------------------------------------

def claimed_functions() -> set[str]:
    """Names another vacuum session currently holds a lease on.

    Skipping them is what lets this run alongside `vacuum.sh --orchestrator`
    without two processes matching and landing the same function.
    """
    try:
        common = subprocess.run(
            ["git", "rev-parse", "--git-common-dir"], cwd=REPO_ROOT,
            capture_output=True, text=True, check=True,
        ).stdout.strip()
    except subprocess.CalledProcessError:
        return set()
    state = Path(common)
    if not state.is_absolute():
        state = REPO_ROOT / state
    state = state / "vacuum-orch.json"
    if not state.is_file():
        return set()
    try:
        return set(json.loads(state.read_text()).get("claims", {}))
    except (json.JSONDecodeError, OSError):
        return set()


def include_asm_index() -> dict[str, Path]:
    """function name -> the src/ file whose INCLUDE_ASM stands in for it.

    Built in one pass. `decomp_overlay.FunctionLoc.c_file` resolves this by
    re-reading all 1109 sources per lookup, which is fine for one function and
    quadratic for twelve thousand.
    """
    index: dict[str, Path] = {}
    for path in (REPO_ROOT / "src").rglob("*.c"):
        try:
            text = path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for _folder, name in ovl.INCLUDE_ASM_RE.findall(text):
            index.setdefault(name, path)
    return index


def _fallback_host(loc: ovl.FunctionLoc) -> Optional[Path]:
    """Unit source for a function no INCLUDE_ASM line names (already decompiled
    siblings only, or a unit whose stub uses a different spelling)."""
    candidate = loc.overlay.src_path / f"{loc.unit}.c"
    return candidate if candidate.is_file() else None


def iter_functions(
    overlay: Optional[str], version: Optional[str]
) -> Iterator[ovl.FunctionLoc]:
    overlays = ovl.discover_overlays(version)
    if overlay:
        overlays = [o for o in overlays if ovl.overlay_matches(o, overlay)]
        if not overlays:
            sys.exit(f"no overlay matches {overlay!r}")
    for o in overlays:
        root = o.asm_path / "nonmatchings"
        if not root.is_dir():
            continue
        for asm in sorted(root.rglob("*.s")):
            # splat emits data units into the same tree; they are not functions.
            if asm.name.startswith(("D_", "jtbl_")):
                continue
            yield ovl.FunctionLoc(
                name=asm.stem, overlay=o, asm_file=asm,
                kind="nonmatchings", unit=asm.parent.name,
            )


def dup_groups(names: set[str]) -> tuple[dict[str, tuple[str, int]], set[str]]:
    """(func -> (body hash, group size), already-solved funcs).

    Bodies repeat across overlays in a family, so matching every copy is the
    same work done many times over -- of the first 158 room functions matched,
    only 29 were distinct bodies. Processing one representative per group and
    fanning the result out through a shared `src/<family>/lib/` object is the
    documented route (MATCH_LOOP.md, "Shared bodies"); this supplies the
    grouping, it does not do the fan-out.

    The second return value is the functions whose body is *already matched* in
    another overlay. Those want promoting into the shared library, not seeding:
    running m2c on them would be pure waste.
    """
    import overlay_dup_index as dup

    try:
        data = dup.load(rebuild=False, families=None)
    except Exception as exc:  # noqa: BLE001
        print(f"warning: dup index unavailable ({exc}); every function treated "
              "as unique", file=sys.stderr)
        return {}, set()

    groups: dict[str, tuple[str, int]] = {}
    solved: set[str] = set()
    for key, members in dup.classes(data, "text").items():
        todo = [m["name"] for m in members if m["name"] in names]
        if not todo:
            continue
        if any(m.get("state") == "matched" for m in members):
            solved.update(todo)
        for name in todo:
            groups[name] = (key, len(todo))
    return groups, solved


# --- seed generation --------------------------------------------------------

# --- context ----------------------------------------------------------------
# The single biggest lever on output quality. An overlay source includes only
# `common.h`, which is types and macros with no game structs, so m2c has nothing
# to type an imported symbol against and falls back to raw offsets:
#
#     M2C_FIELD(Game_Session, s8 *, 5) = arg0;      /* common.h alone */
#     Game_Session->field_5 = arg0;                 /* + main/session.h */
#
# The first is unlandable pointer arithmetic; the second is the finished line.
# So the pass builds a context out of every header that declares an imported
# symbol, and the candidate includes that same list so it still compiles.
#
# psyq first: the game headers use its types without including it themselves
# (main/session.h names VECTOR, which only libgte.h defines), so a context
# missing them dies with "Syntax error when parsing C context" and every
# function in the family silently falls back to untyped output.
PSYQ_HEADERS = [
    "libgte.h", "libgpu.h", "libetc.h", "libapi.h", "libcd.h",
    "libspu.h", "libsnd.h", "kernel.h",
]


def candidate_headers(overlay: str) -> list[str]:
    """Header include lines to try for one overlay, in dependency order.

    Deliberately NOT every header under `include/<family>/`. Those are
    per-overlay views and they contradict each other on purpose:
    `include/actors/actor_101600.h` declares `extern Actor01600StateC08
    Gp_StateC08` while `include/gameplay/gameplay.h` declares the same symbol as
    `GpStateC08`. Including both is a hard cc1 conflict, and including all 34
    actor headers at once is 34 of them. A real source takes one -- see
    src/actors/lib/actor_101600_text.c -- so the context is per overlay too.

    The overlay's own header precedes gameplay's so that, when the two disagree,
    the greedy filter drops gameplay and keeps the overlay's own view.
    """
    heads = list(PSYQ_HEADERS)
    d = REPO_ROOT / "include" / "main"
    if d.is_dir():
        heads += sorted(f"main/{p.name}" for p in d.glob("*.h"))
    for family in ("actors", "weapons", "rooms", "aya", "kyle", "pe"):
        own = REPO_ROOT / "include" / family / f"{overlay}.h"
        if own.is_file():
            heads.append(f"{family}/{overlay}.h")
            break
    d = REPO_ROOT / "include" / "gameplay"
    if d.is_dir():
        heads += sorted(f"gameplay/{p.name}" for p in d.glob("*.h"))
    return heads


def _header_set_ok(headers: list[str], probe_asm: Path, workdir: Path) -> Optional[str]:
    """Preprocessed context text if this header set is usable, else None.

    Three gates, because each catches what the previous one misses:
      cpp  -- the headers preprocess at all;
      m2c  -- its parser is stricter than cpp, and a context it rejects aborts
              the whole run and silently downgrades every function using it;
      cc1  -- the candidate has to compile against these same headers, and two
              headers can each parse yet contradict each other ("previous
              declaration of `Gp_StateC08'").
    """
    tu = workdir / "ctx_tu.c"
    body = '#include "common.h"\n' + "".join(f'#include "{h}"\n' for h in headers)
    tu.write_text(body, encoding="utf-8")
    try:
        text = m2ctx.import_c_file(str(tu))
    except (SystemExit, Exception):  # noqa: B014 - m2ctx exits on cpp failure
        return None

    ctx = workdir / "probe_ctx.c"
    ctx.write_text(text, encoding="utf-8")
    proc = run([sys.executable, str(M2C), "--target", "mips-gcc-c",
                "--context", str(ctx), str(probe_asm)])
    if "parsing C context" in (proc.stdout + proc.stderr):
        return None

    probe_c = workdir / "probe_compile.c"
    probe_c.write_text(body + "\nvoid _bulk_m2c_probe(void) {}\n", encoding="utf-8")
    try:
        compile_candidate(probe_c, workdir)
    except RuntimeError:
        return None
    return text


def build_overlay_context(overlay: str, probe_asm: Path, workdir: Path) -> list[str]:
    """Greedily grow the largest header set that passes all three gates.

    One bad header costs that header's types rather than the whole context.
    """
    kept: list[str] = []
    for header in candidate_headers(overlay):
        if _header_set_ok(kept + [header], probe_asm, workdir) is not None:
            kept.append(header)
    return kept


_ctx_cache: dict[str, Optional[str]] = {}


def context_for(
    c_file: Optional[Path], headers: list[str], workdir: Path
) -> Optional[Path]:
    """Preprocessed m2c context: the family's headers plus the unit's own
    source, which carries the signatures of siblings already decompiled.

    m2ctx.import_c_file is called directly rather than shelling out to
    tools/m2ctx.py, because that script always writes <root>/ctx.c -- a single
    shared path that parallel workers would race on.
    """
    body = '#include "common.h"\n' + "".join(f'#include "{h}"\n' for h in headers)
    if c_file is not None and c_file.is_file():
        body += f'#include "{c_file}"\n'

    key = hashlib.sha1(body.encode()).hexdigest()
    if key in _ctx_cache:
        text = _ctx_cache[key]
    else:
        # Shared on disk, not just per process: a unit has many functions and
        # every worker would otherwise re-run gcc -E over the same 40 headers.
        # Preprocessing dominates the per-function cost without this.
        cached = CTX_DIR / f"{key}.c"
        text: Optional[str]
        if cached.is_file():
            text = cached.read_text(encoding="utf-8") or None
        else:
            tu = workdir / "unit_ctx.c"
            tu.write_text(body, encoding="utf-8")
            try:
                text = m2ctx.import_c_file(str(tu))
            except (SystemExit, Exception):  # noqa: B014
                text = None
            if text is None and c_file is not None:
                # The unit source is the likelier culprit; keep the headers.
                tu.write_text(body.rsplit("#include", 1)[0], encoding="utf-8")
                try:
                    text = m2ctx.import_c_file(str(tu))
                except (SystemExit, Exception):  # noqa: B014
                    text = None
            tmp = cached.with_suffix(f".{os.getpid()}.tmp")
            tmp.write_text(text or "", encoding="utf-8")
            os.replace(tmp, cached)  # atomic: workers race on the same key
        _ctx_cache[key] = text
    if text is None:
        return None
    path = workdir / "ctx.c"
    path.write_text(text, encoding="utf-8")
    return path


def jump_table_asm(loc: ovl.FunctionLoc) -> list[Path]:
    """Extra .s files m2c needs to resolve a `jr $v0` switch.

    splat leaves the table in the function's own .s only while the rodata block
    is unowned; otherwise it lives in the unit's .rodata.s and m2c bails with
    "the corresponding jump table is not provided", producing an empty seed that
    reads as a hard function. Mirrors tools/claude.
    """
    text = loc.asm_file.read_text(errors="ignore")
    if "jtbl_" not in text:
        return []
    scope = loc.overlay.asm_path
    extra: list[Path] = []
    for name in sorted(set(JTBL_RE.findall(text))):
        if re.search(rf"^dlabel {name}$", text, re.M):
            continue
        hit = run(["grep", "-rlx", f"dlabel {name}", str(scope), "--include=*.s"])
        for line in hit.stdout.splitlines():
            p = Path(line)
            if p not in extra:
                extra.append(p)
                break
    return extra


def run_m2c(
    loc: ovl.FunctionLoc, ctx: Optional[Path], jtbl: list[Path], valid_syntax: bool,
    headers: list[str],
) -> tuple[Optional[str], str]:
    cmd = [sys.executable, str(M2C), "--target", "mips-gcc-c"]
    if valid_syntax:
        cmd.append("--valid-syntax")
    if jtbl:
        cmd += ["-f", loc.name]
    if ctx:
        cmd += ["--context", str(ctx)]
    cmd += [str(loc.asm_file), *[str(p) for p in jtbl]]

    proc = run(cmd)
    if proc.returncode != 0 and ctx and "parsing C context" in (proc.stdout + proc.stderr):
        # A context m2c cannot parse aborts the whole run even though the
        # function's own asm is fine (the GTE inline-asm macros do this).
        # Untyped output beats no output.
        retry = [c for c in cmd if c != str(ctx) and c != "--context"]
        if "-f" not in retry:
            retry = retry[:-1] + ["-f", loc.name, str(loc.asm_file)]
        proc = run(retry)
    # m2c reports a failed decompilation as a C comment on stdout, and the exit
    # status varies, so this has to be checked before the returncode. The common
    # cause here is not an m2c bug: splat emits some `jlabel` blocks as their
    # own nonmatchings file, and a block whose branch targets live in a sibling
    # file is a fragment of a function, not a function. Nothing can match it
    # standalone, so say so rather than reporting it as an m2c failure.
    both = proc.stdout + proc.stderr
    if "Decompilation failure" in both:
        why = re.sub(r"\s+", " ", both.split("Decompilation failure", 1)[1])
        why = why.strip(" :*/")[:160]
        if "Cannot find branch target" in why or "outside of function" in why:
            return None, f"not a standalone function: {why}"
        return None, f"m2c: {why}"
    if proc.returncode != 0:
        err = (proc.stderr or proc.stdout).strip()
        return None, (err.splitlines()[-1][:200] if err else "m2c failed")
    body = proc.stdout.strip()
    if not body:
        return None, "m2c produced no output"
    # The candidate has to see the same declarations the context did, or a
    # `Game_Session->field_5` that m2c only wrote because session.h typed it
    # will not compile. m2c_macros.h typedefs M2C_UNK as s32, so it has to come
    # after common.h -- which is why these are include lines, not a cpp -include.
    prelude = '#include "common.h"\n'
    prelude += "".join(f'#include "{h}"\n' for h in headers)
    if valid_syntax:
        prelude += '#include "m2c_macros.h"\n'
    return prelude + "\n" + body + "\n", ""


def seed_sources(
    loc: ovl.FunctionLoc, c_file: Optional[Path], workdir: Path
) -> tuple[list[tuple[str, str]], str, str]:
    """Seeds to try, in order of how much we would like them to be the winner:
    [(source, kind)].

    An archived give-up short-circuits the list -- it is a human's or agent's
    best effort and is strictly better than anything m2c will produce.

    Otherwise plain m2c comes first because its output can be clean enough to
    land unattended, and `--valid-syntax` second because it only ever produces
    `dirty` output: it buys compilability with M2C_FIELD casts. Trying it is
    still worth it, since a 100% from it proves the shape and leaves an agent
    nothing to do but type the struct.
    """
    giveup = GIVEUPS / loc.name / "base.c"
    if giveup.is_file():
        return [(giveup.read_text(errors="ignore"), "giveup")], "", "giveup"

    headers = HEADERS.get(overlay_of(loc)) or HEADERS.get("__base__", [])
    ctx = context_for(c_file, headers, workdir)
    # The prelude must describe exactly what the context described. m2c's
    # `--globals used` re-declares any symbol it does not find in the context,
    # so a candidate that includes headers m2c could not see collides with them
    # ("previous declaration of ..."), and the seed fails to compile for a
    # reason that has nothing to do with whether it matches.
    if ctx is None:
        headers = []
    jtbl = jump_table_asm(loc)
    out: list[tuple[str, str]] = []
    detail = ""
    for valid_syntax, kind in ((False, "m2c"), (True, "m2c-valid-syntax")):
        source, why = run_m2c(loc, ctx, jtbl, valid_syntax, headers)
        if source is not None:
            out.append((source, kind))
        elif not detail:
            detail = why
    return out, detail, ("headers" if ctx else "none")


# --- compile and score ------------------------------------------------------

def build_target(loc: ovl.FunctionLoc, workdir: Path) -> Path:
    target_s = workdir / "target.s"
    target_o = workdir / "target.o"
    with target_s.open("w") as fh:
        for part in (ENV_DIR / "prelude.inc", REPO_ROOT / "include" / "macro.inc",
                     loc.asm_file):
            fh.write(part.read_text(errors="ignore"))
            fh.write("\n")
    proc = run([AS, "-EL", "-march=r3000", "-mtune=r3000",
                "-I", str(REPO_ROOT / "include"), "-o", str(target_o), str(target_s)])
    if proc.returncode != 0:
        raise RuntimeError(f"assembling target: {proc.stderr.strip()[:200]}")
    return target_o


def compile_candidate(c_path: Path, workdir: Path) -> Path:
    i_path = workdir / "cand.i"
    s_path = workdir / "cand.s"
    o_path = workdir / "cand.o"

    proc = run([CPP, "-P", *CPP_FLAGS, "-o", str(i_path), str(c_path)])
    if proc.returncode != 0:
        raise RuntimeError("cpp: " + proc.stderr.strip().splitlines()[-1][:200])

    proc = run([str(CC1), *CC_FLAGS, "-o", str(s_path), str(i_path)])
    if proc.returncode != 0:
        err = (proc.stderr or proc.stdout).strip().splitlines()
        raise RuntimeError("cc1: " + (err[-1][:200] if err else "failed"))

    proc = run([sys.executable, str(MASPSX), *MASPSX_FLAGS, *AS_FLAGS, "-G0",
                "-o", str(o_path), str(s_path)], stdin=subprocess.DEVNULL)
    if proc.returncode != 0:
        raise RuntimeError("maspsx: " + proc.stderr.strip().splitlines()[-1][:200])

    proc = run([NM, str(o_path)])
    if " T " not in proc.stdout:
        raise RuntimeError("object has no text symbols")
    return o_path


def score(target_o: Path, cand_o: Path) -> tuple[float, str]:
    proc = run([sys.executable, str(DIST), str(target_o), str(cand_o), "--stack-diffs"])
    m = SCORE_RE.search(proc.stdout)
    if not m:
        raise RuntimeError("dist.py: " + (proc.stderr or proc.stdout).strip()[:200])
    pen = PENALTY_RE.search(proc.stdout)
    return float(m.group(1)), (pen.group(1).strip() if pen else "")


def grade(source: str) -> tuple[str, list[str]]:
    reasons = [why for rx, why in DIRTY_RES if rx.search(source)]
    return ("clean" if not reasons else "dirty"), reasons


# --- worker -----------------------------------------------------------------

def process(job: tuple[ovl.FunctionLoc, Optional[Path]]) -> Result:
    loc, c_file = job
    name = loc.name
    started = time.time()

    res = Result(
        func=name, overlay=ovl.overlay_key(loc.overlay), unit=loc.unit,
        asm_file=str(loc.asm_file.relative_to(REPO_ROOT)), seed="m2c",
        status="error", instructions=instruction_count(loc.asm_file),
    )

    workdir = Path(tempfile.mkdtemp(prefix=f"bulkm2c-{name}-"))
    try:
        seeds, why, ctx_state = seed_sources(loc, c_file, workdir)
        res.context = ctx_state
        if not seeds:
            res.status = "not_a_function" if why.startswith("not a standalone") \
                else "no_seed"
            res.detail = why or "m2c produced nothing"
            return res

        target_o = build_target(loc, workdir)
        best: Optional[tuple[float, str, str, str, list[str]]] = None
        last_error = ""

        for source, kind in seeds:
            c_path = workdir / f"cand_{kind.replace('-', '_')}.c"
            c_path.write_text(source, encoding="utf-8")
            try:
                cand_o = compile_candidate(c_path, workdir)
            except RuntimeError as exc:
                last_error = str(exc)
                continue
            pct, penalties = score(target_o, cand_o)
            g, reasons = grade(source)
            # Rank on score, then prefer the seed we could actually land: a
            # clean 100% is a commit, a dirty 100% is a much cheaper agent task.
            rank = (pct, g == "clean")
            if best is None or rank > (best[0], best[3] == "clean"):
                best = (pct, penalties, source, g, reasons)
                res.seed = kind

        if best is None:
            res.status, res.detail = "no_compile", last_error
            return res

        pct, penalties, source, g, reasons = best
        res.score, res.penalties = pct, penalties
        if pct >= 100.0:
            res.status, res.grade, res.dirty_reasons = "matched", g, reasons
            bucket = g
        else:
            res.status = "mismatch"
            bucket = "near" if pct >= 95.0 else ""
        if bucket:
            staged = STAGE_DIR / bucket / f"{name}.c"
            staged.parent.mkdir(parents=True, exist_ok=True)
            staged.write_text(source, encoding="utf-8")
        return res
    except Exception as exc:  # noqa: BLE001 - one bad function must not stop the pass
        res.status, res.detail = "error", f"{type(exc).__name__}: {exc}"[:200]
        return res
    finally:
        res.seconds = round(time.time() - started, 2)
        shutil.rmtree(workdir, ignore_errors=True)


STAGE_DIR = Path()          # set in main(), read by workers after fork
CTX_DIR = Path()            # shared preprocessed-context cache
HEADERS: dict[str, list[str]] = {}   # family -> validated context header list


def family_of(loc: ovl.FunctionLoc) -> str:
    return ovl.overlay_key(loc.overlay).split("/")[-1]


def overlay_of(loc: ovl.FunctionLoc) -> str:
    """The overlay a function belongs to. In a family tree the asm path is
    asm/<ver>/<family>/nonmatchings/<overlay>/<unit>/<fn>.s, so the overlay is
    the directory under nonmatchings -- not the family, which is what
    overlay_key names for these."""
    parts = loc.asm_file.parts
    if "nonmatchings" in parts:
        at = parts.index("nonmatchings")
        if at + 1 < len(parts) - 1:
            return parts[at + 1]
    return family_of(loc)


def _init(stage: str, headers: dict, ctx_dir: str) -> None:
    global STAGE_DIR, HEADERS, CTX_DIR
    STAGE_DIR = Path(stage)
    HEADERS = headers
    CTX_DIR = Path(ctx_dir)


def resolve_contexts(
    overlays: set[str], locs: dict, out: Path, rebuild: bool
) -> dict[str, list[str]]:
    """Validated context header list per overlay, cached on disk.

    The greedy filter is run once, on the shared psyq+main+gameplay set, because
    that part is the same everywhere and costs one cpp+m2c+cc1 round per
    candidate header. Only the ~35 overlays that ship their own header need any
    further work, and they need at most two whole-set checks each.
    """
    cache = out / "contexts.json"
    known: dict[str, list[str]] = {}
    if cache.is_file() and not rebuild:
        try:
            known = json.loads(cache.read_text())
        except json.JSONDecodeError:
            known = {}

    todo = sorted(o for o in overlays if o not in known)
    if not todo:
        return known

    probe_for = {}
    for loc in locs.values():
        probe_for.setdefault(overlay_of(loc), loc.asm_file)

    workdir = Path(tempfile.mkdtemp(prefix="bulkm2c-ctx-"))
    try:
        base = known.get("__base__")
        if base is None:
            probe = probe_for[todo[0]]
            generic = [h for h in candidate_headers("__none__")]
            base = build_overlay_context("__none__", probe, workdir)
            known["__base__"] = base
            print(f"  context base: {len(base)}/{len(generic)} shared headers",
                  file=sys.stderr)

        no_gameplay = [h for h in base if not h.startswith("gameplay/")]
        for overlay in todo:
            probe = probe_for.get(overlay)
            own = next(
                (h for h in candidate_headers(overlay)
                 if h.endswith(f"/{overlay}.h")), None
            )
            if own is None or probe is None:
                known[overlay] = base
                continue
            # The overlay's own header is the more specific view, so when it
            # collides with gameplay's, gameplay is what gets dropped.
            for trial in (base + [own], no_gameplay + [own]):
                if _header_set_ok(trial, probe, workdir) is not None:
                    known[overlay] = trial
                    break
            else:
                known[overlay] = base
            print(f"  context[{overlay}]: {len(known[overlay])} headers"
                  f"{' (own)' if own in known[overlay] else ' (own header rejected)'}",
                  file=sys.stderr)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)

    cache.write_text(json.dumps(known, indent=2))
    return known


# --- reporting --------------------------------------------------------------

def report(path: Path) -> None:
    rows = [json.loads(l) for l in path.read_text().splitlines() if l.strip()]
    if not rows:
        print("no results")
        return
    by_status = collections.Counter(r["status"] for r in rows)
    total = len(rows)
    print(f"{total} functions processed\n")
    for status in ("matched", "mismatch", "no_compile", "not_a_function",
                   "no_seed", "error"):
        n = by_status.get(status, 0)
        if n:
            print(f"  {status:<12} {n:>6}  {n/total:6.1%}")

    matched = [r for r in rows if r["status"] == "matched"]
    if matched:
        grades = collections.Counter(r["grade"] for r in matched)
        print(f"\nmatched breakdown ({len(matched)}):")
        for g, n in grades.most_common():
            print(f"  {g:<12} {n:>6}  {n/len(matched):6.1%}")
        clean = [r for r in matched if r["grade"] == "clean"]
        fanout = sum(r.get("dup_count", 1) for r in clean)
        print(f"\n  clean candidates land {len(clean)} bodies "
              f"covering {fanout} functions after dup fan-out")
        reasons = collections.Counter(
            why for r in matched if r["grade"] == "dirty" for why in r["dirty_reasons"]
        )
        if reasons:
            print("\n  why the rest are dirty:")
            for why, n in reasons.most_common(8):
                print(f"    {why:<34} {n:>6}")

    typed = sum(1 for r in rows if r.get("context") == "headers")
    print(f"\n{typed}/{total} ({typed/total:.0%}) had a parsed context. "
          "Without one m2c cannot type an imported symbol and the seed is "
          "unlandable by construction.")

    near = [r for r in rows if r["status"] == "mismatch" and (r["score"] or 0) >= 95]
    print(f"\n{len(near)} near misses (>=95%) staged as permuter/vacuum seeds")
    fails = collections.Counter(
        r["detail"].split(":")[0] for r in rows if r["status"] == "no_compile"
    )
    if fails:
        print("\ncompile failures by stage:")
        for stage, n in fails.most_common(6):
            print(f"  {stage:<12} {n:>6}")
    secs = sum(r.get("seconds", 0) for r in rows)
    print(f"\n{secs/60:.1f} CPU-minutes, {secs/max(total,1):.2f}s per function")


# --- main -------------------------------------------------------------------

def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    # Not under build/: `build-and-verify.sh` wipes that tree, and it would
    # take the results and staged candidates of a multi-hour pass with it.
    ap.add_argument("--out", default=".bulk_m2c",
                    help="output directory (default: .bulk_m2c)")
    ap.add_argument("--overlay", help="restrict to one overlay or family")
    ap.add_argument("--version", dest="game_version", help="game version (default: all)")
    ap.add_argument("--limit", type=int, help="process the first N functions")
    ap.add_argument("--sample", type=int,
                    help="process N functions drawn at random instead of the "
                         "first N -- enumeration order is alphabetical by "
                         "overlay, which is not a representative slice")
    ap.add_argument("--seed", type=int, default=0, help="--sample RNG seed")
    ap.add_argument("--rebuild-context", action="store_true",
                    help="re-derive each family's context header list "
                         "instead of reusing the cached one")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("--max-instructions", type=int, default=0,
                    help="skip functions longer than this (0 = no limit)")
    ap.add_argument("--no-dedup", action="store_true",
                    help="process every copy instead of one per duplicate group")
    ap.add_argument("--include-claimed", action="store_true",
                    help="do not skip functions another vacuum session has claimed")
    ap.add_argument("--report", metavar="RESULTS.JSONL",
                    help="summarise an existing run and exit")
    args = ap.parse_args()

    # overlay_dup_index addresses asm/ and build/ relatively.
    os.chdir(REPO_ROOT)

    if args.report:
        report(Path(args.report))
        return 0

    out = Path(args.out)
    stage = out / "staged"
    ctx_dir = out / "ctx-cache"
    out.mkdir(parents=True, exist_ok=True)
    stage.mkdir(parents=True, exist_ok=True)
    ctx_dir.mkdir(parents=True, exist_ok=True)

    print("enumerating unmatched functions...", file=sys.stderr)
    locs = {l.name: l for l in iter_functions(args.overlay, args.game_version)}
    names = list(locs)
    print(f"  {len(names)} unmatched", file=sys.stderr)

    if not args.include_claimed:
        claimed = claimed_functions()
        if claimed:
            names = [n for n in names if n not in claimed]
            print(f"  -{len(claimed)} claimed by a running vacuum session",
                  file=sys.stderr)

    if args.max_instructions:
        names = [n for n in names
                 if instruction_count(locs[n].asm_file) <= args.max_instructions]
        print(f"  {len(names)} within --max-instructions", file=sys.stderr)

    groups: dict[str, tuple[str, int]] = {}
    if not args.no_dedup:
        print("  grouping duplicate bodies...", file=sys.stderr)
        groups, solved = dup_groups(set(names))
        if solved:
            names = [n for n in names if n not in solved]
            print(f"  -{len(solved)} whose body is already matched elsewhere "
                  "(promote, do not seed)", file=sys.stderr)
        if groups:
            seen: set[str] = set()
            reps: list[str] = []
            for n in names:
                key = groups.get(n, (None, 1))[0]
                if key is None:
                    reps.append(n)
                    continue
                if key in seen:
                    continue
                seen.add(key)
                reps.append(n)
            print(f"  {len(reps)} representatives for {len(names)} functions",
                  file=sys.stderr)
            names = reps

    if args.sample and args.sample < len(names):
        names = random.Random(args.seed).sample(names, args.sample)
    if args.limit:
        names = names[: args.limit]

    print("  indexing INCLUDE_ASM host files...", file=sys.stderr)
    hosts = include_asm_index()
    jobs = [(locs[n], hosts.get(n) or _fallback_host(locs[n])) for n in names]

    headers = resolve_contexts(
        {overlay_of(locs[n]) for n in names}, locs, out, args.rebuild_context
    )

    results_path = out / "results.jsonl"
    started = time.time()
    done = 0
    with results_path.open("w") as fh, multiprocessing.Pool(
        args.jobs, initializer=_init, initargs=(str(stage), headers, str(ctx_dir))
    ) as pool:
        for res in pool.imap_unordered(process, jobs, chunksize=4):
            if res.func in groups:
                res.dup_group, res.dup_count = groups[res.func]
            fh.write(res.to_json() + "\n")
            fh.flush()
            done += 1
            if done % 25 == 0 or done == len(names):
                rate = done / max(time.time() - started, 1e-9)
                eta = (len(names) - done) / rate if rate else 0
                print(f"  {done}/{len(names)}  {rate:.1f}/s  eta {eta/60:.1f}m",
                      file=sys.stderr)

    print(f"\nwrote {results_path}\n", file=sys.stderr)
    report(results_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
