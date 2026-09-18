#!/usr/bin/env python3
"""Order the naming work by dependency, and say what is ready to work on.

Naming an item well means knowing what it is made of. A function cannot be
described honestly before the functions it calls, the globals it touches and the
types it passes around have been described, because until then its own summary
can only restate the assembly. So the work has an order, and the order is a
graph: process the leaves, and each item becomes a leaf once everything it uses
has been processed.

    dep_graph.py --build              build the graph (slow, cached)
    dep_graph.py ready <spec>         is this item ready to work on?
    dep_graph.py next [<spec>]        the next leaf to process
    dep_graph.py stats                how much of the graph is processed

`<spec>` is the form the other refactor tools take, e.g.
`src/main/stage.c/stageSetFadeRate`. With no argument, `next` considers the
whole graph.

**Cycles.** Mutual recursion and mutually referencing types mean the graph is
not a DAG. Each strongly connected component is collapsed to a single node, so a
cycle becomes one unit of work that has to be understood together rather than a
deadlock. `next` reports the whole component when it picks one.

The graph is cached under the gitignored local directory. It describes the tree
as it was when built; rebuild after landing a batch.
"""

import argparse
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import clang.cindex as ci  # noqa: E402
import cref  # noqa: E402
import name_index  # noqa: E402

DEFAULT_GRAPH = os.path.join("local", "dep_graph.json")

_DEF_KINDS = {ci.CursorKind.FUNCTION_DECL, ci.CursorKind.VAR_DECL}
_TYPE_KINDS = {ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL,
               ci.CursorKind.TYPEDEF_DECL, ci.CursorKind.ENUM_DECL}
_USE_KINDS = {
    ci.CursorKind.CALL_EXPR,
    ci.CursorKind.DECL_REF_EXPR,
    ci.CursorKind.MEMBER_REF_EXPR,
    ci.CursorKind.TYPE_REF,
}


# Aliases for machine types. They are referenced by almost everything and are
# never work, so treating them as graph nodes would make every item depend on
# them and drown the real structure.
_PRIMITIVE = {
    "s8", "s16", "s32", "s64", "u8", "u16", "u32", "u64", "f32", "f64",
    "size_t", "bool", "byte", "void", "char", "int", "short", "long",
    "unsigned", "signed", "float", "double", "va_list", "ptrdiff_t",
}


def _is_local(usr: str) -> bool:
    """A local or parameter, identified by the byte offset in its USR."""
    import re
    return bool(re.search(r"@\d+@", usr))


def scan_tu(job):
    """(node, [used nodes]) for every definition in one translation unit."""
    rel, args, root = job
    tu = cref.parse_tu(rel, args, root)
    if tu is None:
        return []
    out = []
    for cur in tu.cursor.get_children():
        # A type depends on the types of its fields. Without this the graph has
        # no type-to-type edges at all, every type looks like a leaf, and the
        # cycles that types form with each other disappear.
        if cur.kind in _TYPE_KINDS:
            t_usr = cur.get_usr()
            loc = cur.location
            if not t_usr or loc.file is None:
                continue
            where = cref.relpath(loc.file.name, root)
            if where.startswith("..") or "psyq" in where:
                continue
            uses = set()
            # A typedef has no fields of its own; without an edge to the record
            # it names, it is a dead end that silently breaks every cycle those
            # records form with each other.
            if cur.kind == ci.CursorKind.TYPEDEF_DECL:
                d = cur.underlying_typedef_type.get_declaration()
                if d is not None and d.spelling and d.spelling not in _PRIMITIVE:
                    d_usr = d.get_usr()
                    if d_usr and d_usr != t_usr:
                        uses.add((d_usr, d.spelling))
            for f in cur.get_children():
                if f.kind != ci.CursorKind.FIELD_DECL:
                    continue
                for r in f.get_children():
                    if r.kind != ci.CursorKind.TYPE_REF:
                        continue
                    d = r.referenced
                    if d is None or d.spelling in _PRIMITIVE:
                        continue
                    d_usr = d.get_usr()
                    if d_usr and d_usr != t_usr:
                        uses.add((d_usr, d.spelling))
            if cur.spelling:
                out.append(((t_usr, cur.spelling, where), sorted(uses)))
            continue
        if cur.kind not in _DEF_KINDS or not cur.is_definition():
            continue
        loc = cur.location
        if loc.file is None:
            continue
        where = cref.relpath(loc.file.name, root)
        if where.startswith("..") or not where.startswith("src"):
            continue
        usr = cur.get_usr()
        if not usr or _is_local(usr):
            continue
        uses = set()
        stack = [cur]
        while stack:
            node = stack.pop()
            stack.extend(node.get_children())
            if node.kind not in _USE_KINDS:
                continue
            ref = node.referenced
            if ref is None:
                continue
            # A field is not an item on its own: it is understood as part of
            # the type that declares it, so the dependency is on that type.
            if ref.kind == ci.CursorKind.FIELD_DECL:
                owner = ref.semantic_parent
                if owner is None or not owner.spelling:
                    continue
                ref = owner
            if ref.spelling in _PRIMITIVE:
                continue
            r_usr = ref.get_usr()
            if not r_usr or r_usr == usr or _is_local(r_usr):
                continue
            uses.add((r_usr, ref.spelling))
        out.append(((usr, cur.spelling, where), sorted(uses)))
    return out


