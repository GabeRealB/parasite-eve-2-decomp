#!/usr/bin/env python3
"""Report and remove unused includes, and give shared fragments the includes they use.

    venv/bin/python3 tools/refactor/include_use.py report src/weapons [--json]
    venv/bin/python3 tools/refactor/include_use.py apply src/weapons

Arguments are C sources or directories of them (found through compile_commands.json). Each source is parsed with
libclang, and its files are split into owners - files that should state their own includes:

* the source itself, with its sibling `config.h`;
* each generated asset include (`build/include/assets/`), which states the includes its declarations need
  (tools/peassets/room/c_headers.py); what one lacks is reported, never edited - its generator is the place to fix it;
* every other file under `src/` the source includes: a shared fragment or library header. Names the carrier's own
  directory declares are the fragment's contract (its binding macros, the carrier's data), not missing includes.

A name an owner uses is either an identifier token of its text (comments and literals removed; parameters, locals and
fields it declares excluded) or a use the AST resolves: a call, a reference, a type, or a member access, which needs
the record's definition rather than a forward declaration. A header provides a name it declares at file scope, as an
enumerator or as a macro. A macro defined by several headers is provided by the last one included, the definition in
effect (`gte.h` overrides `inline_c.h`'s placeholder GTE words this way).

For each used name an owner needs its own include of a declaring header; failing that, the first of its includes
whose inclusion tree reaches one. Includes that provide nothing are unused - a fragment's only when that holds in
every analyzed source including it. Names no include of a fragment provides are missing, reported against the first
declaring header in inclusion order. `.c` includes (fragments) and generated asset includes are never unused.

`apply` deletes the unused includes and adds each fragment's missing ones after its leading comment (inside a
header's guard), merged with the includes already there, in the project's group order. Sony's psyq headers include
nothing themselves, so it keeps each one's prerequisites in front of it, in Sony's order. Check the result with the
full `tools/build-and-verify.sh` and `tools/refactor/check_decls.py`: the compiler runs with `-w`, so a removed
prototype would only show as an implicit declaration.
"""
import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import clang.cindex as ci  # noqa: E402

ROOT = cref.repo_root()
SEARCH = ["include", "build", "build/include", "include/psyq", "include/decomp"]
DECL_KINDS = {ci.CursorKind.FUNCTION_DECL, ci.CursorKind.VAR_DECL, ci.CursorKind.TYPEDEF_DECL,
              ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL, ci.CursorKind.ENUM_DECL}
USE_KINDS = {ci.CursorKind.MEMBER_REF_EXPR, ci.CursorKind.DECL_REF_EXPR, ci.CursorKind.TYPE_REF, ci.CursorKind.CALL_EXPR}
LOCAL_KINDS = {ci.CursorKind.PARM_DECL, ci.CursorKind.VAR_DECL, ci.CursorKind.FIELD_DECL}
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)", re.M)
INCLUDE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"]+)[>"]', re.M)
INCLUDE_LINE = re.compile(r'^[ \t]*#[ \t]*include[ \t]*([<"])([^>"]+)[>"].*$')
IDENT = re.compile(r"[A-Za-z_]\w*")
ASSETS = "build/include/assets/"
CARRY_FRAGMENTS = False  # --only-sources: fragments state no includes of their own, so their uses are the source's
# Sony's headers in the order they have to come; each needs the ones listed before it
PSYQ = ["psyq/sys/types.h", "psyq/libgte.h", "psyq/libgpu.h", "psyq/inline_c.h", "psyq/gtemac.h", "psyq/libgs.h"]
PSYQ_NEEDS = {"psyq/libgs.h": PSYQ[:3], "psyq/gtemac.h": [PSYQ[0], PSYQ[1], PSYQ[3]], "psyq/libgpu.h": PSYQ[:2],
              "psyq/inline_c.h": PSYQ[:2], "psyq/libgte.h": PSYQ[:1]}


def rel(path: str) -> str:
    p = os.path.realpath(path)
    return os.path.relpath(p, ROOT) if p.startswith(ROOT + os.sep) else p


def read(f: str) -> str:
    try:
        with open(os.path.join(ROOT, f), errors="replace") as fh:
            return fh.read()
    except OSError:
        return ""


