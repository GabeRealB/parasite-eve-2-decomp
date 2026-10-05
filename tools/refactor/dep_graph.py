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
    dep_graph.py round --worklist F --workers N < orders
                                      which of these independent steps can
                                      share a parallel round

`<spec>` is the form the other refactor tools take, e.g.
`src/main/stage.c/stageSetFadeRate`. With no argument, `next` considers the
whole graph.

Project macros use definition-file/name identity and conservative lexical
dependencies, including conditional configuration and shared-source bindings.
The reference tool supplies their definitions and potential uses for review.

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
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import name_index  # noqa: E402
import macro_refs  # noqa: E402
import ref_index  # noqa: E402
from ref_index import _PRIMITIVE  # noqa: E402

DEFAULT_GRAPH = os.path.join("local", "dep_graph.json")



def scan_tu(job):
    """((node, [used nodes]) for every definition, SDK USRs) for one translation unit."""
    rel, args, root = job
    tu = cref.parse_tu(rel, args, root)
    if tu is None:
        return [], set()
    return ref_index.graph_records(tu, root), ref_index.sdk_usrs(tu, root)


def build(root: str, version: str, jobs: int, out_path: str) -> None:
    from multiprocessing import Pool

    nodes, edges = {}, {}
    sdk = set()

    def from_parse():
        db = cref.load_db(root, version)
        files = list(db)
        done = 0
        with Pool(jobs) as pool:
            for res, tu_sdk in pool.imap_unordered(scan_tu, [(f, db[f], root) for f in files], chunksize=4):
                done += 1
                if done % 50 == 0 or done == len(files):
                    print(f"\r  parsed {done}/{len(files)} TUs", end="", file=sys.stderr, flush=True)
                yield res, tu_sdk

    def from_index():
        # The reference index holds the same records, refreshed for whatever
        # changed since, so the graph needs no parse of its own.
        records, index_sdk = ref_index.fresh(root, jobs).graph()
        print(f"  {len(records)} records from the reference index", end="", file=sys.stderr)
        yield records, index_sdk

    for res, tu_sdk in (from_index() if ref_index.enabled() else from_parse()):
            sdk |= tu_sdk
            for (usr, spelling, where, line, start_line, end_line), uses in res:
                # An extern declaration must not erase the definition's extent.
                if where or not nodes.get(usr, {}).get("file"):
                    nodes[usr] = {"name": spelling, "file": where, "line": line,
                                  "start_line": start_line, "end_line": end_line}
                edges.setdefault(usr, set()).update(u for u, _ in uses)
                for u, s in uses:
                    nodes.setdefault(u, {"name": s, "file": ""})
    print(file=sys.stderr)

    # A typedef and the record it names are one thing to a reader, but two
    # declarations to the parser, so they arrive as two nodes with the same
    # spelling. Left apart they double-count: a caller of one type is reported
    # as depending on both `Foo` and `_Foo`.
    alias = {}
    for usr, deps in edges.items():
        if "@T@" not in usr:
            continue
        name = nodes[usr]["name"]
        for d in deps:
            if d in nodes and nodes[d]["name"].lstrip("_") == name.lstrip("_"):
                alias[usr] = d
                break
    if alias:
        for usr, target in alias.items():
            keep = nodes.pop(usr, None)
            if not keep:
                continue
            tgt = nodes.setdefault(target, dict(keep))
            # Keep the record's location, because the doc comment sits above
            # the declaration it opens, but take the *typedef's* spelling: that
            # is what a reader calls the type, and what the convention judges.
            # The tag may carry a leading underscore, which would otherwise be
            # read as the private-symbol marker - making a public type look
            # already-conventional and dropping it from the worklist.
            tgt["name"] = keep["name"]
            tgt["file"] = tgt.get("file") or keep.get("file", "")
            edges.pop(usr, None)
        for usr in list(edges):
            edges[usr] = {alias.get(d, d) for d in edges[usr]} - {usr}
        print(f"  merged {len(alias)} typedef/record pairs", file=sys.stderr)

    for usr in sdk & set(nodes):
        nodes[usr]["sdk"] = True

    # A file-local symbol of a shared fragment is one definition, but every
    # translation unit that includes the fragment gives it its own USR
    # (`c:<carrier>.c@Name`), so it arrived as one node per carrier - and one
    # worklist step per carrier for a single line of source. Nodes declared at
    # the same place under the same name are one item.
    by_place = collections.defaultdict(list)
    for usr, meta in nodes.items():
        if meta.get("file"):
            by_place[(meta["name"], meta["file"], meta.get("line"))].append(usr)
    same = {}
    for group in by_place.values():
        if len(group) > 1:
            keep = min(group)
            for usr in group:
                if usr != keep:
                    same[usr] = keep
    if same:
        for usr, keep in same.items():
            nodes.pop(usr, None)
            edges.setdefault(keep, set()).update(edges.pop(usr, set()))
        for usr in list(edges):
            edges[usr] = {same.get(d, d) for d in edges[usr]} - {usr}
        alias.update(same)
        print(f"  merged {len(same)} per-carrier copies of shared-fragment symbols", file=sys.stderr)

    owned = import_aliases(root, version, nodes, edges)
    if owned:
        alias.update(owned)
        print(f"  merged {len(owned)} references into another image with the definition they name", file=sys.stderr)

    tied = package_aliases(root, version, nodes, edges)
    if tied:
        print(f"  tied {tied} package aliases to the definitions they export", file=sys.stderr)

    macro_refs.add_graph(root, nodes, edges)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w") as fh:
        json.dump({"nodes": nodes,
                   "edges": {k: sorted(v) for k, v in edges.items()},
                   "alias": alias, "macro_inventory": 1}, fh)
    print(f"{len(nodes)} nodes, {sum(len(v) for v in edges.values())} edges "
          f"-> {os.path.relpath(out_path, root)}", file=sys.stderr)


