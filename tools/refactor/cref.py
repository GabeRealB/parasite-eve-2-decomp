"""Shared C cross-reference engine for the refactor CLIs.

Everything here goes through libclang, never a regex. A reference is matched by
the USR (Unified Symbol Resolution) of the declaration it resolves to, which is
stable across translation units, so identity rather than spelling decides what
matches.

That distinction is the whole point. Where many unrelated types declare a member
of the same name, text matching cannot tell one from another. Neither can
scoping a search to a file, or even to a function: one function may declare the
same identifier in several nested blocks with different types, so only the
resolved declaration is a reliable answer.

Symbol specs are a source path with the symbol appended:

    <header>/<Type>::<member>        field of a struct or union
    <header>/<Type>                  a type
    <source>/<name>                  a function, global or enum constant
    <source>/<function>::<param>     a function parameter

The type part accepts either the typedef name or the underlying tag name, so
both spellings resolve to the same declaration. The path may be omitted, in
which case the first matching declaration found is used.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field as dc_field
from multiprocessing import Pool

import clang.cindex as ci

# The compile database targets an older, pre-C99 compiler and carries a few
# flags libclang does not accept. -ferror-limit=0 keeps the AST complete: some
# translation units use constructs that later standards reject, which stops
# neither parsing nor symbol resolution.
_DROP_FLAGS = {"-lang-c", "-undef", "-Wall", "-Werror", "-c", "-E", "-P"}

# Trees are laid out per game version (asm/<version>, configs/<version>, ...).
# Only one version exists today, so it is the default, but nothing here assumes
# it is the only one.
DEFAULT_VERSION = "USA"
_EXTRA_FLAGS = ["-target", "mipsel-unknown-elf", "-std=gnu89", "-ferror-limit=0"]


def repo_root() -> str:
    """Repository root, two levels above this package."""
    here = os.path.dirname(os.path.abspath(__file__))
    return os.path.dirname(os.path.dirname(here))


def iter_versions(root: str) -> list[str]:
    """Version directories present in the tree."""
    cfg = os.path.join(root, "configs")
    if not os.path.isdir(cfg):
        return []
    return sorted(d for d in os.listdir(cfg) if os.path.isdir(os.path.join(cfg, d)))


def load_db(root: str, version: str = DEFAULT_VERSION) -> dict[str, list[str]]:
    """file -> clang argv, from compile_commands.json, for one version.

    Entries are keyed by source path, and a source belongs to every version that
    builds it. Without filtering, a tree configured for more than one version
    would silently keep whichever entry was read last and resolve symbols
    against the wrong flags, so the version's own define selects them.
    """
    path = os.path.join(root, "compile_commands.json")
    if not os.path.exists(path):
        sys.exit(
            "compile_commands.json is missing; run ninja_config.py in this tree first"
        )
    marker = f"-DVER_{version}"
    entries = json.load(open(path))
    tagged = [e for e in entries if "-DVER_" in e["command"]]
    if tagged:
        chosen = [e for e in tagged if marker in e["command"].split()]
        if not chosen:
            seen = sorted({t[len("-DVER_"):] for e in tagged
                           for t in e["command"].split() if t.startswith("-DVER_")})
            sys.exit(f"no entries for version {version!r}; "
                     f"this tree is configured for: {', '.join(seen) or 'none'}")
        entries = chosen
    db: dict[str, list[str]] = {}
    for entry in entries:
        args: list[str] = []
        skip = False
        want_inc = False
        for tok in entry["command"].split()[1:]:
            if skip:
                skip = False
                continue
            if tok == "-o":
                skip = True
                continue
            if tok in _DROP_FLAGS or tok.endswith(".c"):
                continue
            # Include paths in the compile db are relative to the repo, and
            # clang resolves them against the *process* cwd. Absolutising them
            # here lets the tools run from any directory.
            if want_inc:
                args.append(_abs_inc(tok, root))
                want_inc = False
                continue
            if tok == "-I":
                args.append(tok)
                want_inc = True
                continue
            if tok.startswith("-I"):
                args.append("-I" + _abs_inc(tok[2:], root))
                continue
            args.append(tok)
        db[entry["file"]] = args + _EXTRA_FLAGS
    return db


def _abs_inc(path: str, root: str) -> str:
    return path if os.path.isabs(path) else os.path.normpath(os.path.join(root, path))


# --------------------------------------------------------------------------
# spec parsing / resolution
# --------------------------------------------------------------------------


@dataclass
class Spec:
    path: str | None
    owner: str | None  # type name for a field, else None
    name: str

    @property
    def token(self) -> str:
        """The identifier as it appears in source; the rename target."""
        return self.name


def parse_spec(text: str) -> Spec:
    owner = None
    name = text
    if "::" in text:
        head, name = text.rsplit("::", 1)
    else:
        head = None
    path = None
    if head is not None:
        # head is "<maybe/path>/<Owner>" or just "<Owner>"
        if "/" in head:
            path, owner = head.rsplit("/", 1)
        else:
            owner = head
    elif "/" in text:
        path, name = text.rsplit("/", 1)
    if not re.fullmatch(r"[A-Za-z_]\w*", name):
        sys.exit(f"not a C identifier: {name!r}")
    if owner is not None and not re.fullmatch(r"[A-Za-z_]\w*", owner):
        sys.exit(f"not a C identifier: {owner!r}")
    return Spec(path=path, owner=owner, name=name)


def _record_of(cur: ci.Cursor) -> ci.Cursor | None:
    """The struct/union decl behind a typedef or tag cursor."""
    if cur.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL):
        return cur.get_definition() or cur
    if cur.kind == ci.CursorKind.TYPEDEF_DECL:
        decl = cur.underlying_typedef_type.get_declaration()
        if decl and decl.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL):
            return decl.get_definition() or decl
    return None


def _names_for(owner: str) -> set[str]:
    """Accept the typedef name, the tag name, and the _Tag convention."""
    return {owner, owner.lstrip("_"), "_" + owner.lstrip("_")}


def resolve(spec: Spec, root: str, db: dict[str, list[str]], verbose=False):
    """Return (usr, kind_name, declaration location) for the spec."""
    for tu_file in _candidate_tus(spec, root, db):
        tu = parse_tu(tu_file, db.get(tu_file), root)
        if tu is None:
            continue
        hit = _find_decl(tu, spec, root)
        if hit:
            return hit
        if spec.owner:
            hit = _find_param(tu, spec, root)
            if hit:
                return hit
    hint = ""
    if _is_asm_stub(root, spec.name):
        hint = (f"\n{spec.name} appears only in an INCLUDE_ASM stub, so it has no C\n"
                "declaration to resolve. Renaming a symbol that is still assembly is a\n"
                "different operation: change it in the symbol map, update the\n"
                "INCLUDE_ASM argument, and re-split so the generated file is renamed.")
    sys.exit(f"could not resolve {spec.name!r}"
             + (f" in {spec.path}" if spec.path else "") + hint)


def _is_asm_stub(root: str, name: str) -> bool:
    try:
        out = subprocess.run(["grep", "-rlF", f", {name})", "src"],
                             cwd=root, capture_output=True, text=True, timeout=120).stdout
        return bool(out.strip())
    except Exception:
        return False


def _candidate_tus(spec: Spec, root: str, db) -> list[str]:
    """TUs likely to contain the declaration, cheapest first."""
    if spec.path and spec.path.endswith(".c"):
        rel = os.path.relpath(os.path.join(root, spec.path), root)
        return [rel] + [f for f in db if f != rel]
    # A header: prefer TUs that mention the owner/name textually.
    needle = spec.owner or spec.name
    hits = _grep_files(root, needle, list(db))
    return hits + [f for f in db if f not in set(hits)]


def _find_param(tu, spec: Spec, root: str):
    """A function parameter, spelled `<file>/<function>::<param>`.

    Parameter USRs embed the file and byte offset of their own declaration, so
    the same parameter has a different USR in a prototype and in the
    definition. Only the enclosing function's USR is stable, so a parameter is
    identified by its index under that function and matched positionally across
    every declaration of it.
    """
    want_file = abspath(spec.path, root) if spec.path else None
    for cur in tu.cursor.walk_preorder():
        if cur.kind != ci.CursorKind.FUNCTION_DECL or cur.spelling != spec.owner:
            continue
        loc = cur.location
        if want_file and (loc.file is None or abspath(loc.file.name, root) != want_file):
            continue
        args = list(cur.get_arguments())
        for i, a in enumerate(args):
            if a.spelling == spec.name:
                return (f"{cur.get_usr()}#param{i}", "parameter", _loc(a, root))
    return None


def _find_decl(tu, spec: Spec, root: str):
    want_file = abspath(spec.path, root) if spec.path else None
    for cur in tu.cursor.walk_preorder():
        loc = cur.location
        if loc.file is None:
            continue
        if want_file and abspath(loc.file.name, root) != want_file:
            continue
        if spec.owner:
            if cur.kind != ci.CursorKind.FIELD_DECL or cur.spelling != spec.name:
                continue
            rec = cur.semantic_parent
            if rec is None:
                continue
            tags = {rec.spelling} | _typedef_aliases(tu, rec)
            if not (tags & _names_for(spec.owner)):
                continue
            return cur.get_usr(), "field", _loc(cur, root)
        else:
            if cur.spelling != spec.name:
                continue
            if cur.kind in (
                ci.CursorKind.FUNCTION_DECL,
                ci.CursorKind.VAR_DECL,
                ci.CursorKind.TYPEDEF_DECL,
                ci.CursorKind.STRUCT_DECL,
                ci.CursorKind.UNION_DECL,
                ci.CursorKind.ENUM_DECL,
                ci.CursorKind.ENUM_CONSTANT_DECL,
            ):
                if cur.kind == ci.CursorKind.VAR_DECL and cur.semantic_parent.kind != ci.CursorKind.TRANSLATION_UNIT:
                    continue  # a local, not a global
                defn = cur.get_definition() or cur
                return defn.get_usr(), _kind_label(cur.kind), _loc(defn, root)
    return None


_ALIAS_CACHE: dict[int, dict[str, set[str]]] = {}


def _typedef_aliases(tu, rec) -> set[str]:
    """Typedef names aliasing a record, built once per TU rather than per field."""
    key = id(tu)
    table = _ALIAS_CACHE.get(key)
    if table is None:
        table = {}
        for cur in tu.cursor.walk_preorder():
            if cur.kind != ci.CursorKind.TYPEDEF_DECL:
                continue
            r = _record_of(cur)
            if r is not None:
                table.setdefault(r.get_usr(), set()).add(cur.spelling)
        _ALIAS_CACHE[key] = table
    return table.get((rec.get_definition() or rec).get_usr(), set())


def _kind_label(kind) -> str:
    return {
        ci.CursorKind.FUNCTION_DECL: "function",
        ci.CursorKind.VAR_DECL: "global",
        ci.CursorKind.TYPEDEF_DECL: "typedef",
        ci.CursorKind.STRUCT_DECL: "struct",
        ci.CursorKind.UNION_DECL: "union",
        ci.CursorKind.ENUM_DECL: "enum",
        ci.CursorKind.ENUM_CONSTANT_DECL: "enum-constant",
    }.get(kind, str(kind))


def abspath(name: str, root: str) -> str:
    """libclang reports paths as given on the command line, so an include found
    through a relative -I comes back relative to the process cwd, not to root."""
    if not name:
        return ""
    return os.path.normpath(name if os.path.isabs(name) else os.path.join(root, name))


def relpath(name: str, root: str) -> str:
    return os.path.relpath(abspath(name, root), root)


def _loc(cur, root) -> str:
    loc = cur.location
    name = relpath(loc.file.name, root) if loc.file else "?"
    return f"{name}:{loc.line}"


# --------------------------------------------------------------------------
# parsing
# --------------------------------------------------------------------------

_index = None


def parse_tu(rel_file: str, args, root: str):
    global _index
    if args is None:
        return None
    if _index is None:
        _index = ci.Index.create()
    path = os.path.join(root, rel_file)
    if not os.path.exists(path):
        return None
    try:
        # PARSE_DETAILED_PROCESSING_RECORD segfaults this libclang build; we do
        # not need preprocessor records, only the semantic AST.
        return _index.parse(path, args=args)
    except ci.TranslationUnitLoadError:
        return None


def _token_files(root: str, token: str) -> tuple[list[str], list[str]]:
    """(.c files, .h files) that spell the identifier."""
    try:
        out = subprocess.run(
            ["grep", "-rlw", "--include=*.c", "--include=*.h", token, "src", "include"],
            cwd=root, capture_output=True, text=True, timeout=300,
        ).stdout.split()
    except Exception:
        return [], ["*"]
    return [f for f in out if f.endswith(".c")], [f for f in out if f.endswith(".h")]


def _grep_files(root: str, token: str, candidates: list[str]) -> list[str]:
    cs, hs = _token_files(root, token)
    hits = set(cs)
    tus = [f for f in candidates if f in hits]
    if hs:
        return tus + [f for f in candidates if f not in set(tus)]
    return tus


_INC_RE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)


def _include_index(root: str) -> dict[str, set[str]]:
    """header path -> files that #include it directly.

    Project headers are reached through a single include root, so an include
    spelled relative to that root resolves by simple path joining. That is
    enough to walk the graph without invoking the preprocessor, which would
    cost another full parse of every translation unit.
    """
    rev: dict[str, set[str]] = {}
    out = subprocess.run(["grep", "-rl", "#include", "src", "include"],
                         cwd=root, capture_output=True, text=True).stdout.split()
    for f in out:
        try:
            text = open(os.path.join(root, f), errors="replace").read()
        except OSError:
            continue
        here = os.path.dirname(f)
        for inc in _INC_RE.findall(text):
            for cand in (os.path.normpath(os.path.join("include", inc)),
                         os.path.normpath(os.path.join(here, inc))):
                rev.setdefault(cand, set()).add(f)
    return rev


def _tus_reaching(root: str, headers: list[str], candidates: list[str]) -> set[str]:
    """.c files that reach any of `headers` through the include graph."""
    rev = _include_index(root)
    seen: set[str] = set()
    stack = [os.path.normpath(h) for h in headers]
    while stack:
        h = stack.pop()
        for f in rev.get(h, ()):
            if f in seen:
                continue
            seen.add(f)
            if f.endswith(".h"):
                stack.append(os.path.normpath(f))
    cand = set(candidates)
    return {f for f in seen if f in cand}


def includers_of(root: str, header_rel: str) -> dict[str, list[tuple[int, str]]]:
    """file -> [(line, spelling)] for every #include that resolves to header_rel.

    Both spellings are recognised: one relative to the include root, and one
    relative to the including file's own directory.
    """
    target = os.path.normpath(header_rel)
    out: dict[str, list[tuple[int, str]]] = {}
    files = subprocess.run(["grep", "-rl", "#include", "src", "include"],
                           cwd=root, capture_output=True, text=True).stdout.split()
    for f in files:
        try:
            lines = open(os.path.join(root, f), errors="replace").read().splitlines()
        except OSError:
            continue
        here = os.path.dirname(f)
        hits = []
        for i, line in enumerate(lines, 1):
            m = _INC_RE.match(line)
            if not m:
                continue
            inc = m.group(1)
            for cand in (os.path.normpath(os.path.join("include", inc)),
                         os.path.normpath(os.path.join(here, inc))):
                if cand == target:
                    hits.append((i, inc))
                    break
        if hits:
            out[f] = hits
    return out


def new_include_spelling(old_spelling: str, includer: str, old_rel: str,
                         new_rel: str) -> str:
    """Rewrite one #include target, preserving the form the includer used."""
    here = os.path.dirname(includer) or "."
    via_include = os.path.normpath(os.path.join("include", old_spelling))
    if via_include == os.path.normpath(old_rel):
        return os.path.relpath(new_rel, "include").replace(os.sep, "/")
    return os.path.relpath(new_rel, here).replace(os.sep, "/")