def strip(text: str) -> str:
    """The text with comments, string and character literals and #include lines blanked."""
    text = re.sub(r"//[^\n]*|/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r'"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', " ", text)
    return INCLUDE.sub(" ", text)


def resolve(frm: str, kind: str, name: str) -> str | None:
    for d in ([os.path.dirname(frm)] if kind == '"' else []) + SEARCH:
        p = os.path.normpath(os.path.join(d, name))
        if os.path.exists(os.path.join(ROOT, p)):
            return rel(os.path.join(ROOT, p))
    return None


def analyze(source: str, db) -> tuple[dict, list[str]]:
    """{owner: {"includes": [(file, line, target)], "unused": set(targets), "missing": {header: [names]}}}."""
    tu = cref.parse_tu(source, db[source], ROOT)
    if tu is None:
        raise SystemExit(f"{source}: libclang could not parse it")
    main = rel(os.path.join(ROOT, source))
    carrier_dir = os.path.dirname(main) + "/"

    # the inclusion tree, from the files' own #include lines: libclang omits a guarded header's second inclusion
    edges, direct, order = {}, {}, []

    def scan(f):
        if f in edges:
            return
        edges[f] = []
        text = read(f)
        for m in INCLUDE.finditer(text):
            t = resolve(f, m.group(1), m.group(2))
            if t is None:
                continue
            edges[f].append(t)
            direct.setdefault(f, []).append((text.count("\n", 0, m.start()) + 1, t))
            if t not in order:
                order.append(t)
            scan(t)

    scan(main)
    files = [main] + order
    reach_cache = {}

    def reach(f):
        if f not in reach_cache:
            seen, todo = {f}, [f]
            while todo:
                for d in edges.get(todo.pop(), []):
                    if d not in seen:
                        seen.add(d)
                        todo.append(d)
            reach_cache[f] = seen
        return reach_cache[f]

    declared, macros = {}, {}

    def note(name, f):
        if name:
            declared.setdefault(name, set()).add(f)

    for cur in tu.cursor.get_children():
        if cur.location.file is None:
            continue
        if cur.kind in DECL_KINDS:
            note(cur.spelling, rel(cur.location.file.name))
        if cur.kind in (ci.CursorKind.ENUM_DECL, ci.CursorKind.TYPEDEF_DECL):
            for c in cur.walk_preorder():
                if c.kind == ci.CursorKind.ENUM_CONSTANT_DECL and c.location.file is not None:
                    note(c.spelling, rel(c.location.file.name))
    bodies = {}  # macro name -> identifiers of its definitions' bodies
    for f in files:
        text = strip(read(f)).replace("\\\n", " ")
        for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_]\w*)([^\n]*)", text, re.M):
            note(m.group(1), f)
            macros.setdefault(m.group(1), []).append(f)
            bodies.setdefault(m.group(1), set()).update(IDENT.findall(m.group(2)))

    def owner_of(f):
        if f == main or f == carrier_dir + "config.h":
            return main
        if f.startswith(ASSETS):
            return f                                    # a generated include states its own includes
        if f.startswith("src/") and not f.startswith(carrier_dir):
            return main if CARRY_FRAGMENTS else f           # --only-sources: the source keeps what its fragments use
        return None

    def record_of(c):
        while c is not None and c.kind not in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL):
            c = c.semantic_parent
        return c

    ast_uses, locals_of = {}, {}
    for c in tu.cursor.walk_preorder():
        if c.location.file is None:
            continue
        uf = rel(c.location.file.name)
        if c.kind in LOCAL_KINDS and (c.semantic_parent is None or c.semantic_parent.kind != ci.CursorKind.TRANSLATION_UNIT):
            locals_of.setdefault(uf, set()).add(c.spelling)
        if c.kind not in USE_KINDS:
            continue
        ref = c.referenced
        if ref is None or ref.location.file is None:
            continue
        if c.kind == ci.CursorKind.MEMBER_REF_EXPR:
            rec = record_of(ref)
            if rec is None or rec.location.file is None:
                continue
            ref = rec.get_definition() or rec
            df = rel(ref.location.file.name)
            label = ref.spelling if ref.spelling and "(unnamed" not in ref.spelling else f"anonymous record at line {ref.location.line}"
            name = f"record {label} in {df}"  # needs the definition, not a forward declaration
        elif ref.kind == ci.CursorKind.ENUM_CONSTANT_DECL or (
                ref.semantic_parent is not None and ref.semantic_parent.kind == ci.CursorKind.TRANSLATION_UNIT):
            df = rel(ref.location.file.name)
            name = ref.spelling
        else:
            continue  # parameters, locals and fields are no header's export
        ast_uses.setdefault(uf, set()).add(name)
        declared.setdefault(name, set()).add(df)

    owners = {}
    for f in files:
        o = owner_of(f)
        if o:
            owners.setdefault(o, []).append(f)
    report = {}
    for owner, members in owners.items():
        includes = [(m, line, dst) for m in members for line, dst in direct.get(m, []) if owner_of(dst) != owner]
        targets = [t for _m, _l, t in includes]
        local = set().union(*(locals_of.get(m, set()) for m in members))
        used = set(IDENT.findall(" ".join(strip(read(m)) for m in members))) - local
        used |= set().union(*(ast_uses.get(m, set()) for m in members))
        # a macro's expansion uses what its body names: gte_ApplyMatrixSV needs gte.h's gte_rtv0
        todo = [n for n in used if n in bodies]
        while todo:
            for n in bodies.get(todo.pop(), ()):
                if n not in used:
                    used.add(n)
                    todo.append(n)
        needed, missing = set(), {}
        for name in sorted(used):
            hs = {h for h in declared.get(name, ()) if h.endswith(".h") and owner_of(h) != owner
                  and (owner == main or not h.startswith(carrier_dir))}   # a fragment's carrier binds those names
            if not hs:
                continue
            if len(macros.get(name, ())) > 1:
                hs = set([f for f in files if f in hs and f in macros[name]][-1:]) or hs
            hit = [t for t in targets if t in hs] or [t for t in targets if reach(t) & hs][:1]
            if hit:
                needed.add(hit[0])
            elif owner != main:
                missing.setdefault(next((f for f in files if f in hs), min(hs)), []).append(name)
        # a psyq header another kept one needs stays: Sony's headers include nothing themselves
        for t in list(needed):
            needed |= {"include/" + n for n in PSYQ_NEEDS.get(t[len("include/"):], ())}
        report[owner] = {
            "includes": includes,
            "unused": {t for _m, _l, t in includes if t not in needed and not t.endswith(".c") and not t.startswith(ASSETS)},
            "missing": missing,
        }
    implicit = sorted({d.spelling for d in tu.diagnostics if "implicit declaration" in d.spelling})
    return report, implicit