# --------------------------------------------------------------------------
# querying
# --------------------------------------------------------------------------


_ASM_NAMES: dict = {}


def assembly_names(root: str, version: str = "USA") -> set:
    """Names that still have assembly of their own to be matched.

    A placeholder with no C definition blocks what uses it only when its
    meaning is still locked in assembly. Otherwise it is a reference to
    something outside the images - a slot's load address, a fixed buffer - and
    its uses are all the evidence there will ever be.
    """
    key = (root, version)
    if key not in _ASM_NAMES:
        names = set()
        base = os.path.join(root, "asm", version)
        for dirpath, _, files in os.walk(base):
            if "nonmatchings" not in dirpath.split(os.sep):
                continue
            names.update(f[:-2] for f in files if f.endswith(".s"))
        _ASM_NAMES[key] = names
    return _ASM_NAMES[key]


def package_aliases(root: str, version: str, nodes: dict, edges: dict) -> int:
    """Make each package alias one step with the definition it exports.

    A source built for several packages defines a symbol once, and the manifest
    gives the other packages' copies names of their own (`aliases` on a slot,
    DEFINE_ALIAS). An alias is a name for the same code, so it is understood
    and named with its definition, never ahead of it: each waits for the other,
    which makes them one component and so one step.
    """
    import tomllib
    try:
        with open(os.path.join(root, "configs", version, "overlays.toml"), "rb") as fh:
            manifest = tomllib.load(fh)
    except OSError:
        return 0
    by_name = collections.defaultdict(list)
    for usr, meta in nodes.items():
        by_name[meta["name"]].append(usr)
    tied = 0
    for spec in manifest.values():
        for entry in (spec.get("overlays") or {}).values():
            for slot in entry.get("slots") or []:
                for orig, name in (slot.get("aliases") or {}).items():
                    for a in by_name.get(name, []):
                        for d in by_name.get(orig, []):
                            edges.setdefault(a, set()).add(d)
                            edges.setdefault(d, set()).add(a)
                            tied += 1
    return tied


def import_aliases(root: str, version: str, nodes: dict, edges: dict) -> dict:
    """Fold a reference into another image into the definition it names.

    A reference with `owner=` in its symbol map means one image's symbol
    (tools/check_symbols.py verifies that). Where it is spelled as its owner
    spells it, the parser has already made them one node. Where it cannot be -
    one copy of a source built for several slots, or a package of a variant
    source, has no name of its own to give - the reference is spelled after the
    package and address, and without this it is a second item with no
    definition: something that looks as if it were still assembly, and blocks
    everything that uses it.
    """
    tools = os.path.join(root, "tools")
    if tools not in sys.path:
        sys.path.insert(0, tools)
    try:
        import check_symbols as cs
        from pathlib import Path
        configs = cs.load_configs(Path(root))
    except Exception:
        return {}
    out_dir = os.path.join(root, "build", version, "out")
    by_name = collections.defaultdict(list)
    for usr, meta in nodes.items():
        by_name[meta["name"]].append(usr)
    syms: dict = {}
    merged: dict = {}
    for image, cfg in configs.items():
        for d in cs.parse_decls(Path(root), image, cfg["syms"]):
            owner = d.attrs.get("owner")
            if not owner:
                continue
            elf = os.path.join(out_dir, owner + ".elf")
            if owner not in syms:
                syms[owner] = cs.image_symbols_at(Path(elf)) if os.path.isfile(elf) else {}
            sources = [u for u in by_name.get(d.name, ()) if not nodes.get(u, {}).get("file")]
            if not sources:
                continue
            targets = [u for n, _ in syms[owner].get(d.addr, ()) if n != d.name
                       for u in by_name.get(n, ()) if nodes.get(u, {}).get("file")]
            if not targets:
                continue
            keep = min(targets)
            for usr in sources:
                if usr in nodes and usr != keep:
                    merged[usr] = keep
    for usr, keep in merged.items():
        nodes.pop(usr, None)
        edges.setdefault(keep, set()).update(edges.pop(usr, set()))
    if merged:
        for usr in list(edges):
            edges[usr] = {merged.get(d, d) for d in edges[usr]} - {usr}
    return merged


def load(path: str):
    if not os.path.exists(path):
        sys.exit(f"no graph at {path}; run dep_graph.py --build first")
    g = json.load(open(path))
    if not g.get("macro_inventory"):
        sys.exit("graph predates macro inventory; run dep_graph.py --build first")
    # Merged typedef USRs redirect to the record they name, so a spec that
    # resolves to the typedef still finds its node.
    return (g["nodes"], {k: set(v) for k, v in g["edges"].items()},
            g.get("alias", {}))


