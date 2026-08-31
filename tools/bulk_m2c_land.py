#!/usr/bin/env python3
"""Land verified bulk_m2c candidates into src/, one overlay at a time.

`tools/bulk_m2c.py` proves a seed matches; this puts it in the tree. The two are
separate on purpose -- scoring is cheap and repeatable, landing rewrites sources
and has to be verified against the whole project.

What it does
------------
For each candidate, replace the function's `INCLUDE_ASM` line in its host source
with the decompiled body, adding only the headers that body actually needs, then
verify. Verification is the point of the script, so it is done twice, exactly as
CLAUDE.md requires:

* `--only <overlay>` after each overlay's batch -- seconds, and it localises a
  failure to the overlay that caused it;
* a bare, unscoped `./tools/build-and-verify.sh` at the end -- the only result
  that means the project matches. A scoped pass says nothing about the overlays
  it skipped, which is exactly what a shared struct or a moved `.rodata` block
  breaks.

A candidate whose overlay fails to verify is bisected out and reverted, so one
bad body does not cost the whole batch. Nothing is committed unless --commit is
passed, and committing takes the vacuum orchestrator's merge lock first so this
cannot interleave with a running vacuum session's own landing.

Usage
-----
    python3 tools/bulk_m2c_land.py --results .bulk_m2c/results.jsonl --dry-run
    python3 tools/bulk_m2c_land.py --results .bulk_m2c/results.jsonl --limit 20
    python3 tools/bulk_m2c_land.py --results ... --grade clean,dirty --commit
"""

from __future__ import annotations

import argparse
import collections
import contextlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))

import bulk_m2c as B  # noqa: E402
import decomp_overlay as ovl  # noqa: E402

REPO_ROOT = B.REPO_ROOT
VERIFY = REPO_ROOT / "tools" / "build-and-verify.sh"
ORCH = REPO_ROOT / "tools" / "vacuum_orch.py"

C_KEYWORDS = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if", "int",
    "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile",
    "while", "u8", "s8", "u16", "s16", "u32", "s32", "u64", "s64", "f32", "f64",
}
IDENT_RE = re.compile(r"\b[A-Za-z_]\w*\b")


@dataclass
class Candidate:
    func: str
    overlay: str
    family: str
    grade: str
    body: str
    host: Path
    stub: str
    headers: list[str] = field(default_factory=list)
    landed: bool = False
    note: str = ""

    @property
    def scope(self) -> str:
        """Selector for `build-and-verify.sh --only`, which takes a family or an
        overlay basename. A shared body lives in `<family>/nonmatchings/lib/...`,
        so its derived overlay is the literal `lib`, which is neither -- and
        passing it fails the build for a reason unrelated to the code. Those
        verify against the whole family instead, which is also what a shared
        object actually affects."""
        return self.family if self.overlay == "lib" else self.overlay


def sh(cmd: list[str], cwd: Path = REPO_ROOT, timeout: int = 3600):
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True,
                          timeout=timeout)


# --- selecting and preparing -------------------------------------------------

