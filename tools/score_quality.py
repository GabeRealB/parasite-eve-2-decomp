#!/usr/bin/env python3
"""Rate matched C by how much matching scaffolding it still carries.

`score_functions.py` ranks *unmatched* assembly by how hard it will be to
match. This is its counterpart for code that already matches: it ranks
functions by how much of the source exists only to steer GCC 2.8.1, so a
cleanup pass can start where the payoff is.

Everything here comes from a real parse (libclang), not from pattern matching
on text. C is not a regular language, and the constructs that matter are
exactly the ones a regex gets wrong:

  * A pin is an `ASM_LABEL_ATTR` on a declaration, so its register name is read
    off the AST rather than out of a string a comment or literal could fake.
  * `volatile` is read from the *type* (`is_volatile_qualified`, including
    through a pointer), so `volatile CdStreamState*` is recognised as a
    qualified pointee rather than a word on a line.
  * Every inline-asm statement is attributed to the macro it expanded from,
    and that macro to the header that defined it. Both are parser facts, so
    the classifier cannot drift from the headers the way a hardcoded name list
    does - it even picks up locally defined macros such as `gte_nclip_real`
    and `and_mask` that no list would have known about.

The question being scored is **how far this is from C a 1999 PS1 programmer
could plausibly have written**, not how far it is from modern style. That cuts
both ways, and the second half matters as much as the first:

  penalised    things no human wrote - compiler steering, identifiers named
               after machine state (`temp_v0`, `sp18`), and control flow shaped
               like the assembly rather than the intent (a `switch` whose every
               arm is `goto L_caseN`).
  not penalised  things that are simply how C was written then - `goto` for
               cleanup and early exit, magic numbers, long functions, global
               state, no `const`. Flagging these would push the code *away*
               from era plausibility, which is why general-purpose linters are
               a poor fit here (see tools/score_quality_checks.yaml).

Why the asm attribution matters: inline asm here comes from three populations
and only some of it is debt.

  legitimate  `include/psyq/**` - the `gte_*` macros encode real GTE
              coprocessor instructions and are what the original source used.
              Also local `gte_<op>_real()` redefinitions, which exist because
              the stock header emits the wrong opcode.
  scaffolding `include/decomp/**` - `TOUCH_REG`, `USE_REG`, `SCHED_BARRIER`,
              `COMPILER_BARRIER` and friends exist only to steer the register
              allocator and scheduler.
  raw         inline asm from no macro at all: hand-written `lw`/`mfc2`/`%hi`
              blocks sitting in the C. The most severe of the three.

Scored on a naive `asm|volatile` grep, `Gp_DrawActorTmdFlagged` comes out the
worst function in the tree - and nearly all of that is ordinary graphics code.

Requires the libclang Python bindings, which are deliberately not in
requirements.txt: that file is the build's dependency list, and the build does
not need clang. This is a maintenance tool, so it asks for what it needs at
run time instead of making every CI install pay for it. Either package works -

    venv/bin/pip install clang       348 KB, uses the system libclang
                                     (apt install libclang-dev, or any
                                     /usr/lib/llvm-*/lib/libclang.so)
    venv/bin/pip install libclang     62 MB, bundles its own copy

The first is preferable wherever LLVM is already installed. The tool probes for
a system library automatically, so `clang` alone is usually enough.

Examples:
  python3 tools/score_quality.py                       # worst 40, whole tree
  python3 tools/score_quality.py --family gameplay
  python3 tools/score_quality.py --summary
  python3 tools/score_quality.py --ranked | head -20   # names, worst first
  python3 tools/score_quality.py --pin-sites --family rooms
"""

import argparse
import collections
import json
import os
import re
import sys
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

ROOT = Path(__file__).resolve().parent.parent

try:
    import clang.cindex as ci
except ImportError:  # pragma: no cover
    sys.exit(
        "score_quality: needs the libclang Python bindings.\n"
        "  venv/bin/pip install clang      # 40 KB, uses the system libclang\n"
        "  venv/bin/pip install libclang   # 62 MB, bundles its own copy"
    )