def processed_set(root: str, nodes: dict) -> set:
    """USRs the pass has already handled, from its own records.

    What a name looks like cannot say whether it was processed: a type may have
    arrived already spelled to the convention and never been examined. So this
    reads the records the pass writes - the driver's step ledger and the
    renamer's log, both under the gitignored local directory - rather than
    inferring anything. Library and machine types are excluded as well, not
    because they are done but because they are out of scope.
    """
    vendor = name_index.vendored_names(root)
    out = set()
    handled = _ledger_names(root)
    former = _former_names(root)
    for usr, meta in nodes.items():
        name = meta["name"]
        if name in handled or (former.get(name, set()) & handled):
            out.add(usr)
        elif _out_of_scope(name, meta, vendor):
            out.add(usr)
    return out


def _out_of_scope(name: str, meta: dict, vendor: set) -> bool:
    """Is this not the pass's work at all?

    Distinct from having been processed: these are never named or documented,
    they just must not block anything that depends on them. Three kinds beyond
    the library and machine types - the decomp's own scaffolding headers, the
    assembler shim symbols, and the symbols the static-assert macro generates,
    which exist once per assertion and carry no meaning of their own.
    """
    if meta.get("kind") == "macro":
        return False  # The inventory already excludes guards and infrastructure.
    if meta.get("sdk"):
        return True  # Declared in a Psy-Q header.
    if name in vendor or name in _PRIMITIVE:
        return True
    if name.startswith("__maspsx_") or name.startswith("static_assertion_"):
        return True
    if name.startswith("__builtin_"):
        return True  # The compiler's own; there is no declaration to name.
    m = re.fullmatch(r"(?:D|func)_(8[0-9A-Fa-f]{7})", name)
    if m and not meta.get("file") and int(m.group(1), 16) >= 0x80200000:
        # An address past the retail console's memory: debug tooling that was
        # resident on a development unit. No image of the game holds it, so
        # there is nothing to read a name from.
        return True
    if "(unnamed at " in name or "(anonymous at " in name:
        # An inline struct or union with no tag: there is nothing to name, and
        # the driver could never select it - it looks for an item by its name
        # in the sources. Its fields are reviewed with the type that holds it.
        return True
    return (meta.get("file") or "").startswith("include/decomp/")


def _ledger_names(root: str) -> set:
    """Items the driver recorded finishing, under the name the worklist gave.

    The step ledger is the only record of what has been *processed*. The
    renamer's log is deliberately not consulted for this: a step renames
    whatever its analysis requires - a merged-away duplicate, a field, a
    parameter - and none of that means the renamed thing was itself examined.
    Treating a rename as evidence of processing marked a parameter name, and
    two types that had merely been deleted, as done.
    """
    names = set()
    path = os.path.join(root, "local/name_pass_done.tsv")
    if os.path.exists(path):
        with open(path) as fh:
            for line in fh:
                fields = line.rstrip("\n").split("\t")
                # Only a step that finished counts. A row can also record that
                # the step failed its build or, in older runs, changed nothing
                # without a review. Treating those as done would retire an item
                # unexamined. New no-change reviews require a validated report.
                # A followup completes the naming visit while its structured
                # report retains separate rematching/runtime/semantic work.
                if len(fields) >= 4 and fields[3] in {"ok", "followup"}:
                    names.update(fields[1].split())
    return names


def _former_names(root: str) -> dict:
    """Current spelling -> every spelling it has had, from the renamer's log.

    This is an alias map, not a record of work: it exists so that looking a node
    up by the name it carries today still finds the ledger row filed under the
    name it carried when the step ran.
    """
    direct = {}
    path = os.path.join(root, "local/renames.tsv")
    if os.path.exists(path):
        with open(path) as fh:
            for i, line in enumerate(fh):
                f = line.rstrip("\n").split("\t")
                if i == 0 or len(f) < 4 or not f[2] or not f[3]:
                    continue
                direct.setdefault(f[3], set()).add(f[2])
    out = {}
    for new_name in direct:
        seen, stack = set(), list(direct[new_name])
        while stack:
            n = stack.pop()
            if n in seen:
                continue
            seen.add(n)
            stack.extend(direct.get(n, ()))
        out[new_name] = seen
    return out

def _node_kind(usr: str) -> str:
    """Kind of a node from its libclang USR.

    The tail decides, not any marker along the way: a constant of an enum
    declared inside a function carries the function's `@F@` too, and was once
    taken for a function. An enum constant is the component after an enum's
    (`@E@Tag@CONST`, or `@Ea@`/`@EA@` for an anonymous one); a function's USR
    ends at its own `@F@name`.
    """
    if usr.startswith("macro:"):
        return "macro"
    if re.search(r"@E[aA]?@[^@]*@[^@]+$", usr):
        return "enum"
    if re.search(r"@F@[^@]+$", usr):
        return "func"
    if any(t in usr for t in ("@S@", "@SA@", "@U@", "@UA@", "@E@", "@EA@", "@T@")):
        return "type"
    return "data"


_DOC_CACHE = {}


def _has_doc(root: str, meta: dict) -> bool:
    """Is there a /// comment immediately above the declaration?"""
    path, line = meta.get("file"), meta.get("line")
    if not path or not line:
        return False
    lines = _DOC_CACHE.get(path)
    if lines is None:
        try:
            lines = open(os.path.join(root, path), errors="replace").read().splitlines()
        except OSError:
            lines = []
        _DOC_CACHE[path] = lines
    # The cached graph records the line an item was declared on when it was
    # built; naming work shortens files, so that line can now be past the end.
    i = min(line - 2, len(lines) - 1)
    while i >= 0 and not lines[i].strip():
        i -= 1
    return i >= 0 and lines[i].lstrip().startswith("///")


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