def build(root: str, version: str, jobs: int, out_path: str) -> None:
    from multiprocessing import Pool

    db = cref.load_db(root, version)
    files = list(db)
    nodes, edges = {}, {}
    done = 0
    with Pool(jobs) as pool:
        for res in pool.imap_unordered(scan_tu, [(f, db[f], root) for f in files], chunksize=4):
            done += 1
            if done % 50 == 0 or done == len(files):
                print(f"\r  parsed {done}/{len(files)} TUs", end="", file=sys.stderr, flush=True)
            for (usr, spelling, where), uses in res:
                nodes[usr] = {"name": spelling, "file": where}
                edges.setdefault(usr, set()).update(u for u, _ in uses)
                for u, s in uses:
                    nodes.setdefault(u, {"name": s, "file": ""})
    print(file=sys.stderr)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as fh:
        json.dump({"nodes": nodes, "edges": {k: sorted(v) for k, v in edges.items()}}, fh)
    print(f"{len(nodes)} nodes, {sum(len(v) for v in edges.values())} edges "
          f"-> {os.path.relpath(out_path, root)}", file=sys.stderr)


# --------------------------------------------------------------------------
# querying
# --------------------------------------------------------------------------


def load(path: str):
    if not os.path.exists(path):
        sys.exit(f"no graph at {path}; run dep_graph.py --build first")
    g = json.load(open(path))
    return g["nodes"], {k: set(v) for k, v in g["edges"].items()}


def processed_set(root: str, nodes: dict) -> set:
    """USRs whose name already follows the convention.

    Naming and documenting go together, but only the name can be judged
    mechanically, so that is what readiness is measured on.
    """
    vendor = name_index.vendored_names(root)
    out = set()
    for usr, meta in nodes.items():
        name = meta["name"]
        # Nothing defined in this tree cannot be work: library symbols and
        # machine-type aliases are out of scope and must not block anything.
        if name in vendor or name in _PRIMITIVE:
            out.add(usr)
            continue
        kind = "func" if "@F@" in usr else "data"
        if name_index.classify(name, kind, vendor) == "current":
            out.add(usr)
    return out


def components(nodes, edges):
    """Strongly connected components, so a cycle is one unit of work.

    Kosaraju rather than Tarjan: two plain iterative passes are far easier to
    get right than one, and an earlier hand-rolled Tarjan here silently
    reported that a graph of 156k edges contained no cycles at all.
    """
    order, seen = [], set()
    for start in nodes:
        if start in seen:
            continue
        stack = [(start, False)]
        while stack:
            v, done_ = stack.pop()
            if done_:
                order.append(v)
                continue
            if v in seen:
                continue
            seen.add(v)
            stack.append((v, True))
            for w in edges.get(v, ()):
                if w in nodes and w not in seen:
                    stack.append((w, False))
    rev = collections.defaultdict(set)
    for v, ws in edges.items():
        if v not in nodes:
            continue
        for w in ws:
            if w in nodes:
                rev[w].add(v)
    comp, assigned = {}, set()
    for v in reversed(order):
        if v in assigned:
            continue
        group, stack = [], [v]
        assigned.add(v)
        while stack:
            x = stack.pop()
            group.append(x)
            for y in rev.get(x, ()):
                if y not in assigned:
                    assigned.add(y)
                    stack.append(y)
        g = tuple(sorted(group))
        for x in group:
            comp[x] = g
    return comp