def prefilter_tus(root: str, token: str, candidates: list[str], decl_file: str | None) -> list[str]:
    """TUs that can possibly contain a reference.

    A reference spells the identifier in some file of the translation unit, so a
    TU whose sources never contain the token cannot hold one. The declaring
    header always contains it and must not by itself force a full scan -
    otherwise every field rename degrades to parsing the whole tree.
    """
    cs, hs = _token_files(root, token)
    if hs == ["*"]:
        return list(candidates)
    hs = [h for h in hs if decl_file is None or os.path.normpath(h) != os.path.normpath(decl_file)]
    hits = set(cs)
    tus = [f for f in candidates if f in hits]
    if hs:
        # Another header spells it (an inline body, a macro, a struct member
        # declared there). Add exactly the TUs that can reach those headers
        # rather than falling back to the whole tree.
        extra = _tus_reaching(root, hs, candidates)
        hits |= extra
        tus = [f for f in candidates if f in hits]
    return tus


# --------------------------------------------------------------------------
# references in prose
# --------------------------------------------------------------------------


def comment_refs(root: str, token: str, owner: str | None = None) -> list:
    """Whole-word mentions of the identifier inside comments.

    These are not references the compiler knows about, so they never affect
    dependency order - but they are how the codebase talks about a symbol, and a
    rename that leaves them behind turns every one of them into a lie pointing
    at a name that no longer exists.
    """
    try:
        out = subprocess.run(
            ["grep", "-rnw", "--include=*.c", "--include=*.h", token, "src", "include"],
            cwd=root, capture_output=True, text=True, timeout=300,
        ).stdout.splitlines()
    except Exception:
        return []
    # A bare word in prose is not evidence that the symbol is meant: renaming
    # every mention of "work" would rewrite unrelated sentences. Require the
    # mention to be qualified - `Type::field`, `->field`, `.field` - or set in
    # backticks, which is how this codebase cites a symbol.
    t = re.escape(token)
    pats = [rf"`{t}`", rf"->\s*{t}\b", rf"\.{t}\b", rf"\b\w+::{t}\b"]
    if owner:
        pats.append(rf"\b{re.escape(owner)}::{t}\b")
    word = re.compile("|".join(pats))
    refs = []
    for row in out:
        try:
            path, lineno, text = row.split(":", 2)
        except ValueError:
            continue
        stripped = text.lstrip()
        line_comment = text.find("//")
        for m in word.finditer(text):
            # Inside a `///`, `//`, `*` or `/*` line, or after a trailing `//`.
            in_block = stripped.startswith(("///", "//", "*", "/*"))
            in_trailing = line_comment != -1 and m.start() > line_comment
            if in_block or in_trailing:
                # Point at the identifier, not at the qualifier that precedes it.
                col = text.index(token, m.start()) + 1
                refs.append(Ref(path, int(lineno), col, "comment", text.strip(), ""))
                break
    return refs