def load_candidates(
    results: Path, staged: Path, grades: set[str], overlay: Optional[str],
    audit: Optional[list] = None,
) -> list[Candidate]:
    """Candidates to land, recording why every other row was left out.

    Selection depends on the tree, which vacuum sessions change underneath us,
    so two runs minutes apart legitimately differ. Without a per-row reason the
    only visible symptom is a count that moved, which is not enough to tell a
    session landing a function from a bug in here.
    """
    rows = [json.loads(l) for l in results.read_text().splitlines() if l.strip()]
    hosts = B.include_asm_index()
    out: list[Candidate] = []

    def note(func, why, **extra):
        if audit is not None:
            audit.append({"func": func, "decision": "reject", "reason": why, **extra})

    for r in rows:
        func = r["func"]
        if r.get("status") != "matched":
            note(func, f"status {r.get('status')}")
            continue
        if r.get("grade") not in grades:
            note(func, f"grade {r.get('grade')} not requested")
            continue
        src = staged / r["grade"] / f"{func}.c"
        if not src.is_file():
            note(func, "no staged file")
            continue
        loc = ovl.find_function(func)
        if loc is None:
            note(func, "no .s in asm/ (tree not split?)")
            continue
        name = B.overlay_of(loc)
        if overlay and name != overlay and B.family_of(loc) != overlay:
            note(func, f"overlay {name} filtered out")
            continue
        host = hosts.get(func) or B._fallback_host(loc)
        if host is None or not host.is_file():
            note(func, "already landed (no INCLUDE_ASM anywhere)")
            continue
        body = "\n".join(
            l for l in src.read_text().splitlines() if not l.startswith("#include")
        ).strip()
        stub = find_stub(host, func)
        if stub is None:
            note(func, "stub gone from its host", host=str(host))
            continue
        cand = Candidate(func, name, B.family_of(loc), r["grade"], body, host, stub)
        if r["grade"] == "dirty":
            why = rewrite_task_fields(cand, STRICT_TYPES)
            if why:
                note(func, why)
                continue
        if audit is not None:
            audit.append({"func": func, "decision": "accept", "reason": "",
                          "overlay": name})
        out.append(cand)
    return out


def find_stub(host: Path, func: str) -> Optional[str]:
    for line in host.read_text().splitlines():
        m = ovl.INCLUDE_ASM_RE.search(line)
        if m and m.group(2) == func:
            return line
    return None


# --- Task-typed dirty seeds --------------------------------------------------
# A dirty seed is a proven match whose accesses m2c could not type, so it writes
# M2C_FIELD(arg0, s32 *, 0x30) where the source said arg0->state. For most of
# them the type is not undescribed at all -- it is `Task`, which
# include/main/task.h already defines with every hot offset named (0x1C idMap,
# 0x30 state, 0x2C extra). Those need no new type and no judgement: retype the
# parameter and substitute the field, and let the build adjudicate exactly as it
# does for a clean seed.
#
# The limit of that adjudication is worth stating: the checksum proves the
# codegen, not that the field NAME is the right reading of the offset. A
# same-sized field with the wrong name would land silently, which is why this
# only ever uses names the header already commits to.
TASK_FIELD_RE = re.compile(r"/\*\s*(0x[0-9A-Fa-f]+)\s*\*/\s*([\w \*]+?)\s*(\w+)\s*;")
M2C_ARG0_RE = re.compile(
    r"M2C_FIELD\(\s*arg0\s*,\s*([^,]+?)\s*,\s*(0x[0-9A-Fa-f]+|\d+)\s*\)")
ANY_M2C_RE = re.compile(r"\bM2C_[A-Z]")
TASK_SIZE = 0x48


def task_fields() -> dict[int, tuple[str, str]]:
    """offset -> (field name, declared type), read from the Task definition."""
    header = REPO_ROOT / "include" / "main" / "task.h"
    if not header.is_file():
        return {}
    text = header.read_text(errors="ignore")
    start = text.find("typedef struct _Task")
    end = text.find("} Task;", start)
    if start < 0 or end < 0:
        return {}
    return {int(off, 16): (name, ctype.strip())
            for off, ctype, name in TASK_FIELD_RE.findall(text[start:end])}


TASK_FIELDS = task_fields()


def types_compatible(cast: str, field: str) -> bool:
    """Is m2c's cast type close enough to the field's declared type to land?

    Pointer against pointer is the case that matters. m2c writes
    `M2C_FIELD(arg0, void **, 0x1C)` meaning "load a pointer from 0x1C" and the
    field is `TaskIdMap*`; assigning one to the other is ordinary C and the same
    four-byte load, so demanding the spellings match rejects 185 sound
    candidates for a difference that does not exist.

    What is still refused is a width change - `s16` where the field is `s32` -
    because that does alter the load and would show up as a failed checksum
    rather than as tidy code.
    """
    strip = lambda s: re.sub(r"\s+", "", s)
    c, f = strip(cast), strip(field)
    if c == f:
        return True
    if "*" in c and "*" in f:
        # A pointer read through one more level of indirection than the field
        # declares is still a pointer read: void** vs TaskIdMap*.
        return True
    return False