def closure(start, edges, nodes):
    seen, stack = set(), [start]
    while stack:
        v = stack.pop()
        if v in seen:
            continue
        seen.add(v)
        stack.extend(w for w in edges.get(v, ()) if w in nodes)
    return seen


def leaves(cands, edges, nodes, done, comp):
    """Components all of whose outside dependencies are already processed."""
    out = []
    for usr in cands:
        if usr in done:
            continue
        group = comp.get(usr, (usr,))
        deps = set()
        for m in group:
            deps |= {w for w in edges.get(m, ()) if w in nodes and w not in group}
        if deps <= done:
            out.append(group)
    uniq = {g: None for g in out}
    return list(uniq)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="?", default="stats",
                    choices=["ready", "next", "stats"])
    ap.add_argument("spec", nargs="?")
    ap.add_argument("--build", action="store_true", help="rebuild the cached graph")
    ap.add_argument("--graph", default=None)
    ap.add_argument("--version", default=cref.DEFAULT_VERSION)
    ap.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("-n", "--limit", type=int, default=10)
    args = ap.parse_args()

    root = cref.repo_root()
    path = args.graph or os.path.join(root, DEFAULT_GRAPH)
    if not os.path.isabs(path):
        path = os.path.join(root, path)

    if args.build:
        build(root, args.version, args.jobs, path)
        return 0

    nodes, edges = load(path)
    done = processed_set(root, nodes)
    comp = components(nodes, edges)

    if args.command == "stats":
        cyc = {g for g in comp.values() if len(g) > 1}
        print(f"nodes {len(nodes)}   processed {len(done)}   remaining {len(nodes) - len(done)}")
        print(f"cycles (components > 1): {len(cyc)}, largest {max((len(g) for g in cyc), default=0)}")
        print(f"ready now: {len(leaves(nodes, edges, nodes, done, comp))}")
        return 0

    target = None
    if args.spec:
        spec = cref.parse_spec(args.spec)
        usr, kind, where = cref.resolve(spec, root, cref.load_db(root, args.version))
        target = usr
        if usr not in nodes:
            sys.exit(f"{spec.name} is not in the graph; rebuild after it was added")

    if args.command == "ready":
        group = comp.get(target, (target,))
        deps = set()
        for m in group:
            deps |= {w for w in edges.get(m, ()) if w in nodes and w not in group}
        missing = sorted(deps - done, key=lambda u: nodes[u]["name"])
        if not missing:
            print(f"READY: {nodes[target]['name']} — every item it uses is processed")
            return 0
        print(f"NOT READY: {nodes[target]['name']} uses {len(missing)} unprocessed item(s)")
        for u in missing[:args.limit]:
            print(f"    {nodes[u]['name']}  ({nodes[u]['file'] or 'declared elsewhere'})")
        if len(missing) > args.limit:
            print(f"    ... {len(missing) - args.limit} more")
        return 1

    # next
    cands = closure(target, edges, nodes) if target else set(nodes)
    ready = leaves(cands, edges, nodes, done, comp)
    if not ready:
        print("nothing ready" + (" in that closure" if target else ""))
        return 1
    ready.sort(key=lambda g: (len(g), nodes[g[0]]["name"]))
    for group in ready[:args.limit]:
        if len(group) == 1:
            m = nodes[group[0]]
            print(f"{m['name']}\t{m['file']}")
        else:
            names = ", ".join(nodes[m]["name"] for m in group)
            print(f"[cycle of {len(group)}] {names}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