# --------------------------------------------------------------------------
# reference collection
# --------------------------------------------------------------------------


@dataclass
class Ref:
    file: str
    line: int
    col: int
    use: str
    context: str = ""
    enclosing: str = ""


# Declarations of the symbol itself. A prototype is not a DECL_REF_EXPR, so
# matching references alone would rename a definition and leave its declarations
# behind, which does not compile.
_DECL_KINDS = {
    ci.CursorKind.FUNCTION_DECL,
    ci.CursorKind.VAR_DECL,
    ci.CursorKind.FIELD_DECL,
    ci.CursorKind.TYPEDEF_DECL,
    ci.CursorKind.STRUCT_DECL,
    ci.CursorKind.UNION_DECL,
    ci.CursorKind.ENUM_DECL,
    ci.CursorKind.ENUM_CONSTANT_DECL,
}

_REF_KINDS = {
    ci.CursorKind.DECL_REF_EXPR,
    ci.CursorKind.MEMBER_REF_EXPR,
    ci.CursorKind.MEMBER_REF,
    ci.CursorKind.TYPE_REF,
    ci.CursorKind.CALL_EXPR,
}


def _cast_around(cur, parents) -> bool:
    """Is this reference wrapped in, or assigned from, an explicit cast?

    A field every user has to cast is a field whose declared type is wrong, so
    counting the casts turns that judgement into an observation.
    """
    node, hops = cur, 0
    while node is not None and hops < 4:
        if node.kind == ci.CursorKind.CSTYLE_CAST_EXPR:
            return True
        node = parents.get(node.hash)
        hops += 1
    p = parents.get(cur.hash)
    while p is not None and p.kind in (ci.CursorKind.UNEXPOSED_EXPR,
                                       ci.CursorKind.PAREN_EXPR):
        p = parents.get(p.hash)
    if p is not None and p.kind in (ci.CursorKind.BINARY_OPERATOR,
                                    ci.CursorKind.VAR_DECL):
        for kid in p.get_children():
            stack = [kid]
            depth = 0
            while stack and depth < 40:
                n = stack.pop()
                depth += 1
                if n.kind == ci.CursorKind.CSTYLE_CAST_EXPR:
                    return True
                stack.extend(n.get_children())
    return False