def rewrite_task_fields(cand: Candidate, strict: bool = False) -> str:
    """Turn a Task-shaped dirty body into typed field access.

    Returns "" on success, else the reason it declined -- the reason is the
    point: a run that selects a different number of candidates than the last one
    has to be explainable without re-deriving it by hand.
    """
    body = cand.body
    if not TASK_FIELDS:
        return "task.h not parsed"
    if "M2C_FIELD" not in body:
        return "no M2C_FIELD (goto / stack slot / unnamed data)"
    hits = M2C_ARG0_RE.findall(body)
    if not hits:
        return "no access through arg0"
    offs = [int(o, 0) for _ty, o in hits]
    outside = [o for o in offs if o >= TASK_SIZE or o not in TASK_FIELDS]
    if outside:
        return f"offset 0x{outside[0]:X} is not a named Task field"
    if strict:
        bad = [(ct, o) for ct, o in hits
               if not types_compatible(ct, TASK_FIELDS[int(o, 0)][1])]
        if bad:
            ct, o = bad[0]
            return (f"cast {ct.strip()} != field type "
                    f"{TASK_FIELDS[int(o, 0)][1]} at 0x{int(o, 0):X}")

    new = M2C_ARG0_RE.sub(lambda m: f"arg0->{TASK_FIELDS[int(m.group(2), 0)][0]}", body)
    if ANY_M2C_RE.search(new):
        return "M2C_ macro left after rewrite (access not through arg0)"
    retyped = re.sub(r"\bvoid\s*\*\s*arg0\b", "Task *arg0", new)
    if "Task *arg0" not in retyped:
        return "first parameter is not void *arg0"
    cand.body = retyped
    return ""


COMMENT_RE = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)
UNDECLARED_RE = re.compile(
    r"`([A-Za-z_]\w*)' undeclared|parse error before `([A-Za-z_]\w*)'"
)


def declares(header: str, symbol: str) -> bool:
    """Does this header declare the symbol, in code rather than in prose?

    The headers in this project are heavily commented and name dozens of
    unrelated symbols in those comments, so a plain text search matches almost
    every header for almost every body. Stripping comments first is what makes
    the answer mean anything.
    """
    path = REPO_ROOT / "include" / header
    if not path.is_file():
        return False
    code = COMMENT_RE.sub(" ", path.read_text(errors="ignore"))
    return re.search(rf"\b{re.escape(symbol)}\b", code) is not None


def host_compiles(host: Path, workdir: Path) -> tuple[bool, str]:
    try:
        B.compile_candidate(host, workdir)
        return True, ""
    except RuntimeError as exc:
        return False, str(exc)


DEFINE_PATTERNS = [
    r"\}}\s*{sym}\s*;",                      # } Task;
    r"typedef[^;]*\b{sym}\s*;",              # typedef struct _Task Task;
    r"\b(?:struct|union|enum)\s+{sym}\b",
    r"\b{sym}\s*\(",                         # function declaration
    r"\bextern\b[^;]*\b{sym}\b",             # extern object
]


def defines(header: str, symbol: str) -> bool:
    """Does this header *define or declare* the symbol, rather than merely
    mention it?

    `declares` (a bare word search) matches 20 headers for a body that names
    `Task`, because a type is named by every header that takes one as a
    parameter. Matching the shapes a definition actually takes narrows that to
    the header that owns it -- main/task.h for Task, TaskDesc, Task_Kill and
    Task_SpawnFromTable alike.
    """
    path = REPO_ROOT / "include" / header
    if not path.is_file():
        return False
    code = COMMENT_RE.sub(" ", path.read_text(errors="ignore"))
    esc = re.escape(symbol)
    return any(re.search(p.format(sym=esc), code) for p in DEFINE_PATTERNS)