# The initializer an asset's C arrays take from the extracted package. The name
# is generated from the manifest object - package, kind and offset, then the
# part - so it identifies which asset a definition belongs to without reading
# anything into the symbol's name. A collision patch has no part suffix and is
# not matched: it has no record, and code applies each one on its own.
# The asset is everything before the part: a model named in the asset manifest
# has no package or offset in its name (`glutton_leg_left_skeleton.inc`), and a
# stream shared by meshes with different vertices carries a version after its
# offset (`mappic_s2_02_model_00C48_00070_verts.inc`). Cutting the name at the
# kind and offset put every version of such a stream - 315 arrays of the map
# pictures - into one step.
ASSET_INCLUDE = __import__("re").compile(
    r'#include "assets/(\w+)_(?:skeleton|partVerts|verts|normals|stream|bank\d+|records|indices'
    r'|table|faces|cells|vertices|pose|packets)\.inc"')


def asset_groups(root, nodes, edges):
    """Each embedded asset's record and part arrays, as one set per asset.

    A model's skeleton, vertices, normals and packet stream, an animation set's
    banks, records and indices, a collision grid's arrays: nothing reaches them
    except through the asset's record (`TmdSource`, `AnimationSet`, the grid's
    `WorldCollisionGrid`), apart from the few functions that edit a live grid in
    place. Naming them is one decision - what the asset is - so they are one
    unit of work with their record, as a cycle is. The record is the data item
    whose references into asset parts all go to a single asset.
    """
    lines, key_of = {}, {}
    for usr, meta in nodes.items():
        if usr.startswith("macro:") or _node_kind(usr) != "data":
            continue
        where, start, end = meta.get("file"), meta.get("start_line"), meta.get("end_line")
        if not where or not start:
            continue
        if where not in lines:
            try:
                with open(os.path.join(root, where)) as fh:
                    lines[where] = fh.read().split("\n")
            except OSError:
                lines[where] = []
        m = ASSET_INCLUDE.search("\n".join(lines[where][start - 1:end or start]))
        if m:
            key_of[usr] = m.groups()
    groups = collections.defaultdict(set)
    for usr, key in key_of.items():
        groups[key].add(usr)
    for usr, deps in edges.items():
        if usr in key_of or usr not in nodes or _node_kind(usr) != "data":
            continue
        keys = {key_of[d] for d in deps if d in key_of}
        if len(keys) == 1:
            groups[keys.pop()].add(usr)
    return list(groups.values())


def break_table_cycles(nodes, edges):
    """Drop a table's wait for its functions where that wait closes a cycle.

    A dispatch table uses the functions it points at, and the code that spawns
    or dispatches through the table uses the table. When one of those functions
    in turn reaches the dispatcher - an effect callback that spawns another
    effect - the three form a cycle, and through a table of several hundred
    callbacks the cycle swallows everything they touch: gameplay's task
    descriptor table alone tied 1373 functions, most of them room code, into a
    single step no one could review.

    The edge to give up is the table's. Describing a table does not need the
    meaning of every entry, a rename of an entry rewrites the table anyway, and
    each callback is still reviewed with everything it calls in front of it.
    Only edges inside a cycle go: a state table that is in no cycle still waits
    for its handlers, where naming them first is what makes the table readable.
    """
    kind = {u: _node_kind(u) for u in nodes}
    out = {u: list(ds) for u, ds in edges.items()}
    for _ in range(8):
        comp = components(nodes, out)
        cut = 0
        for u, ds in out.items():
            if kind.get(u) != "data":
                continue
            g = comp.get(u, (u,))
            if len(g) < 2:
                continue
            members = set(g)
            keep = [d for d in ds if not (kind.get(d) == "func" and d in members)]
            cut += len(ds) - len(keep)
            out[u] = keep
        if not cut:
            break
    return out


def free_types(nodes, edges):
    """A type waits for types, enums and the macros its definition is built from.

    It cannot use a function or an object, so an edge that says it does is an
    artefact - and the usual one comes through macros. A file-level `#if` on a
    macro may select whole declarations, so `macro_refs.add_graph` makes every
    declaration of the file and of its carriers depend on each macro that could
    bind there; a variant macro such as ROOM_EVENT_ACTIVE names an object, and
    so every type of every room carrying it waited for room data, and behind
    that for whatever was still in assembly. A macro that reaches a function or
    an object, directly or through other macros, is not one a type's layout is
    made of, and the type does not wait for it.
    """
    kind = {u: _node_kind(u) for u in nodes}
    reaches = {}

    def code_macro(m):
        if m in reaches:
            return reaches[m]
        seen, todo, hit = {m}, [m], False
        while todo and not hit:
            for d in edges.get(todo.pop(), ()):
                k = kind.get(d)
                if k in ("func", "data"):
                    hit = True
                    break
                if k == "macro" and d not in seen:
                    seen.add(d)
                    todo.append(d)
        reaches[m] = hit
        return hit

    out = {}
    for u, ds in edges.items():
        if kind.get(u) != "type":
            out[u] = ds
            continue
        out[u] = [d for d in ds
                  if kind.get(d) not in ("func", "data") and not (kind.get(d) == "macro" and code_macro(d))]
    return out