def _ensure_library() -> None:
    """Point the bindings at a libclang if they do not bundle one.

    The `clang` distribution is bindings only (40 KB) and needs a system
    library; `libclang` bundles a 61 MB copy. Supporting both keeps this tool
    from forcing the heavy one on a machine that already has LLVM installed -
    it is a maintenance tool, not something the build needs.
    """
    import ctypes
    from glob import glob

    try:
        ci.Index.create()
        return
    except Exception:
        pass
    for so in sorted(glob("/usr/lib/llvm-*/lib/libclang.so*"), reverse=True) + sorted(
        glob("/usr/lib/*/libclang*.so*"), reverse=True
    ):
        try:
            ctypes.CDLL(so)
        except OSError:
            continue
        try:
            ci.Config.set_library_file(so)
            ci.Index.create()
            return
        except Exception:
            continue
    sys.exit(
        "score_quality: found the bindings but no libclang shared library.\n"
        "  apt install libclang-dev      # or\n"
        "  venv/bin/pip install libclang # bundles its own"
    )

# Mirrors the build's own preprocessor setup (tools/claude-decomp-env/dump.sh).
# The target triple is not optional: without it clang assumes a 64-bit host,
# every sizeof assertion in the headers fails, and the parse comes back with
# 20 errors that bury any real one.
CLANG_ARGS = [
    "-Iinclude",
    "-Ibuild",
    "-Iinclude/psyq",
    "-Iinclude/decomp",
    "-D_LANGUAGE_C",
    "-DUSE_INCLUDE_ASM",
    "-DVER_USA",
    "-DSKIP_ASM",
    "-DNON_MATCHING",
    "-nostdinc",
    "-ffreestanding",
    "-w",
    # Those incomplete-array errors are benign but numerous, and clang stops
    # at 20 by default - which truncated one file's AST silently.
    "-ferror-limit=0",
    "-std=gnu89",
    "--target=mipsel-unknown-elf",
]

# A pin, a steering macro and a raw asm block are the same kind of debt; a goto
# is a smaller smell and often original code.
W_PIN = 3
W_MACRO = 3
W_RAWASM = 3
W_VOLATILE = 2
W_GOTO = 1
W_VOIDP = 1
W_BARE = 2
W_ARTIFACT = 2

# Statements that legitimately own a `{ ... }`. A compound statement whose
# parent is anything else is a scope somebody opened by hand, and in this tree
# that is nearly always to bound a pinned variable's live range.
BLOCK_OWNERS = frozenset(
    {
        ci.CursorKind.FUNCTION_DECL,
        ci.CursorKind.IF_STMT,
        ci.CursorKind.FOR_STMT,
        ci.CursorKind.WHILE_STMT,
        ci.CursorKind.DO_STMT,
        ci.CursorKind.SWITCH_STMT,
        ci.CursorKind.CASE_STMT,
        ci.CursorKind.DEFAULT_STMT,
        ci.CursorKind.LABEL_STMT,
    }
)

# "Matched but not understood" markers. These say nothing about scaffolding -
# see the --comprehension view, which scores them separately.
ANON_FUNC_RE = re.compile(r"^func_(?:[0-9A-Fa-f]{8}$|.*_[0-9A-Fa-f]{8}$)")
ANON_ARG_RE = re.compile(r"^arg\d+$")
ANON_FIELD_RE = re.compile(r"^(?:field_[0-9A-Fa-f]+|unk[0-9A-Fa-f_]*)$")
ANON_DATA_RE = re.compile(r"^D_[0-9A-Fa-f]{6,8}$")

# A COP2 instruction encoded by hand. Used only to tell a local `gte_*_real()`
# opcode fix (legitimate) from a local steering macro such as `and_mask`.
GTE_OPCODE_RE = re.compile(r"\.word\s+0x4[AB]|c(?:op|fc|tc)2|m[ft]c2", re.I)

# The switch-layout idiom:
#     switch (x) { case 0: goto L_case0; ... }  ... L_case0: /* body */
# written to control basic-block order. No human writes a switch whose every
# arm is a goto, so *this* is the goto worth counting - while a plain
# `goto kill;` for cleanup or early exit is ordinary 1999 C and is not counted
# at all. That is the opposite of what a modern style guide would say.
SWITCH_LABEL_RE = re.compile(r"^(?:L_)?case_?\d+$|^default_body$|^L_default$")

# Identifiers m2c invents when it has nothing better: named after a register or
# a stack slot. Strictly worse than `arg0`/`field_1C`, which merely lack a name
# - these actively name machine state, so no human could have written them.
ARTIFACT_RE = re.compile(r"^(?:temp_|var_[0-9A-Fa-f]+$|sp[0-9A-Fa-f]{2,}$|phi_)")