def body_symbols(body: str) -> set[str]:
    """Names in the body that some header might have to supply. The function's
    own symbols and its overlay's data are never in a header."""
    return {
        i for i in IDENT_RE.findall(body)
        if i not in C_KEYWORDS and not i.startswith(("D_", "func_", "arg", "temp_",
                                                     "var_", "sp", "phi_"))
    }


EXTERN_RE = re.compile(r"^\s*extern\s+[^;=]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;\s*$")


def split_externs(body: str) -> tuple[str, list[str]]:
    """Separate a body's file-scope `extern` declarations from its code."""
    code, externs = [], []
    for line in body.splitlines():
        (externs if EXTERN_RE.match(line) else code).append(line)
    return "\n".join(code).strip(), externs


def declared_in(text: str, symbol: str) -> bool:
    return re.search(r"^\s*extern\b[^;]*\b%s\b[^;]*;" % re.escape(symbol),
                     text, re.M) is not None


def insert_declarations(text: str, decls: list[str]) -> str:
    """Put declarations directly after the file's include block.

    Not next to the function that needs them. m2c emits each body's `extern`
    lines inline, and two functions in one file routinely disagree about a
    global's type -- the same symbol is `Task *` where a task is assigned to it
    and `s32` where it is only zeroed. Keeping one declaration ahead of every
    use is what makes both functions landable: leaving them inline means
    whichever body sits later in the file gets "used prior to declaration".
    """
    if not decls:
        return text
    lines = text.splitlines()
    last = max((i for i, l in enumerate(lines)
                if l.startswith("#include")), default=-1)
    block = list(decls)
    if last + 1 < len(lines) and lines[last + 1].strip():
        block.append("")
    lines[last + 1:last + 1] = block
    return "\n".join(lines) + ("\n" if text.endswith("\n") else "")


def _compiles_with(cand: Candidate, headers: list[str], workdir: Path,
                   original: str) -> bool:
    """Would the host compile with this include set? Checked on a copy in the
    scratch dir, so `src/` is untouched until the candidate is actually landed."""
    cand.headers = list(headers)
    try:
        text = edited_text(cand, original)
    except RuntimeError as exc:
        cand.note = str(exc)[:160]
        return False
    probe = workdir / f"probe_{cand.func}.c"
    probe.write_text(text)
    ok, err = host_compiles(probe, workdir)
    if not ok:
        cand.note = err[:160]
    return ok


def resolve_includes(cand: Candidate, available: list[str], workdir: Path) -> bool:
    """Find the smallest include list that makes the edited host source compile.

    Ordered cheapest-first: most bodies need nothing, most of the rest need the
    one header that defines the types they name, and only the stragglers pay for
    the broad search plus minimisation.
    """
    original = cand.host.read_text()
    if _compiles_with(cand, [], workdir, original):
        return True

    symbols = body_symbols(cand.body)
    owning: list[str] = []
    for sym in sorted(symbols):
        header = next((h for h in available if defines(h, sym)), None)
        if header and header not in owning:
            owning.append(header)
    if owning and _compiles_with(cand, owning, workdir, original):
        return minimise(cand, owning, workdir, original)

    broad = [h for h in available if any(declares(h, s) for s in symbols)]
    if broad and _compiles_with(cand, broad, workdir, original):
        return minimise(cand, broad, workdir, original)

    cand.headers = []
    return False


def minimise(cand: Candidate, headers: list[str], workdir: Path,
             original: str) -> bool:
    """Drop every include the file compiles without, so what lands is the list a
    person would have written rather than everything m2c was typed against."""
    keep = list(headers)
    for header in headers:
        trial = [h for h in keep if h != header]
        if _compiles_with(cand, trial, workdir, original):
            keep = trial
    cand.headers = keep
    cand.note = ""
    return True