def _usage(cur, parents) -> str:
    """Classify how a reference is used, from the AST shape above it."""
    kind = cur.kind
    if kind == ci.CursorKind.TYPE_REF:
        return "type-use"
    # A direct call is a CALL_EXPR whose own spelling is the callee; only an
    # *indirect* use of a function appears as a bare DECL_REF_EXPR, and that is
    # the shape that means the address was taken (a dispatch-table entry).
    if kind == ci.CursorKind.CALL_EXPR:
        return "call"
    parent = parents.get(_key(cur))
    # function used without being called == address taken
    if kind == ci.CursorKind.DECL_REF_EXPR and cur.referenced is not None:
        if cur.referenced.kind == ci.CursorKind.FUNCTION_DECL:
            p = parent
            while p is not None and p.kind in (ci.CursorKind.UNEXPOSED_EXPR, ci.CursorKind.PAREN_EXPR):
                p = parents.get(_key(p))
            if p is not None and p.kind == ci.CursorKind.CALL_EXPR:
                # the callee child of the call, not an argument
                kids = list(p.get_children())
                if kids and _key(kids[0]) == _key_chain(cur, parents, p):
                    return "call"
                return "address-of (argument)"
            return "address-of"
    # walk up through transparent nodes to find an assignment/unary context
    node = cur
    p = parents.get(_key(node))
    while p is not None and p.kind in (
        ci.CursorKind.UNEXPOSED_EXPR,
        ci.CursorKind.PAREN_EXPR,
        ci.CursorKind.ARRAY_SUBSCRIPT_EXPR,
    ):
        node, p = p, parents.get(_key(p))
    if p is None:
        return "read"
    if p.kind == ci.CursorKind.COMPOUND_ASSIGNMENT_OPERATOR:
        return "read-write" if _is_lhs(p, node) else "read"
    if p.kind == ci.CursorKind.BINARY_OPERATOR:
        if _binop(p) == "=" and _is_lhs(p, node):
            return "write"
        return "read"
    if p.kind == ci.CursorKind.UNARY_OPERATOR:
        op = _unop(p)
        if op == "&":
            return "address-of"
        if op in ("++", "--"):
            return "read-write"
        return "read"
    return "read"