LEGIT, SCAFFOLD, RAW = "legit", "scaffold", "raw"


@dataclass
class FunctionScore:
    name: str
    path: str
    line: int
    end_line: int = 0
    pins: int = 0
    macros: int = 0
    rawasm: int = 0
    volatiles: int = 0
    gotos: int = 0
    voidps: int = 0
    tidy: int = 0
    artifacts: int = 0
    bare: int = 0
    bare_with_pin: int = 0
    anon_name: int = 0
    anon_args: int = 0
    named_args: int = 0
    anon_fields: int = 0
    anon_data: int = 0
    pin_sites: List[int] = field(default_factory=list)
    pin_regs: List[str] = field(default_factory=list)
    macro_names: List[str] = field(default_factory=list)

    @property
    def score(self) -> int:
        return (
            self.pins * W_PIN
            + self.macros * W_MACRO
            + self.rawasm * W_RAWASM
            + self.volatiles * W_VOLATILE
            + self.gotos * W_GOTO
            + self.voidps * W_VOIDP
            + self.bare * W_BARE
            + self.artifacts * W_ARTIFACT
        )

    @property
    def anon(self) -> int:
        """How much of this function is still unnamed.

        Deliberately *not* folded into `score`. Scaffolding is compiler
        steering that can be deleted against an exact oracle; anonymity is
        missing understanding, which no build can verify and which is fixed by
        research rather than by editing. Summing them would also rank every
        long function to the top regardless of how clean it is.
        """
        return self.anon_name + self.anon_args + self.anon_fields + self.anon_data

    def anon_parts(self) -> str:
        return " ".join(
            f"{k}={v}"
            for k, v in (
                ("name", self.anon_name),
                ("args", self.anon_args),
                ("fields", self.anon_fields),
                ("data", self.anon_data),
            )
            if v
        )

    def parts(self) -> str:
        return " ".join(
            f"{k}={v}"
            for k, v in (
                ("pin", self.pins),
                ("macro", self.macros),
                ("rawasm", self.rawasm),
                ("vol", self.volatiles),
                ("goto", self.gotos),
                ("void*", self.voidps),
                ("bare", self.bare),
                ("m2c", self.artifacts),
                ("tidy", self.tidy),
            )
            if v
        )


TIDY_RE = re.compile(r"^(?P<file>[^:]+):(?P<line>\d+):\d+:\s+warning:.*\[(?P<check>[\w.-]+)\]$")


def _tidy_binary() -> Optional[str]:
    from shutil import which

    for name in ("clang-tidy", "clang-tidy-17", "clang-tidy-18", "clang-tidy-16"):
        if which(name):
            return name
    return None


def _tidy_checks() -> List[str]:
    cfg = ROOT / "tools" / "score_quality_checks.yaml"
    if not cfg.exists():
        return []
    out, in_list = [], False
    for raw in cfg.read_text().splitlines():
        line = raw.split("#", 1)[0].rstrip()
        if not line:
            continue
        if line.startswith("checks:"):
            in_list = True
        elif in_list and line.lstrip().startswith("- "):
            out.append(line.lstrip()[2:].strip())
    return out


def run_tidy(path_str: str) -> List[Tuple[int, str]]:
    """(line, check) for every clang-tidy finding in one file."""
    import subprocess

    binary, checks = _tidy_binary(), _tidy_checks()
    if not binary or not checks:
        return []
    try:
        proc = subprocess.run(
            [binary, "--quiet", f"--checks=-*,{','.join(checks)}", path_str, "--"]
            + CLANG_ARGS,
            capture_output=True,
            text=True,
            timeout=300,
            cwd=ROOT,
        )
    except (subprocess.TimeoutExpired, OSError):
        return []
    hits = []
    for line in proc.stdout.splitlines():
        m = TIDY_RE.match(line.strip())
        if m and Path(m.group("file")).name == Path(path_str).name:
            hits.append((int(m.group("line")), m.group("check")))
    return hits


def _origin(defining_file: Optional[str]) -> Optional[str]:
    """Which population a macro's defining file puts it in, if it decides it."""
    if not defining_file:
        return None
    # clang echoes the path back in whatever form the -I gave it, so this has to
    # work for a relative "include/psyq/inline_c.h" as well as an absolute one.
    parts = Path(defining_file).parts
    if "psyq" in parts:
        return LEGIT
    if "decomp" in parts:
        return SCAFFOLD
    return None