def edited_text(cand: Candidate, text: str) -> str:
    """The host source with this candidate's body in place of its INCLUDE_ASM.

    Pure: it takes the file's current contents and returns the new contents. The
    include-resolution loop compiles the result many times per candidate, and
    doing that by writing the real file and restoring it would leave `src/`
    briefly half-edited -- which a vacuum session's `git add -A -- src` could
    stage out from under us.
    """
    if cand.stub not in text:
        raise RuntimeError(f"stub for {cand.func} no longer present in {cand.host}")

    code, externs = split_externs(cand.body)
    text = text.replace(cand.stub, code)

    missing = [h for h in cand.headers if f'#include "{h}"' not in text]
    if missing:
        anchor = '#include "common.h"'
        adds = "".join(f'\n#include "{h}"' for h in missing)
        if anchor in text:
            text = text.replace(anchor, anchor + adds, 1)
        else:
            text = adds.lstrip("\n") + "\n" + text

    # A declaration the file already carries wins: it came from a sibling
    # function that saw more of the symbol's real type than this one did.
    return insert_declarations(
        text, [e for e in externs
               if not declared_in(text, EXTERN_RE.match(e).group(1))])


def apply_edit(cand: Candidate) -> None:
    cand.host.write_text(edited_text(cand, cand.host.read_text()))
    cand.landed = True


def revert(paths: set[Path]) -> None:
    if paths:
        sh(["git", "checkout", "--"] + [str(p) for p in sorted(paths)])


# --- verification ------------------------------------------------------------

def verify(scope: Optional[str]) -> tuple[bool, str]:
    cmd = [str(VERIFY)] + (["--only", scope] if scope else [])
    proc = sh(cmd)
    ok_marker = "SCOPED BUILD SUCCEEDED" if scope else "BUILD SUCCEEDED"
    out = proc.stdout + proc.stderr
    ok = proc.returncode == 0 and ok_marker in out
    tail = "\n".join(out.strip().splitlines()[-4:])
    return ok, tail


def land_overlay(cands: list[Candidate], dry_run: bool,
                 do_commit: bool = False) -> list[Candidate]:
    """Land one overlay's candidates, bisecting out any that break it."""
    overlay = cands[0].overlay
    scope = cands[0].scope
    workdir = Path(tempfile.mkdtemp(prefix=f"bulkland-{overlay}-"))
    try:
        resolved = []
        for c in cands:
            available = HEADERS.get(c.overlay) or HEADERS.get("__base__", [])
            if resolve_includes(c, available, workdir):
                resolved.append(c)
            else:
                print(f"      skipped {c.func}: {c.note}", file=sys.stderr)
        cands = resolved
    finally:
        shutil.rmtree(workdir, ignore_errors=True)
    if not cands:
        return []
    if dry_run:
        return []

    # One function at a time, verified and committed individually. Landing a
    # whole overlay and committing afterwards produced one commit per *file*:
    # `git add <host>` stages every change in it, so the first commit swallowed
    # its siblings. fit_difficulty_model.py mines `^matched (\S+) (\d+)$` off the
    # subject and joins it to that commit's diff, so bundled functions are
    # invisible to it and one name is credited with several functions' code.
    kept: list[Candidate] = []
    for c in cands:
        apply_edit(c)
        ok, tail = verify(scope)
        if not ok:
            revert({c.host})
            c.landed = False
            c.note = tail.splitlines()[-1][:160] if tail else "scoped build failed"
            # Only re-apply siblings that are not committed yet. Once
            # commit_one has run, the body is in HEAD, so `git checkout --`
            # restores it rather than losing it -- and re-applying then fails,
            # because the stub it looks for has already been replaced.
            if not do_commit:
                for k in kept:
                    apply_edit(k)
            continue
        if not do_commit or commit_one(c):
            kept.append(c)
        else:
            revert({c.host})
            c.landed = False
            c.note = "commit failed"
    return kept


# --- committing --------------------------------------------------------------

@contextlib.contextmanager
def held_merge_lock(session: str, wait: int):
    """Hold the orchestrator merge lock across the whole verify-and-commit run.

    Taking it only around `git commit` leaves a window in which a vacuum session
    can land on trunk between the build and the commit, so the tree that was
    verified and the tree the commit sits on are not the same.
    """
    if not merge_lock(session, True, wait):
        raise RuntimeError("could not take the orchestrator merge lock")
    try:
        yield
    finally:
        merge_lock(session, False, wait)