def _key(cur):
    return cur.hash


def _key_chain(cur, parents, stop):
    node = cur
    p = parents.get(_key(node))
    while p is not None and _key(p) != _key(stop):
        node, p = p, parents.get(_key(p))
    return _key(node)


def _is_lhs(binop, child) -> bool:
    kids = list(binop.get_children())
    return bool(kids) and _key(kids[0]) == _key(child)


def _binop(cur) -> str:
    kids = list(cur.get_children())
    if len(kids) < 2:
        return ""
    lhs_end = kids[0].extent.end
    for tok in cur.get_tokens():
        s = tok.extent.start
        if (s.line, s.column) >= (lhs_end.line, lhs_end.column):
            return tok.spelling
    return ""


def _unop(cur) -> str:
    toks = list(cur.get_tokens())
    return toks[0].spelling if toks else ""


def collect_in_tu(job):
    """Scan one TU for references to `usr`.

    Ordering matters for speed: reading a cursor's spelling is one cheap call,
    while resolving its referent and building that referent's USR allocate and
    are far dearer. Testing the spelling first discards almost every cursor in
    the tree, and is the difference between a fast scan and an unusable one.
    """
    rel_file, args, root, usr, token = job
    tu = parse_tu(rel_file, args, root)
    if tu is None:
        return []
    if "#param" in usr:
        return _collect_param(tu, root, usr, token)
    _spellings = {token, f"struct {token}", f"union {token}", f"enum {token}"}
    refs: list[Ref] = []
    parents: dict[int, ci.Cursor] = {}
    src_cache: dict[str, list[str]] = {}
    stack = [tu.cursor]
    while stack:
        cur = stack.pop()
        h = cur.hash
        for kid in cur.get_children():
            parents[kid.hash] = cur
            stack.append(kid)
        # A TYPE_REF to a tagged type spells itself "struct X", not "X", so an
        # exact match finds the declarations and misses every use.
        if cur.spelling not in _spellings:
            continue
        if cur.kind in _REF_KINDS:
            ref = cur.referenced
            if ref is None or ref.get_usr() != usr:
                continue
            decl_here = False
        elif cur.kind in _DECL_KINDS:
            if cur.get_usr() != usr:
                continue
            decl_here = True
        else:
            continue
        loc = cur.location
        if loc.file is None:
            continue
        fname = relpath(loc.file.name, root)
        if fname.startswith(".."):
            continue
        if decl_here:
            use = "definition" if cur.is_definition() else "declaration"
        else:
            use = _usage(cur, parents)
            if _cast_around(cur, parents):
                use = f"{use} (cast)"
        if fname not in src_cache:
            try:
                src_cache[fname] = open(os.path.join(root, fname), errors="replace").read().splitlines()
            except OSError:
                src_cache[fname] = []
        lines = src_cache[fname]
        raw = lines[loc.line - 1] if 0 < loc.line <= len(lines) else ""
        text = raw.strip()
        col = loc.column
        if not raw[col - 1:].startswith(token):
            # A reference to a tagged type is located at the keyword, so step
            # over it to reach the identifier itself.
            for kw in ("struct ", "union ", "enum "):
                if raw[col - 1:].startswith(kw + token):
                    col += len(kw)
                    break
        if not raw[col - 1:].startswith(token):
            # A reference inside a macro body is reported at the *invocation*,
            # where the identifier does not appear. The site is real, but the
            # edit belongs in the macro definition rather than here.
            use = f"{use} (via macro)"
        refs.append(Ref(fname, loc.line, col, use, text, _enclosing(cur, parents)))
    return refs