def merge_groups(comp, sets):
    """Make each set one component, together with anything already cycled to it."""
    for members in sets:
        union = set()
        for usr in members:
            union.update(comp.get(usr, (usr,)))
        g = tuple(sorted(union))
        for usr in g:
            comp[usr] = g


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
    # A large cycle is one review. Inspect it once, not once per member, and
    # use set membership for its internal edges.
    groups = {comp.get(usr, (usr,)) for usr in cands if usr not in done}
    for group in groups:
        members = set(group)
        deps = set()
        for m in group:
            deps |= {w for w in edges.get(m, ()) if w in nodes and w not in members}
        if deps <= done:
            out.append(group)
    return out


# --------------------------------------------------------------------------
# the ordered worklist
# --------------------------------------------------------------------------


def asm_used(root: str, version: str, names) -> set:
    """Names the generated assembly reaches, ignoring their own definitions.

    A symbol with no C caller outside its file may still be reached from a
    dispatch table or a `jal` in an unmatched body, so visibility cannot be
    decided from C alone.
    """
    import subprocess
    import tempfile
    if not names:
        return set()
    with tempfile.NamedTemporaryFile("w", delete=False) as fh:
        fh.write("\n".join(sorted(names)))
        listing = fh.name
    try:
        out = subprocess.run(["grep", "-rhFf", listing, "--include=*.s",
                              f"asm/{version}"],
                             cwd=root, capture_output=True, text=True, timeout=900).stdout
    except Exception:
        return set(names)
    finally:
        os.unlink(listing)
    want, used = set(names), set()
    defre = __import__("re").compile(r"^\s*(glabel|dlabel|endlabel|nonmatching|jlabel)\s+(\S+)")
    for line in out.splitlines():
        if defre.match(line):
            continue
        for tok in __import__("re").findall(r"[A-Za-z_]\w*", line):
            if tok in want:
                used.add(tok)
    return used


def _impact(groups, deps, users, order_hint):
    """How many components transitively wait on each one.

    Any topological order is correct, but not all are useful: ordering ready
    items alphabetically buries the few things almost everything needs. Sorting
    by impact front-loads those, so the work that unblocks the most happens
    first. Reachability is accumulated as bitsets, which Python's integers make
    cheap to union.
    """
    bit = {g: 1 << i for i, g in enumerate(groups)}
    reach = {}
    for g in reversed(order_hint):
        acc = bit[g]
        for u in users.get(g, ()):
            acc |= reach.get(u, 0)
        reach[g] = acc
    return {g: bin(v).count("1") for g, v in reach.items()}


def topo_order(nodes, edges, comp, vendor=frozenset()):
    """Components in dependency order: everything a component uses comes first.

    Any topological order is correct, so the freedom is in which of the
    currently-ready components to emit next. Two things decide it: a component
    still in assembly is a barrier the pass stops at, so it goes last among its
    equals - meeting it early would strand work that was ready anyway - and
    otherwise the most depended-upon goes first.
    """
    groups = {}
    for usr in nodes:
        groups.setdefault(comp.get(usr, (usr,)), None)
    groups = list(groups)
    gid = {g: i for i, g in enumerate(groups)}
    out_deg = {g: set() for g in groups}
    for usr in nodes:
        g = comp.get(usr, (usr,))
        for d in edges.get(usr, ()):
            if d in nodes:
                h = comp.get(d, (d,))
                if h is not g:
                    out_deg[g].add(h)
    indeg = collections.Counter()
    users = collections.defaultdict(set)
    for g, deps in out_deg.items():
        indeg[g] = len(deps)
        for d in deps:
            users[d].add(g)
    # A first pass in any order, only to give the impact DP a topological
    # sequence to accumulate along.
    plain, deg = [], dict(indeg)
    q = [g for g in groups if deg[g] == 0]
    while q:
        g = q.pop()
        plain.append(g)
        for u in users.get(g, ()):
            deg[u] -= 1
            if deg[u] == 0:
                q.append(u)
    plain_seen = set(plain)
    plain.extend(g for g in groups if g not in plain_seen)
    impact = _impact(groups, out_deg, users, plain)

    import heapq
    asm_names = assembly_names(cref.repo_root())

    def barrier(g):
        return int(any(not nodes[m].get("file")
                       and nodes[m]["name"] in asm_names
                       and name_index.classify(nodes[m]["name"], _node_kind(m),
                                               vendor) == "generated"
                       for m in g))

    key = lambda g: (barrier(g), -impact.get(g, 0),
                     min(nodes[m]["name"] for m in g))
    heap = [(key(g), gid[g]) for g in groups if indeg[g] == 0]
    heapq.heapify(heap)
    order, seen = [], set()
    while heap:
        _, i = heapq.heappop(heap)
        g = groups[i]
        if g in seen:
            continue
        seen.add(g)
        order.append(g)
        for u in users.get(g, ()):
            indeg[u] -= 1
            if indeg[u] == 0:
                heapq.heappush(heap, (key(u), gid[u]))
    # Anything left sits in a cycle the condensation failed to break; emit it
    # rather than dropping it silently.
    order.extend(g for g in groups if g not in seen)
    return order


_LIBS = None