def refresh_to_trunk() -> tuple[bool, str]:
    """Fast-forward onto trunk before landing, so candidates a session matched
    meanwhile have lost their INCLUDE_ASM line and get dropped."""
    before = sh(["git", "rev-parse", "--short", "HEAD"]).stdout.strip()
    proc = sh(["git", "merge", "--ff-only", "main"])
    if proc.returncode != 0:
        return False, proc.stderr.strip()[:200]
    after = sh(["git", "rev-parse", "--short", "HEAD"]).stdout.strip()
    return True, ("already current" if before == after else f"{before} -> {after}")


def merge_lock(session: str, acquire: bool, wait: int = 3600) -> bool:
    cmd = [sys.executable, str(ORCH),
           "merge-acquire" if acquire else "merge-release", "--session", session]
    if acquire:
        cmd += ["--pid", str(os.getpid()), "--wait", str(wait)]
    return sh(cmd).returncode == 0


def commit_one(cand: Candidate) -> bool:
    """Commit exactly this function, in the vacuum's message shape so
    fit_difficulty_model.py keeps mining it as an observation."""
    if sh(["git", "add", str(cand.host)]).returncode != 0:
        return False
    return sh(["git", "commit", "-m", f"matched {cand.func} 1"]).returncode == 0


# --- main --------------------------------------------------------------------