def _collect_param(tu, root, usr, token):
    """Parameter sites: the PARM_DECL at that index in every declaration of the
    function, plus every use of it inside a body."""
    fn_usr, idx = usr.split("#param")
    idx = int(idx)
    refs: list[Ref] = []
    src_cache: dict[str, list[str]] = {}

    def add(cur, use):
        loc = cur.location
        if loc.file is None:
            return
        fname = relpath(loc.file.name, root)
        if fname.startswith(".."):
            return
        if fname not in src_cache:
            try:
                src_cache[fname] = open(os.path.join(root, fname), errors="replace").read().splitlines()
            except OSError:
                src_cache[fname] = []
        lines = src_cache[fname]
        text = lines[loc.line - 1].strip() if 0 < loc.line <= len(lines) else ""
        refs.append(Ref(fname, loc.line, loc.column, use, text, fn_usr.split("@F@")[-1]))

    for cur in tu.cursor.walk_preorder():
        if cur.kind != ci.CursorKind.FUNCTION_DECL or cur.get_usr() != fn_usr:
            continue
        args = list(cur.get_arguments())
        if idx >= len(args):
            continue
        parm = args[idx]
        if parm.spelling != token:
            continue
        add(parm, "definition" if cur.is_definition() else "declaration")
        parm_usr = parm.get_usr()
        parents: dict[int, ci.Cursor] = {}
        stack = [cur]
        while stack:
            node = stack.pop()
            for kid in node.get_children():
                parents[kid.hash] = node
                stack.append(kid)
            if node.kind != ci.CursorKind.DECL_REF_EXPR or node.spelling != token:
                continue
            r = node.referenced
            if r is not None and r.get_usr() == parm_usr:
                add(node, _usage(node, parents))
    return refs