def _unit_of(root: str, where: str) -> str:
    """What a step may hold several items of: a file, or a shared library.

    The fragments under src/shared are one function to a file, so batching them
    by file batches nothing. They belong to libraries - `mad_chaser.h` with its
    `mad_chaser_*.inc.c` - and a library is one piece of behaviour cut into
    files, so its fragments are one unit, as a source file's functions are.
    """
    global _LIBS
    if not where.startswith("src/shared/"):
        return where
    if _LIBS is None:
        try:
            _LIBS = sorted((f[:-2] for f in os.listdir(os.path.join(root, "src/shared")) if f.endswith(".h")),
                           key=len, reverse=True)
        except OSError:
            _LIBS = []
    stem = os.path.basename(where).split(".", 1)[0]
    for lib in _LIBS:
        if stem == lib or stem.startswith(lib + "_"):
            return "src/shared/" + lib + ".*"
    return where


def batch_ready(root: str, order, nodes, edges, comp, done, limits: dict):
    """Join pending items of one file or library into steps, by kind.

    Two steps that declare items in the same file never run in the same round,
    so a header with forty pending types is forty rounds long however many
    workers there are; and the reasoning for one item of a file - what the
    actor does, what its work block holds - is most of the reasoning for the
    next. `limits` gives the most items a step may hold for each kind batched.

    Steps are built from what is ready. The best-ranked ready item opens a
    step, and ready items of the same unit join it up to the limit - including
    those that become ready only because of what the step already holds, so a
    function and the callers that were waiting for it are reviewed together.
    Everything a step uses outside itself is therefore placed before it, and
    the result is still a dependency order. An item already processed imposes
    no wait, as in `_step_numbers`. `order` supplies the ranking: where there
    is a choice, the earlier item of the plain order goes first.
    """
    if not any(n > 1 for n in limits.values()):
        return order
    import heapq
    rank = {g: i for i, g in enumerate(order)}
    pending = [g for g in order if any(u not in done for u in g)]
    pset = set(pending)
    waits = {g: set() for g in pending}
    users = collections.defaultdict(set)
    for g in pending:
        for u in g:
            for d in edges.get(u, ()):
                if d in nodes:
                    h = comp.get(d, (d,))
                    if h is not g and h in pset:
                        waits[g].add(h)
        for h in waits[g]:
            users[h].add(g)
    left = {g: len(waits[g]) for g in pending}

    def unit(g):
        kinds = {_node_kind(u) for u in g}
        files = {nodes[u].get("file") or "" for u in g}
        if len(kinds) != 1 or len(files) != 1 or "" in files:
            return None
        kind = next(iter(kinds))
        if limits.get(kind, 1) <= 1:
            return None
        return kind, _unit_of(root, next(iter(files)))

    unit_of = {g: unit(g) for g in pending}
    heap = [(rank[g], g) for g in pending if left[g] == 0]
    heapq.heapify(heap)
    ready = collections.defaultdict(list)          # unit -> heap of (rank, group)
    for r, g in heap:
        if unit_of[g]:
            heapq.heappush(ready[unit_of[g]], (r, g))
    placed, steps = set(), []

    def release(g):
        placed.add(g)
        for u in users.get(g, ()):
            left[u] -= 1
            if left[u] == 0:
                heapq.heappush(heap, (rank[u], u))
                if unit_of[u]:
                    heapq.heappush(ready[unit_of[u]], (rank[u], u))

    while heap:
        _, g = heapq.heappop(heap)
        if g in placed:
            continue
        key = unit_of[g]
        if key is None:
            steps.append(g)
            release(g)
            continue
        limit = limits[key[0]]
        members = list(g)
        release(g)
        queue = ready[key]
        while queue:
            _, h = queue[0]
            if h in placed:
                heapq.heappop(queue)
                continue
            if len(members) + len(h) > limit:
                break
            heapq.heappop(queue)
            members.extend(h)
            release(h)              # may make this unit's next items ready
        merged = tuple(sorted(members))
        for u in merged:
            comp[u] = merged
        steps.append(merged)
    # Anything unplaced sits in a cycle the condensation did not break; keep it,
    # in its old order, rather than dropping it.
    steps.extend(g for g in pending if g not in placed)
    return [g for g in order if g not in pset] + steps


RUN_KINDS = os.path.join("local", "name_pass_kinds")


def run_kinds(root):
    """The kinds the running pass is restricted to, or None for all of them.

    local/name_pass_kinds holds one line, `<comma-separated kinds> <driver pid>`,
    written by name_pass.sh when it is given --kinds. The pid is what keeps a
    file left by a run that died from loosening the waits of the next,
    unrestricted one: a restriction counts only while its driver is alive.
    """
    try:
        kinds, pid = open(os.path.join(root, RUN_KINDS)).read().split()
        os.kill(int(pid), 0)
    except (OSError, ValueError):
        return None
    return {k for k in kinds.split(",") if k} or None