HEADERS: dict[str, list[str]] = {}
STRICT_TYPES = False


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--results", required=True, type=Path)
    ap.add_argument("--staged", type=Path,
                    help="staged dir (default: <results dir>/staged)")
    ap.add_argument("--contexts", type=Path,
                    help="contexts.json (default: <results dir>/contexts.json)")
    ap.add_argument("--grade", default="clean",
                    help="comma-separated grades to land (default: clean)")
    ap.add_argument("--overlay", help="restrict to one overlay or family")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--dry-run", action="store_true",
                    help="report what would land without editing anything")
    ap.add_argument("--audit", type=Path,
                    help="where to write the per-row selection record "
                         "(default: <results dir>/selection.jsonl)")
    ap.add_argument("--strict-types", action="store_true",
                    help="only rewrite a dirty seed when m2c's cast type "
                         "matches the field's declared type")
    ap.add_argument("--lock-wait", type=int, default=3600,
                    help="seconds to wait for the orchestrator merge lock")
    ap.add_argument("--commit", action="store_true",
                    help="commit each landed function (takes the merge lock)")
    args = ap.parse_args()

    os.chdir(REPO_ROOT)
    global STRICT_TYPES
    STRICT_TYPES = args.strict_types
    staged = args.staged or args.results.parent / "staged"
    ctx = args.contexts or args.results.parent / "contexts.json"
    global HEADERS
    if ctx.is_file():
        HEADERS = json.loads(ctx.read_text())

    grades = {g.strip() for g in args.grade.split(",") if g.strip()}

    session = f"bulk-m2c-land-{os.getpid()}"
    try:
        lock = held_merge_lock(session, args.lock_wait)
    except RuntimeError as exc:
        print(exc, file=sys.stderr)
        return 1

    with lock:
        ok, how = refresh_to_trunk()
        if not ok:
            print(f"could not fast-forward onto trunk: {how}", file=sys.stderr)
            return 1
        print(f"trunk: {how}")

        # Only meaningful inside the lock: outside it a vacuum session is
        # entitled to have an edit in flight, so checking beforehand just races
        # that session and refuses to start for no reason.
        dirty = sh(["git", "status", "--porcelain", "--", "src"]).stdout.strip()
        if dirty:
            print("src/ is dirty while we hold the merge lock; refusing to "
                  "touch it:\n" + dirty[:400], file=sys.stderr)
            return 1

        audit: list = []
        cands = load_candidates(args.results, staged, grades, args.overlay, audit)

        audit_path = args.audit or args.results.parent / "selection.jsonl"
        audit_path.write_text("".join(json.dumps(a) + "\n" for a in audit))
        rejected = collections.Counter(a["reason"] for a in audit
                                       if a["decision"] == "reject")
        print(f"selection written to {audit_path}")
        print(f"  accepted {sum(1 for a in audit if a['decision'] == 'accept')} "
              f"of {len(audit)} rows")
        for why, n in rejected.most_common(10):
            print(f"    {n:>5}  {why}")

        claimed = B.claimed_functions()
        if claimed:
            before = len(cands)
            cands = [c for c in cands if c.func not in claimed]
            if before != len(cands):
                print(f"skipping {before - len(cands)} claimed by a vacuum session",
                      file=sys.stderr)
        if args.limit:
            cands = cands[: args.limit]
        if not cands:
            print("nothing to land")
            return 0

        by_overlay: dict[str, list[Candidate]] = {}
        for c in cands:
            by_overlay.setdefault(c.overlay, []).append(c)
        print(f"{len(cands)} candidates across {len(by_overlay)} overlays")

        # Dry run reports from inside the lock too. An audit taken while a
        # session is mid-split describes a tree that no longer exists, which is
        # exactly the failure this ordering is meant to remove.
        if args.dry_run:
            for overlay, group in sorted(by_overlay.items()):
                land_overlay(group, True)
                for c in group:
                    heads = ", ".join(c.headers) or "no new includes"
                    mark = " " if not c.note else "!"
                    print(f"  [dry]{mark}{overlay:<26} {c.func}  ({heads})")
            print("\ndry run: nothing written")
            return 0

        # Re-read after the refresh: a session may have matched one of these
        # while we queued for the lock, in which case its INCLUDE_ASM line is
        # gone and the candidate is no longer ours to land.
        fresh = {c.func for c in load_candidates(
            args.results, staged, grades, args.overlay)}
        stale = [c.func for c in cands if c.func not in fresh]
        if stale:
            print(f"dropping {len(stale)} matched on trunk while we waited: "
                  + ", ".join(stale[:4]) + (" ..." if len(stale) > 4 else ""))
            by_overlay = {
                k: [c for c in v if c.func in fresh] for k, v in by_overlay.items()
            }
            by_overlay = {k: v for k, v in by_overlay.items() if v}

        landed: list[Candidate] = []
        for overlay, group in sorted(by_overlay.items()):
            kept = land_overlay(group, False, args.commit)
            landed += kept
            print(f"  {overlay:<28} {len(kept)}/{len(group)} verified")
            for c in group:
                if not c.landed:
                    print(f"      reverted {c.func}: {c.note}")

        if not landed:
            print("\nnothing verified; tree unchanged")
            return 1

        print(f"\n{len(landed)} landed. Running the unscoped build "
              "(the only one that counts)...")
        ok, tail = verify(None)
        if not ok:
            print("\n❌ UNSCOPED BUILD FAILED - reverting every landed file.\n"
                  + tail)
            revert({c.host for c in landed})
            return 1
        print("✅ BUILD SUCCEEDED")

        if args.commit:
            print(f"committed {len(landed)} function(s), one commit each")
        else:
            print("not committing (pass --commit); changes are in the working "
                  "tree, and the merge lock is about to be released -- commit "
                  "or revert them before a vacuum session lands anything else")

    shared = [c.func for c in landed if is_shared(c.func)]
    if shared:
        print(f"\n{len(shared)} of these bodies also exist in other overlays. "
              "Matching them once is the documented route -- run:\n  "
              "python3 tools/overlay_dup_index.py find <func>")
    return 0


def is_shared(func: str) -> bool:
    proc = sh([sys.executable, str(REPO_ROOT / "tools" / "overlay_dup_index.py"),
               "find", func])
    return proc.returncode == 0 and len(proc.stdout.strip().splitlines()) > 1


if __name__ == "__main__":
    sys.exit(main())