def _classify_asm(
    macro: Optional[str],
    macro_def: Dict[str, str],
    macro_body: Dict[str, List[str]],
    psyq_macros: set,
) -> str:
    """legit / scaffold / raw for one inline-asm statement.

    Nested expansions are the awkward case. libclang records only the
    *outermost* macro at a call site, so `gte_MulMatrix0_real(...)` - a local
    wrapper whose body is a run of psyq `gte_SetRotMatrix` calls - arrives
    looking like a macro defined in a .c file. Classifying such a macro by its
    own definition body, rather than by the file it sits in, keeps the wrapper
    legitimate while still catching an ad-hoc helper like `and_mask`.
    """
    if macro is None:
        return RAW
    decided = _origin(macro_def.get(macro))
    if decided:
        return decided
    body = macro_body.get(macro, [])
    if any(GTE_OPCODE_RE.search(t) for t in body):
        return LEGIT  # a local gte_*_real() opcode fix
    if any(t in psyq_macros for t in body):
        return LEGIT  # a wrapper around real Psy-Q GTE calls
    return SCAFFOLD


def score_file(path_str: str, with_tidy: bool = False) -> Tuple[List[dict], Optional[str]]:
    """Parse one .c and score every function defined in it."""
    path = Path(path_str)
    index = ci.Index.create()
    try:
        tu = index.parse(
            str(path),
            args=CLANG_ARGS,
            options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD,
        )
    except ci.TranslationUnitLoadError as e:
        return [], f"{path}: {e}"

    # `extern struct Incomplete arr[];` is legal to GCC and rejected by clang,
    # so plain errors are common and harmless - the AST is still complete. Only
    # a Fatal (severity 4, e.g. a header that would not open) truncates it, and
    # only that is worth reporting.
    fatal = [d for d in tu.diagnostics if d.severity >= 4]
    warn = (
        f"{path}: {len(fatal)} fatal diagnostic(s), first: {fatal[0].spelling}"
        if fatal
        else None
    )

    # macro name -> defining header; expansion site -> macro name
    macro_def: Dict[str, str] = {}
    macro_at: Dict[Tuple[int, int], str] = {}
    macro_body: Dict[str, List[str]] = {}
    psyq_macros: set = set()
    for c in tu.cursor.walk_preorder():
        if c.kind == ci.CursorKind.MACRO_DEFINITION and c.location.file:
            macro_def[c.spelling] = c.location.file.name
            if _origin(c.location.file.name) == LEGIT:
                psyq_macros.add(c.spelling)
            else:
                # Only needed for macros whose file does not decide it.
                macro_body[c.spelling] = [t.spelling for t in c.get_tokens()]
        elif c.kind == ci.CursorKind.MACRO_INSTANTIATION and c.location.file:
            if c.location.file.name == str(path):
                macro_at[(c.location.line, c.location.column)] = c.spelling

    try:
        rel = str(path.resolve().relative_to(ROOT))
    except ValueError:
        rel = str(path)

    out: List[dict] = []
    for fn in tu.cursor.get_children():
        if fn.kind != ci.CursorKind.FUNCTION_DECL or not fn.is_definition():
            continue
        if not fn.location.file or fn.location.file.name != str(path):
            continue

        fs = FunctionScore(
            name=fn.spelling,
            path=rel,
            line=fn.location.line,
            end_line=fn.extent.end.line,
        )

        for p in fn.get_arguments():
            t = p.type
            if t.kind == ci.TypeKind.POINTER and t.get_pointee().kind == ci.TypeKind.VOID:
                fs.voidps += 1

        fs.anon_name = 1 if ANON_FUNC_RE.match(fn.spelling) else 0
        for prm in fn.get_arguments():
            if ANON_ARG_RE.match(prm.spelling or ""):
                fs.anon_args += 1
            elif prm.spelling:
                fs.named_args += 1

        # Explicit stack rather than walk_preorder(): a bare scope is defined
        # by what encloses it, and walk_preorder does not hand back parents.
        stack = [(c, fn) for c in fn.get_children()]
        while stack:
            c, parent = stack.pop()
            k = c.kind
            if k == ci.CursorKind.ASM_LABEL_ATTR:
                fs.pins += 1
                fs.pin_regs.append(c.spelling)
                fs.pin_sites.append(c.location.line)
            elif k == ci.CursorKind.ASM_STMT:
                if c.location.file and c.location.file.name == str(path):
                    mac = macro_at.get((c.location.line, c.location.column))
                    kind = _classify_asm(mac, macro_def, macro_body, psyq_macros)
                    if kind == SCAFFOLD:
                        fs.macros += 1
                        if mac:
                            fs.macro_names.append(mac)
                    elif kind == RAW:
                        fs.rawasm += 1
            elif k == ci.CursorKind.GOTO_STMT:
                label = next(
                    (
                        ch.spelling
                        for ch in c.get_children()
                        if ch.kind == ci.CursorKind.LABEL_REF
                    ),
                    "",
                )
                if SWITCH_LABEL_RE.match(label):
                    fs.gotos += 1  # asm-shaped dispatch, not ordinary early-exit
            elif k == ci.CursorKind.VAR_DECL:
                if ARTIFACT_RE.match(c.spelling or ""):
                    fs.artifacts += 1
                t = c.type
                if t.is_volatile_qualified() or (
                    t.kind == ci.TypeKind.POINTER
                    and t.get_pointee().is_volatile_qualified()
                ):
                    fs.volatiles += 1
            elif k == ci.CursorKind.COMPOUND_STMT:
                if parent.kind not in BLOCK_OWNERS:
                    fs.bare += 1
                    if any(
                        d.kind == ci.CursorKind.ASM_LABEL_ATTR
                        for d in c.walk_preorder()
                    ):
                        fs.bare_with_pin += 1
            elif k == ci.CursorKind.MEMBER_REF_EXPR:
                if ANON_FIELD_RE.match(c.spelling or ""):
                    fs.anon_fields += 1
            elif k == ci.CursorKind.DECL_REF_EXPR:
                if ANON_DATA_RE.match(c.spelling or ""):
                    fs.anon_data += 1
            for ch in c.get_children():
                stack.append((ch, c))

        if fs.score or fs.anon:
            row = asdict(fs)
            row["score"] = fs.score
            out.append(row)
    if with_tidy and out:
        for ln, _check in run_tidy(path_str):
            for row in out:
                if row["line"] <= ln <= row["end_line"]:
                    row["tidy"] += 1
                    row["score"] += 1
                    break
    return out, warn


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Rate matched C by how much matching scaffolding it carries.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__.split("Examples:")[1],
    )
    ap.add_argument("paths", nargs="*", default=["src"], help="files or dirs (default: src)")
    ap.add_argument("--family", help="restrict to one src/ family (gameplay, main, rooms, ...)")
    ap.add_argument("--top", type=int, default=40, help="rows to print (default 40)")
    ap.add_argument("--min-score", type=int, default=1, help="hide rows below this")
    ap.add_argument("--ranked", action="store_true", help="names only, worst first")
    ap.add_argument("--summary", action="store_true", help="totals instead of rows")
    ap.add_argument("--json", action="store_true", help="machine-readable output")
    ap.add_argument(
        "--pin-sites",
        action="store_true",
        help="print every register pin as file:line, to drive a removal pass "
        "(strip one, rebuild scoped, keep it if the checksum still holds)",
    )
    ap.add_argument(
        "--comprehension",
        action="store_true",
        help="rank by how much is still unnamed (func_<hex>, argN, field_XX, "
        "D_<hex>) - the research axis, which has no build oracle",
    )
    ap.add_argument(
        "--tidy",
        action="store_true",
        help="also consult clang-tidy (checks in tools/score_quality_checks.yaml). "
        "Slow - minutes per family - and measured to add little on this tree; "
        "the config explains why and is the place to tune it.",
    )
    ap.add_argument("--jobs", type=int, default=os.cpu_count(), help="parallel parses")
    ap.add_argument("--verbose", action="store_true", help="list files that failed to parse")
    args = ap.parse_args()
    _ensure_library()

    if args.family:
        roots = [ROOT / "src" / args.family]
    else:
        roots = [(Path(p) if Path(p).is_absolute() else ROOT / p) for p in args.paths]
    for r in roots:
        if not r.exists():
            sys.exit(f"score_quality: no such path: {r}")

    files: List[str] = []
    for r in roots:
        if r.is_dir():
            files.extend(str(p) for p in sorted(r.rglob("*.c")))
        else:
            files.append(str(r))
    if not files:
        sys.exit("score_quality: no .c files found")

    os.chdir(ROOT)  # CLANG_ARGS carry repo-relative include paths
    rows: List[dict] = []
    problems: List[str] = []
    # Each worker must locate libclang for itself: Config.set_library_file is
    # process-local state, and a forkserver/spawn pool does not inherit what
    # the parent set after import.
    with ProcessPoolExecutor(
        max_workers=max(1, args.jobs), initializer=_ensure_library
    ) as ex:
        from itertools import repeat

        for got, warn in ex.map(score_file, files, repeat(args.tidy), chunksize=4):
            rows.extend(got)
            if warn:
                problems.append(warn)

    if args.comprehension:
        for r in rows:
            fs = FunctionScore(**{k: v for k, v in r.items() if k != "score"})
            r["anon"] = fs.anon
            r["anon_parts"] = fs.anon_parts()
        rows = [r for r in rows if r["anon"]]
        rows.sort(key=lambda r: (-r["anon"], r["path"], r["line"]))
        if args.json:
            print(json.dumps(rows, indent=2))
            return 0
        if args.ranked:
            for r in rows:
                print(r["name"])
            return 0
        if not args.summary:
            print(f"{'anon':>6}  {'function':<42} {'detail':<34} location")
            for r in rows[: args.top]:
                print(
                    f"{r['anon']:6d}  {r['name'][:42]:<42} {r['anon_parts']:<34} "
                    f"{r['path']}:{r['line']}"
                )
            if len(rows) > args.top:
                print(f"... {len(rows) - args.top} more (use --top)")
            print()
        tot = collections.Counter()
        for r in rows:
            for k in ("anon_name", "anon_args", "anon_fields", "anon_data"):
                tot[k] += r[k]
        named = sum(r["named_args"] for r in rows)
        print(f"{len(rows)} functions carry unnamed identifiers")
        print("  totals: " + ", ".join(f"{k[5:]} {v}" for k, v in tot.most_common() if v))
        args_tot = tot["anon_args"] + named
        if args_tot:
            print(
                f"  parameters: {tot['anon_args']} of {args_tot} still argN "
                f"({100 * tot['anon_args'] / args_tot:.0f}%)"
            )
        return 0

    rows = [r for r in rows if r["score"] >= args.min_score]
    rows.sort(key=lambda r: (-r["score"], r["path"], r["line"]))

    if args.pin_sites:
        for r in rows:
            for ln in r["pin_sites"]:
                print(f"{r['path']}:{ln}")
        return 0
    if args.json:
        print(json.dumps(rows, indent=2))
        return 0
    if args.ranked:
        for r in rows:
            print(r["name"])
        return 0

    tot = collections.Counter()
    for r in rows:
        for k in (
            "pins", "macros", "rawasm", "volatiles",
            "gotos", "voidps", "bare", "artifacts",
        ):
            tot[k] += r[k]
    fam = collections.Counter(r["path"].split("/")[1] for r in rows if "/" in r["path"])

    if not args.summary:
        print(f"{'score':>6}  {'function':<42} {'detail':<34} location")
        for r in rows[: args.top]:
            fs = FunctionScore(**{k: v for k, v in r.items() if k != "score"})
            print(
                f"{r['score']:6d}  {r['name'][:42]:<42} {fs.parts():<34} "
                f"{r['path']}:{r['line']}"
            )
        if len(rows) > args.top:
            print(f"... {len(rows) - args.top} more (use --top)")
        print()

    print(f"{len(rows)} functions contain something no 1999 programmer would write")
    print(
        "  totals: "
        + ", ".join(f"{k} {v}" for k, v in tot.most_common() if v)
        + f"  (weighted {sum(r['score'] for r in rows)})"
    )
    if fam:
        print("  by family: " + ", ".join(f"{k} {v}" for k, v in fam.most_common()))
    named = collections.Counter(m for r in rows for m in r["macro_names"])
    if named:
        print(
            "  top steering macros: "
            + ", ".join(f"{m} {n}" for m, n in named.most_common(5))
        )
    if problems:
        print(
            f"  {len(problems)} file(s) had parse errors"
            + ("" if args.verbose else "; --verbose to list")
        )
        if args.verbose:
            for p in problems:
                print(f"    {p}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