def _step_numbers(order, nodes, edges, comp, done, kinds=None):
    """Step number per component, and the last step each one waits on.

    The worklist is a total order, but most of it is free: a step is only
    genuinely after another when it depends on it. Recording, for each step, the
    highest step number among the items it uses turns that freedom into
    something a driver can read - a set of steps may be worked at the same time
    when none of them waits on a step in the set. Because dependencies always
    precede their users here, the maximum over the direct dependencies is also
    the maximum over the transitive ones, so one pass over the edges is enough.

    A dependency that has already been processed carries no step number and
    imposes no wait, which is what lets the front of the list widen as the pass
    advances.
    """
    step_of, idx = {}, 0
    for g in order:
        if any(u not in done for u in g):
            idx += 1
            step_of[g] = idx
    # A run restricted to some kinds never works the others, so a pending item
    # of another kind imposes no wait of its own: it passes on the waits it has.
    # Without this every function behind a pending enum or data item waits for
    # something the run will not do, and the round is one step wide.
    def waited(h):
        return kinds is None or any(_node_kind(u) in kinds for u in h if u not in done)
    after = {}
    for g in step_of:
        last = 0
        for u in g:
            for d in edges.get(u, ()):
                if d not in nodes:
                    continue
                h = comp.get(d, (d,))
                if h is g or h not in step_of:
                    continue
                last = max(last, step_of[h] if waited(h) else after.get(h, 0))
        after[g] = last
    return step_of, after


def _declared_in(root: str, name: str) -> str:
    try:
        idx = cref._ref_index(root, 8)
        files = sorted(idx.decl_files(name), key=lambda f: (not f.startswith("include"), f)) if idx else []
        return files[0] if files else ""
    except Exception:
        return ""


def worklist(root: str, version: str, nodes, edges, comp, done, out_path: str, limits: dict | None = None):
    asm_names = assembly_names(root, version)
    vendor = name_index.vendored_names(root)
    order = topo_order(nodes, edges, comp, vendor)
    order = batch_ready(root, order, nodes, edges, comp, done, limits or {})

    # who refers to each item, for the visibility guess
    referrers = collections.defaultdict(set)
    for usr, deps in edges.items():
        for d in deps:
            if d in nodes:
                referrers[d].add(usr)

    todo_names = {nodes[u]["name"] for g in order for u in g
                  if u not in done and nodes[u].get("file") and _node_kind(u) != "macro"}
    used_in_asm = asm_used(root, version, todo_names)

    step_of, after = _step_numbers(order, nodes, edges, comp, done, run_kinds(root))

    rows, idx = [], 0
    for g in order:
        pending = [u for u in g if u not in done]
        if not pending:
            continue
        idx += 1
        # The wait column is only meaningful against the numbers written here,
        # so the two countings of the same sequence must not drift apart.
        assert step_of[g] == idx, "step numbering disagrees with the wait map"
        for usr in sorted(pending, key=lambda u: nodes[u]["name"]):
            meta = nodes[usr]
            name, where = meta["name"], meta.get("file", "")
            kind = _node_kind(usr)
            state = name_index.classify(name, kind, vendor)
            # classify() reads the spelling, which cannot tell a placeholder
            # that is still assembly from one whose body has been decompiled
            # and simply never named. Only the second is nameable, and it is
            # the larger group by far, so say which is which: "generated" keeps
            # its form until the function is matched, "unnamed" is ordinary
            # work whose evidence is sitting in the C body.
            if state == "generated" and where:
                state = "unnamed"
            elif state == "generated" and name not in asm_names:
                # Declared and used, defined by no image: named from its uses,
                # and found through the file that declares it.
                state = "unnamed"
                where = _declared_in(root, name)
            refs = referrers.get(usr, set())
            outside = {r for r in refs if nodes.get(r, {}).get("file") != where}
            # Visibility is decided against the file that *defines* the item.
            # Where that is unknown the comparison is meaningless - every
            # referrer differs from "" - and the old rule silently called such
            # an item public, which covered every data symbol still living in
            # assembly. Say so instead of guessing.
            homes = {nodes.get(r, {}).get("file") for r in refs}
            homes.discard(None)
            if kind == "macro":
                vis = "configuration" if meta.get("configuration") else "preprocessor"
            elif name in used_in_asm:
                vis = "public (asm)"
            elif not where:
                vis = "unknown (no C definition)"
            elif kind == "type":
                # A type is declared in a header but owned by whatever uses it:
                # if that is a single translation unit, the type belongs there.
                vis = "private" if len(homes) == 1 else "public"
            elif outside:
                vis = "public"
            else:
                vis = "private"
            rows.append((str(idx), str(len(pending)), name, kind, vis, state,
                         where, str(len(refs)), str(after.get(g, 0))))
    with open(out_path, "w") as fh:
        fh.write("order\tgroup_size\tname\tkind\tvisibility\tstate\tfile"
                 "\treferrers\tafter\n")
        for r in rows:
            fh.write("\t".join(r) + "\n")
    return rows


def footprints(nodes, edges, worklist_path: str, orders):
    """For each step: the files declaring its items, and every file it reaches.

    A step reaches its declaring files and every file holding one of its
    referrers, since a rename or a change of shape is carried to all of them.
    An item the graph does not know reaches only its declaring file.
    """
    want = set(orders)
    by_key = collections.defaultdict(list)
    for usr, meta in nodes.items():
        by_key[(meta["name"], meta.get("file") or "")].append(usr)
    decl = {o: set() for o in orders}
    reach = {o: set() for o in orders}
    step_of = {}
    with open(worklist_path) as fh:
        next(fh, None)
        for line in fh:
            f = line.rstrip("\n").split("\t")
            if len(f) < 7 or f[0] not in want:
                continue
            name, where = f[2], ("" if f[6] == "-" else f[6])
            if where:
                decl[f[0]].add(where)
                reach[f[0]].add(where)
            for usr in by_key.get((name, where), ()):
                step_of[usr] = f[0]
    for usr, deps in edges.items():
        where = nodes.get(usr, {}).get("file")
        if not where:
            continue
        for d in deps:
            o = step_of.get(d)
            if o:
                reach[o].add(where)
    return decl, reach