def _enclosing(cur, parents) -> str:
    p = parents.get(cur.hash)
    while p is not None:
        if p.kind in (ci.CursorKind.FUNCTION_DECL,):
            return p.spelling
        p = parents.get(_key(p))
    return ""


def find_refs(usr: str, token: str, root: str, db, jobs: int = 8, prefilter=True,
              progress=None, decl_file: str | None = None,
              filter_token: str | None = None):
    """filter_token narrows the candidate set by a *different* identifier than
    the one being matched. A parameter name is usually too common to select on,
    while every reference to it lies inside one function, so the function's name
    is the far more selective filter."""
    files = list(db)
    if prefilter:
        files = prefilter_tus(root, filter_token or token, files, decl_file)
    jobs_list = [(f, db[f], root, usr, token) for f in files]
    out: list[Ref] = []
    done = 0
    with Pool(jobs) as pool:
        for refs in pool.imap_unordered(collect_in_tu, jobs_list, chunksize=4):
            out.extend(refs)
            done += 1
            if progress:
                progress(done, len(jobs_list))
    # one physical site can be seen from many TUs (shared headers / shared .c)
    seen = set()
    uniq = []
    for r in sorted(out, key=lambda r: (r.file, r.line, r.col)):
        k = (r.file, r.line, r.col)
        if k in seen:
            continue
        seen.add(k)
        uniq.append(r)
    return uniq, len(files)