def collect(paths, db):
    sources = []
    for p in paths:
        p = rel(os.path.join(ROOT, p))
        if p.endswith(".c"):
            sources.append(p)
        else:
            sources += sorted(f for f in db if f.startswith(p.rstrip("/") + "/") and not f.endswith(".inc.c"))
    unused, missing, seen_in, implicit = {}, {}, {}, {}
    for s in dict.fromkeys(sources):
        if s not in db:
            continue
        report, imp = analyze(s, db)
        if imp:
            implicit[s] = imp
        for owner, o in report.items():
            for m in {m for m, _l, _t in o["includes"]}:
                seen_in[m] = seen_in.get(m, 0) + 1
            for m, line, t in o["includes"]:
                if t in o["unused"]:
                    unused.setdefault((m, t), {"line": line, "count": 0})["count"] += 1
            for h, names in o["missing"].items():
                missing.setdefault(owner, {}).setdefault(h, set()).update(names)
    # an include of a file several sources pull in is unused only where every one of them agrees
    unused = {k: v for k, v in unused.items() if v["count"] == seen_in[k[0]]}
    return unused, missing, implicit


def spell(owner: str, header: str) -> str:
    if header.startswith("include/psyq/"):
        return f"<{header[len('include/'):]}>"
    for prefix in ("include/decomp/", "include/"):
        if header.startswith(prefix):
            return f'"{header[len(prefix):]}"'
    return f'"{os.path.relpath(header, os.path.dirname(owner))}"'


def rank(spec: str):
    name = spec[1:-1]
    if spec.startswith("<"):
        return (0, PSYQ.index(name) if name in PSYQ else len(PSYQ), name)
    if name in ("common.h", "gte.h", "types.h"):
        return (1, 0, name)
    top = name.split("/")[0] if "/" in name and not name.startswith(".") else None
    groups = {"gameplay": 2, "main": 3, "weapons": 4}
    return (groups[top], 0, name) if top in groups else (5 if "/" not in name else 6, 0, name)


def complete_psyq(specs: set[str]) -> set[str]:
    names = {s[1:-1] for s in specs if s.startswith("<")}
    for n in list(names):
        names |= set(PSYQ_NEEDS.get(n, ()))
    return {s for s in specs if not s.startswith("<")} | {f"<{n}>" for n in names}