def pick_round(orders, decl, reach, workers: int):
    """Greedily keep steps, in worklist order, that can be worked side by side.

    The worklist's dependency test says two steps do not wait on each other; it
    does not say their edits stay apart. Steps declared in the same file edit
    one declaration block, and steps whose uses fall mostly in the same files
    are reviewed through the same consumers - both are worked one after the
    other instead. "Mostly" is half the files of the smaller step, so a small
    item whose few uses sit inside a wide item's reach waits for it.
    """
    chosen, skipped = [], []
    for o in orders:
        if len(chosen) >= workers:
            break
        clash = None
        for c in chosen:
            shared = reach[o] & reach[c]
            if decl[o] & decl[c]:
                clash = (c, f"both declared in {sorted(decl[o] & decl[c])[0]}")
            elif shared and 2 * len(shared) >= min(len(reach[o]), len(reach[c])):
                clash = (c, f"{len(shared)} shared file(s)")
            if clash:
                break
        if clash:
            skipped.append((o, *clash))
        else:
            chosen.append(o)
    return chosen, skipped


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="?", default="stats",
                    choices=["ready", "next", "stats", "worklist", "round"])
    ap.add_argument("spec", nargs="?")
    ap.add_argument("--build", action="store_true", help="rebuild the cached graph")
    ap.add_argument("--graph", default=None)
    ap.add_argument("--version", default=cref.DEFAULT_VERSION)
    ap.add_argument("-j", "--jobs", type=int, default=int(os.environ.get("PE2_JOBS") or 0) or os.cpu_count() or 8)
    ap.add_argument("-n", "--limit", type=int, default=10)
    ap.add_argument("--worklist", default=os.path.join("local", "worklist.tsv"))
    ap.add_argument("--workers", type=int, default=1)
    ap.add_argument("--batch", type=int, default=int(os.environ.get("PE2_NAME_BATCH") or 8),
                    help="worklist: join up to N pending types declared in one file into a step "
                         "(default 8, or PE2_NAME_BATCH; 1 keeps one type per step)")
    ap.add_argument("--batch-funcs", type=int, default=int(os.environ.get("PE2_NAME_BATCH_FUNCS") or 16),
                    help="worklist: join up to N pending functions of one file, or of one shared "
                         "library's fragments, into a step (default 16, or PE2_NAME_BATCH_FUNCS)")
    args = ap.parse_args()

    root = cref.repo_root()
    path = args.graph or os.path.join(root, DEFAULT_GRAPH)
    if not os.path.isabs(path):
        path = os.path.join(root, path)

    if args.build:
        build(root, args.version, args.jobs, path)
        return 0

    nodes, edges, alias = load(path)

    if args.command == "round":
        orders = [l.strip() for l in sys.stdin if l.strip()]
        wl = args.worklist if os.path.isabs(args.worklist) else os.path.join(root, args.worklist)
        decl, reach = footprints(nodes, edges, wl, orders)
        chosen, skipped = pick_round(orders, decl, reach, args.workers)
        for o, c, why in skipped:
            print(f"    step {o} waits: overlaps step {c} ({why})", file=sys.stderr)
        print("\n".join(chosen))
        return 0

    done = processed_set(root, nodes)
    edges = break_table_cycles(nodes, free_types(nodes, edges))
    comp = components(nodes, edges)
    merge_groups(comp, asset_groups(root, nodes, edges))

    if args.command == "worklist":
        out = args.worklist if os.path.isabs(args.worklist) else os.path.join(root, args.worklist)
        rows = worklist(root, args.version, nodes, edges, comp, done, out,
                        {"type": args.batch, "func": args.batch_funcs})
        groups = len({r[0] for r in rows})
        multi = len({r[0] for r in rows if int(r[1]) > 1})
        print(f"{len(rows)} items in {groups} ordered steps -> {os.path.relpath(out, root)}")
        print(f"  steps needing more than one item at once: {multi}")
        vis = collections.Counter(r[4] for r in rows)
        for k, n in vis.most_common():
            print(f"  {k:<14} {n}")
        return 0

    if args.command == "stats":
        cyc = {g for g in comp.values() if len(g) > 1}
        print(f"nodes {len(nodes)}   processed {len(done)}   remaining {len(nodes) - len(done)}")
        print(f"cycles (components > 1): {len(cyc)}, largest {max((len(g) for g in cyc), default=0)}")
        print(f"ready now: {len(leaves(nodes, edges, nodes, done, comp))}")
        return 0

    target = None
    if args.spec:
        if macro_refs.definition_exists(root, args.spec):
            usr = "macro:" + args.spec
        else:
            spec = cref.parse_spec(args.spec)
            usrs, _names, kind, where = cref.resolve(spec, root, cref.load_db(root, args.version))
            usr = sorted(usrs)[0]
        target = alias.get(usr, usr)
        if target not in nodes:
            sys.exit(f"{args.spec} is not in the graph; rebuild after it was added (SDK/plumbing macros are excluded)")
        usr = target

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