def remove_includes(f: str, targets: set[str]) -> None:
    lines = [l for l in read(f).split("\n")
             if not ((m := INCLUDE_LINE.match(l)) and resolve(f, m.group(1), m.group(2)) in targets)]
    idx = [i for i, l in enumerate(lines) if (m := INCLUDE_LINE.match(l)) and m.group(1) == "<" and m.group(2) in PSYQ]
    if idx:
        group = sorted(complete_psyq({f"<{INCLUDE_LINE.match(lines[i]).group(2)}>" for i in idx}), key=rank)
        lines = [l for i, l in enumerate(lines) if i not in idx]
        lines[idx[0]:idx[0]] = [f"#include {s}" for s in group]
    text = re.sub(r"\n{3,}(?=#)", "\n\n", "\n".join(lines))
    with open(os.path.join(ROOT, f), "w") as fh:
        fh.write(text)


def add_includes(owner: str, headers: set[str]) -> None:
    lines = read(owner).split("\n")

    def start(ls):
        i = 0
        if ls and ls[0].startswith("/*"):
            while "*/" not in ls[i]:
                i += 1
            i += 1
        if owner.endswith(".h"):
            i = next(n for n, l in enumerate(ls) if l.startswith("#ifndef")) + 2
        return i

    # the includes at the top (before the comment, or right after it or the guard) join the block
    top, i = [], start(lines)
    for j in range(len(lines)):
        if j >= i and not (INCLUDE_LINE.match(lines[j]) or not lines[j].strip()):
            break
        if INCLUDE_LINE.match(lines[j]):
            top.append(j)
    present = {INCLUDE_LINE.match(lines[t]).group(1) + INCLUDE_LINE.match(lines[t]).group(2) for t in top}
    present = {(f"<{s[1:]}>" if s[0] == "<" else f'"{s[1:]}"') for s in present}
    have = {resolve(owner, s[0], s[1:-1]) for s in present}
    specs = complete_psyq(present | {spell(owner, h) for h in headers if h not in have and h != owner})
    if specs == present:
        return
    block, last = [], None
    for s in sorted(specs, key=rank):
        if last is not None and rank(s)[0] != last:
            block.append("")
        block.append(f"#include {s}")
        last = rank(s)[0]
    body = [l for n, l in enumerate(lines) if n not in set(top)]
    at = start(body)
    while at < len(body) and not body[at].strip():
        at += 1
    body[at:at] = ([""] if at else []) + block + [""]
    with open(os.path.join(ROOT, owner), "w") as fh:
        fh.write(re.sub(r"\n{3,}", "\n\n", "\n".join(body)))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("mode", choices=["report", "apply"])
    ap.add_argument("paths", nargs="+", help="C sources, or directories of them")
    ap.add_argument("--json", action="store_true", help="report as JSON")
    ap.add_argument("--only-sources", action="store_true",
                    help="apply: edit only the named sources, not the fragments they include (a fragment's includes "
                         "are judged against every source including it: run over all of them for those)")
    args = ap.parse_args()
    db = cref.load_db(ROOT)
    global CARRY_FRAGMENTS
    CARRY_FRAGMENTS = args.only_sources
    unused, missing, implicit = collect(args.paths, db)
    if args.mode == "report":
        if args.json:
            print(json.dumps({"unused": [f"{m}:{v['line']} {t}" for (m, t), v in sorted(unused.items())],
                              "missing": {o: {h: sorted(n) for h, n in hs.items()} for o, hs in sorted(missing.items())},
                              "implicit": implicit}, indent=1))
        else:
            for (m, t), v in sorted(unused.items()):
                print(f"unused   {m}:{v['line']} {t}")
            for o, hs in sorted(missing.items()):
                for h, names in sorted(hs.items()):
                    print(f"missing  {o} <- {h} ({', '.join(sorted(names)[:5])})")
            for s, imp in implicit.items():
                print(f"implicit {s}: {'; '.join(imp)}")
        return 0
    by_file = {}
    for (m, t) in unused:
        if not m.startswith(ASSETS) and (not args.only_sources or m.endswith(".c") and not m.endswith(".inc.c")):
            by_file.setdefault(m, set()).add(t)          # generated: its generator is the place to change
    for f, ts in sorted(by_file.items()):
        remove_includes(f, ts)
    fragments = {} if args.only_sources else {o: hs for o, hs in missing.items() if not o.startswith(ASSETS)}
    for o, hs in sorted(fragments.items()):
        add_includes(o, set(hs))
    removed = sum(len(ts) for ts in by_file.values())
    print(f"removed {removed} includes from {len(by_file)} files; added includes to {len(fragments)} fragments")
    return 0


if __name__ == "__main__":
    sys.exit(main())
