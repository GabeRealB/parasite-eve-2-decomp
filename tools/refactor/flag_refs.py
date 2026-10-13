#!/usr/bin/env python3
"""Where the source reads or writes one game flag.

    flag_refs.py 1ee write
    flag_refs.py 0x0a2 both

The flag is a nibble position, in hex. The answer is two lists.

  static   the id is established by the source: a constant at the call, a
           constant passed to a function that hands its parameter on, or a
           local that only ever holds constants. A write shows the number it
           stores, a read what the result is compared with.
  dynamic  the id comes out of data at run time - a request's field, a table
           entry. A site is listed with the places that put this flag into what
           it reads; the sites whose data has no known source of the flag are
           counted by what they read, since any of them could be handed it.

Ids and values are followed through parameters, locals, struct fields, globals
and return values, without regard to order: a field holds every constant the
source ever stores in it. Two overlays of one family are never loaded together,
so a constant stored by one room is not offered to a site in another.

A whole-byte access covers both flags of its pair and shows the half that is
this flag's. Flags 0x68..0x6b are also the play-time mark.

What it cannot see: a table that is still assembly, a bank copied whole (save,
load, the attract demo), and an id computed from something that is not a
constant. The first run parses every unit (about half a minute); later runs read
the index it wrote, `local/flag_refs.json`, and rebuild it when a source or
header has changed.
"""

import ctypes
import hashlib
import os
import json
import sys
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cref  # noqa: E402
import clang.cindex as ci  # noqa: E402

K = ci.CursorKind
T = ci.TypeKind

# name -> (access, width of the access)
ACCESSORS = {
    "gameFlagSetNibble":     ("write", "nibble"),
    "gameFlagGetNibble":     ("read", "nibble"),
    "gameFlagSetPackedByte": ("write", "byte"),
    "gameFlagGetPackedByte": ("read", "byte"),
}
# Functions that hand a parameter on but refuse one id, which is then not a
# reference to that flag.
REFUSED_ID = {"gameFlagSetNibbleIfPresent": 0}
# The play-time mark shares the bytes of these nibble positions.
MARK_FIELD = "playTimeMark"
MARK_FLAGS = range(104, 108)
# Always loaded, so their stores reach every overlay.
RESIDENT = {"main", "gameplay"}

INDEX = os.path.join("local", "flag_refs.json")
# A set of ids this large is not a set of flags.
MANY_IDS = 256
OTHER = ("o",)
WRAPPERS = (K.PAREN_EXPR, K.UNEXPOSED_EXPR, K.CSTYLE_CAST_EXPR)
INTS = {T.BOOL, T.CHAR_U, T.UCHAR, T.USHORT, T.UINT, T.ULONG, T.ULONGLONG, T.CHAR_S, T.SCHAR,
        T.SHORT, T.INT, T.LONG, T.LONGLONG, T.ENUM}
ARRAYS = {T.CONSTANTARRAY, T.INCOMPLETEARRAY, T.VARIABLEARRAY}
COMPARISONS = {"==", "!=", "<", "<=", ">", ">="}
FLIPPED = {"<": ">", "<=": ">=", ">": "<", ">=": "<=", "==": "==", "!=": "!="}
LIMIT = 1 << 16
BIG_TABLE = 4096
# Stores kept for one node in one unit. Past it the node counts as holding
# anything: that is animation and geometry data, not a table of flags.
MANY_STORES = 64

_lib = ci.conf.lib
_lib.clang_Cursor_Evaluate.argtypes = [ci.Cursor]
_lib.clang_Cursor_Evaluate.restype = ctypes.c_void_p
_lib.clang_EvalResult_getKind.argtypes = [ctypes.c_void_p]
_lib.clang_EvalResult_getKind.restype = ctypes.c_int
_lib.clang_EvalResult_getAsLongLong.argtypes = [ctypes.c_void_p]
_lib.clang_EvalResult_getAsLongLong.restype = ctypes.c_longlong
_lib.clang_EvalResult_dispose.argtypes = [ctypes.c_void_p]
_lib.clang_getCursorBinaryOperatorKind.argtypes = [ci.Cursor]
_lib.clang_getCursorBinaryOperatorKind.restype = ctypes.c_int
_lib.clang_getCursorUnaryOperatorKind.argtypes = [ci.Cursor]
_lib.clang_getCursorUnaryOperatorKind.restype = ctypes.c_int

# CX_BinaryOperatorKind and CX_UnaryOperatorKind, by number.
BINARY = [None, ".*", "->*", "*", "/", "%", "+", "-", "<<", ">>", "<=>", "<", ">", "<=", ">=", "==",
          "!=", "&", "^", "|", "&&", "||", "=", "*=", "/=", "%=", "+=", "-=", "<<=", ">>=", "&=",
          "^=", "|=", ","]
UNARY = [None, "x++", "x--", "++x", "--x", "&", "*", "+", "-", "~", "!"]


def constant(cur):
    """The integer `cur` evaluates to at compile time, or None."""
    if cur.kind == K.DECL_REF_EXPR:
        ref = cur.referenced
        if ref is not None and ref.kind == K.ENUM_CONSTANT_DECL:
            return ref.enum_value
        return None
    if cur.kind in (K.CALL_EXPR, K.MEMBER_REF_EXPR, K.ARRAY_SUBSCRIPT_EXPR):
        return None
    result = _lib.clang_Cursor_Evaluate(cur)
    if not result:
        return None
    value = _lib.clang_EvalResult_getAsLongLong(result) if _lib.clang_EvalResult_getKind(result) == 1 else None
    _lib.clang_EvalResult_dispose(result)
    return value


def binary(cur) -> str:
    n = _lib.clang_getCursorBinaryOperatorKind(cur)
    return BINARY[n] if 0 < n < len(BINARY) else ""


def unary(cur) -> str:
    n = _lib.clang_getCursorUnaryOperatorKind(cur)
    return UNARY[n] if 0 < n < len(UNARY) else ""


def intlike(type_) -> bool:
    """Whether a value of this type can be a flag id or a flag's value: an
    integer, or the array or pointer a table of them is reached through."""
    t = type_.get_canonical()
    for _ in range(4):
        if t.kind in INTS:
            return True
        if t.kind in ARRAYS:
            t = t.get_array_element_type().get_canonical()
        elif t.kind == T.POINTER:
            t = t.get_pointee().get_canonical()
        else:
            return False
    return False


def unwrap(cur):
    while cur.kind in WRAPPERS:
        kids = list(cur.get_children())
        if not kids:
            break
        cur = kids[-1]
    return cur


def moved(terms, step):
    """`terms` after one arithmetic step, a pair such as ("+", 4)."""
    out = []
    for term in terms:
        if term[0] == "c":
            out.append(("c", apply((step,), term[1])))
        elif term[0] == "n":
            out.append(("n", term[1], term[2] + (step,)))
        else:
            out.append(term)
    return out


def apply(steps, value):
    for op, n in steps:
        if op == "+":
            value += n
        elif op == "&":
            value &= n
        elif op == "neg":
            value = -value
    return value


class Scan:
    """What one unit says: the flag calls, and every store of an integer."""

    def __init__(self, tu, root):
        self.tu = tu
        self.root = root
        self.sites = []       # flag accesses
        self.calls = {}       # callee -> [(where, caller, {argument: terms})]
        self.stores = {}      # node -> {(term, where)}
        self.loose = set()    # nodes changed in a way that is not a store of a value
        self.taken = set()    # functions whose address is used
        self.names = {}       # node or function -> how to show it
        self.expects = {}     # node -> what a local holding a read is compared with
        self.callees = set()
        self.files = {}
        self.function = None
        self.params = {}
        self.reads_into = set()

    # -- naming -----------------------------------------------------------

    def rel(self, cur):
        f = cur.location.file
        if f is None:
            return None
        name = f.name
        rel = self.files.get(name)
        if rel is None:
            rel = self.files[name] = cref.relpath(name, self.root)
        return rel

    def where(self, cur):
        loc = cur.location
        return (self.rel(cur) or "?", loc.line, loc.column)

    def text(self, cur) -> str:
        ext = cur.extent
        f = ext.start.file
        if f is None:
            return ""
        try:
            data = _source(f.name)
        except OSError:
            return ""
        return " ".join(data[ext.start.offset:ext.end.offset].decode("utf-8", "replace").split())

    def function_key(self, decl):
        key = decl.get_usr() or decl.spelling
        self.names.setdefault(key, decl.spelling)
        return key

    def node(self, decl):
        """The node a declaration's value lives in, or None."""
        kind = decl.kind
        if kind == K.PARM_DECL:
            key = self.params.get(decl.hash)
            if key is not None:
                self.names.setdefault(key, f"parameter {decl.spelling}")
            return key
        if kind == K.FIELD_DECL:
            key = "F|" + decl.get_usr()
            if key not in self.names:
                owner = decl.semantic_parent
                owner = owner.type.spelling if owner is not None else ""
                if "unnamed" in owner or "anonymous" in owner:
                    owner = ""
                self.names[key] = f"{owner}.{decl.spelling}" if owner else f".{decl.spelling}"
            return key
        if kind == K.VAR_DECL:
            parent = decl.semantic_parent
            local = parent is not None and parent.kind == K.FUNCTION_DECL
            key = ("L|" if local else "G|") + decl.get_usr()
            self.names.setdefault(key, decl.spelling)
            return key
        return None

    # -- values -----------------------------------------------------------

    def resolve(self, cur):
        """The places the value of an expression can come from."""
        cur = unwrap(cur)
        kind = cur.kind
        value = constant(cur)
        if value is not None:
            return [("c", value)] if -LIMIT < value < LIMIT else []
        if kind in (K.DECL_REF_EXPR, K.MEMBER_REF_EXPR):
            ref = cur.referenced
            key = self.node(ref) if ref is not None else None
            return [("n", key, ())] if key else [OTHER]
        if kind == K.ARRAY_SUBSCRIPT_EXPR:
            kids = list(cur.get_children())
            return self.resolve(kids[0]) if kids else [OTHER]
        if kind == K.CONDITIONAL_OPERATOR:
            kids = list(cur.get_children())
            if len(kids) == 3:
                return self.resolve(kids[1]) + self.resolve(kids[2])
            return [OTHER]
        if kind == K.CALL_EXPR:
            ref = cur.referenced
            if ref is not None and ref.kind == K.FUNCTION_DECL and ref.spelling not in ACCESSORS:
                return [("n", "R|" + self.function_key(ref), ())]
            return [OTHER]
        if kind == K.BINARY_OPERATOR:
            kids = list(cur.get_children())
            if len(kids) != 2:
                return [OTHER]
            op = binary(cur)
            if op in ("=", ","):
                return self.resolve(kids[1])
            if op in ("+", "-", "&"):
                right = constant(kids[1])
                if right is not None:
                    return moved(self.resolve(kids[0]), ("+", -right) if op == "-" else (op, right))
                left = constant(kids[0])
                if left is not None and op != "-":
                    return moved(self.resolve(kids[1]), (op, left))
            return [OTHER]
        if kind == K.UNARY_OPERATOR and unary(cur) == "-":
            kids = list(cur.get_children())
            return moved(self.resolve(kids[0]), ("neg", 0)) if kids else [OTHER]
        return [OTHER]

    def target(self, cur):
        """The node an lvalue stores into, or None."""
        cur = unwrap(cur)
        if cur.kind in (K.DECL_REF_EXPR, K.MEMBER_REF_EXPR):
            ref = cur.referenced
            return self.node(ref) if ref is not None else None
        if cur.kind == K.ARRAY_SUBSCRIPT_EXPR:
            kids = list(cur.get_children())
            return self.target(kids[0]) if kids else None
        return None

    def store(self, key, terms, where):
        if key is None:
            return
        bucket = self.stores.setdefault(key, set())
        for term in terms:
            if term == OTHER or len(bucket) >= MANY_STORES:
                self.loose.add(key)
            else:
                bucket.add((term, where[:2]))

    def fill(self, cur, type_, key=None):
        """Record what a braced initialiser stores in each field it names.

        `key` is the variable the list belongs to while it is still an array of
        integers; a field's own node replaces it once a record is entered.
        """
        t = type_.get_canonical()
        kids = list(cur.get_children())
        if t.kind in ARRAYS:
            if len(kids) > BIG_TABLE:
                return
            element = t.get_array_element_type()
            for kid in kids:
                if kid.kind == K.UNEXPOSED_EXPR and kid.type.kind == T.VOID:   # [index] = value
                    parts = list(kid.get_children())
                    if not parts:
                        continue
                    kid = parts[-1]
                self.fill_one(kid, element, key)
            return
        if t.kind != T.RECORD:
            if kids:
                self.fill_one(kids[0], type_, key)
            return
        fields = list(t.get_fields())
        position = 0
        for kid in kids:
            if kid.kind == K.UNEXPOSED_EXPR and kid.type.kind == T.VOID:       # .field = value
                parts = list(kid.get_children())
                if len(parts) < 2 or parts[0].kind != K.MEMBER_REF:
                    return
                named = parts[0].referenced
                names = [f.spelling for f in fields]
                if len(parts) != 2 or named is None or named.spelling not in names:
                    return                                                      # a nested designator
                position = names.index(named.spelling)
                kid = parts[1]
            if position >= len(fields):
                return
            field = fields[position]
            position += 1
            inner = unwrap(kid) if kid.kind != K.INIT_LIST_EXPR else kid
            if field.type.get_canonical().kind in ARRAYS | {T.RECORD} and inner.kind != K.INIT_LIST_EXPR:
                return                                                          # braces left out: positions unknown
            self.fill_one(kid, field.type, self.node(field))

    def fill_one(self, cur, type_, key):
        if cur.kind == K.INIT_LIST_EXPR:
            self.fill(cur, type_, key)
        elif key is not None and intlike(type_):
            self.store(key, self.resolve(cur), self.where(cur))

    # -- what a read is used for --------------------------------------------

    def expectation(self, cur, parents):
        """How the value at `cur` is used: a comparison, a switch, a store."""
        child = cur
        for parent in reversed(parents):
            kind = parent.kind
            if kind in WRAPPERS:
                child = parent
                continue
            kids = list(parent.get_children())
            first = bool(kids) and kids[0] == child
            if kind == K.BINARY_OPERATOR and len(kids) == 2:
                op = binary(parent)
                other = kids[1] if first else kids[0]
                if op in COMPARISONS:
                    value = constant(other)
                    if value is None:
                        return ("text", f"{op if first else FLIPPED[op]} {self.text(other)}")
                    return ("cmp", op if first else FLIPPED[op], value)
                if op == "&":
                    value = constant(other)
                    return ("text", f"& {value:#x}" if value is not None else f"& {self.text(other)}")
                if op in ("&&", "||"):
                    return ("cmp", "!=", 0)
                if op == "=" and not first:
                    key = self.target(kids[0])
                    return ("into", key) if key else ("text", f"stored in {self.text(kids[0])}")
                return ("text", self.text(parent))
            if kind == K.UNARY_OPERATOR and unary(parent) == "!":
                return ("cmp", "==", 0)
            if kind == K.SWITCH_STMT and first:
                cases = []
                for c in parent.walk_preorder():
                    if c.kind == K.CASE_STMT:
                        ck = list(c.get_children())
                        value = constant(ck[0]) if ck else None
                        cases.append(value if value is not None else "?")
                    elif c.kind == K.DEFAULT_STMT:
                        cases.append("default")
                return ("switch", tuple(cases))
            if kind in (K.IF_STMT, K.WHILE_STMT, K.DO_STMT, K.CONDITIONAL_OPERATOR) and (first or kind == K.DO_STMT):
                return ("cmp", "!=", 0)
            if kind == K.FOR_STMT:
                return ("cmp", "!=", 0)
            if kind == K.VAR_DECL:
                key = self.node(parent)
                return ("into", key) if key else None
            if kind == K.CALL_EXPR:
                return ("text", f"passed to {parent.spelling}" if parent.spelling else "passed on")
            if kind == K.RETURN_STMT:
                return ("text", "returned")
            if kind == K.ARRAY_SUBSCRIPT_EXPR:
                return ("text", f"index of {self.text(kids[0])}" if kids else "an index")
            return None
        return None

    # -- the walk -----------------------------------------------------------

    def run(self):
        for cur in self.tu.cursor.get_children():
            rel = self.rel(cur)
            if rel is None or rel.startswith("..") or rel.startswith("include/psyq"):
                continue
            if cur.kind == K.FUNCTION_DECL and cur.is_definition():
                self.enter(cur)
            elif cur.kind == K.VAR_DECL:
                self.variable(cur)
                for kid in cur.get_children():
                    self.visit(kid, [cur])

    def enter(self, fn):
        self.function = self.function_key(fn)
        self.params = {}
        for i, p in enumerate(fn.get_arguments()):
            self.params[p.hash] = f"P|{self.function}|{i}"
        self.reads_into = set()
        body = [c for c in fn.get_children() if c.kind == K.COMPOUND_STMT]
        for c in body:
            self.visit(c, [fn])
        if self.reads_into:
            for c in body:
                self.uses(c, [fn])
        self.function = None
        self.params = {}

    def uses(self, cur, parents):
        """Second pass: what the locals holding a read are compared with."""
        if cur.kind == K.DECL_REF_EXPR:
            ref = cur.referenced
            if ref is not None and ref.kind in (K.VAR_DECL, K.PARM_DECL):
                key = self.node(ref)
                if key in self.reads_into:
                    found = self.expectation(cur, parents)
                    if found and found[0] in ("cmp", "switch"):
                        self.expects.setdefault(key, set()).add(found)
            return
        parents.append(cur)
        for kid in cur.get_children():
            self.uses(kid, parents)
        parents.pop()

    def variable(self, cur):
        if not cur.is_definition() and cur.semantic_parent.kind != K.FUNCTION_DECL:
            return
        kids = list(cur.get_children())
        if not kids:
            return
        init = kids[-1]
        key = self.node(cur)
        if init.kind == K.INIT_LIST_EXPR:
            self.fill(init, cur.type, key if intlike(cur.type) else None)
        elif cur.type.get_canonical().kind in ARRAYS:
            return                                   # the last child is the array's size
        elif init.kind.is_expression() and intlike(cur.type):
            self.store(key, self.resolve(init), self.where(init))

    def visit(self, cur, parents):
        kind = cur.kind
        if kind == K.CALL_EXPR:
            self.call(cur, parents)
        elif kind == K.BINARY_OPERATOR:
            if binary(cur) == "=":
                kids = list(cur.get_children())
                if len(kids) == 2 and intlike(kids[0].type):
                    self.store(self.target(kids[0]), self.resolve(kids[1]), self.where(kids[1]))
        elif kind == K.COMPOUND_ASSIGNMENT_OPERATOR:
            kids = list(cur.get_children())
            if kids:
                key = self.target(kids[0])
                if key:
                    self.loose.add(key)
        elif kind == K.UNARY_OPERATOR:
            op = unary(cur)
            if op in ("x++", "x--", "++x", "--x", "&"):
                kids = list(cur.get_children())
                key = self.target(kids[0]) if kids else None
                if key:
                    self.loose.add(key)
        elif kind == K.VAR_DECL:
            self.variable(cur)
        elif kind == K.COMPOUND_LITERAL_EXPR:
            kids = list(cur.get_children())
            if kids and kids[-1].kind == K.INIT_LIST_EXPR:
                self.fill(kids[-1], cur.type)
        elif kind == K.RETURN_STMT:
            kids = list(cur.get_children())
            if kids and self.function and intlike(kids[0].type):
                self.store("R|" + self.function, self.resolve(kids[0]), self.where(kids[0]))
        elif kind == K.DECL_REF_EXPR:
            ref = cur.referenced
            if ref is not None and ref.kind == K.FUNCTION_DECL and cur.hash not in self.callees:
                self.taken.add(self.function_key(ref))
            return
        elif kind == K.MEMBER_REF_EXPR and cur.spelling == MARK_FIELD:
            self.mark(cur, parents)
        parents.append(cur)
        for kid in cur.get_children():
            self.visit(kid, parents)
        parents.pop()

    def call(self, cur, parents):
        kids = list(cur.get_children())
        if kids:
            callee = unwrap(kids[0])
            if callee.kind == K.DECL_REF_EXPR:
                self.callees.add(callee.hash)
        ref = cur.referenced
        if ref is None or ref.kind != K.FUNCTION_DECL:
            return
        args = list(cur.get_arguments())
        where = self.where(cur)
        resolved = {}
        for i, arg in enumerate(args):
            if intlike(arg.type):
                terms = self.resolve(arg)
                if terms != [OTHER]:
                    resolved[i] = terms
        name = ref.spelling
        caller = self.function or ""
        if resolved:
            texts = {i: self.text(args[i]) for i in resolved}
            self.calls.setdefault(self.function_key(ref), []).append((where, caller, resolved, texts))
        if name not in ACCESSORS or not args:
            return
        access, width = ACCESSORS[name]
        site = {"where": where, "function": caller, "call": name, "access": access, "width": width,
                "id": resolved.get(0, [OTHER]), "id_text": self.text(args[0])}
        if access == "write" and len(args) > 1:
            site["value"] = resolved.get(1, [OTHER])
            site["value_text"] = self.text(args[1])
        else:
            found = self.expectation(cur, parents)
            if found:
                site["expect"] = found
                if found[0] == "into":
                    self.reads_into.add(found[1])
        self.sites.append(site)

    def mark(self, cur, parents):
        """An access to the play-time mark, which lies over four flags."""
        access = "read"
        child = cur
        for parent in reversed(parents):
            if parent.kind in WRAPPERS or parent.kind == K.ARRAY_SUBSCRIPT_EXPR:
                child = parent
                continue
            kids = list(parent.get_children())
            first = bool(kids) and kids[0] == child
            if parent.kind == K.BINARY_OPERATOR and binary(parent) == "=" and first:
                access = "write"
            elif parent.kind == K.COMPOUND_ASSIGNMENT_OPERATOR and first:
                access = "both"
            elif parent.kind == K.UNARY_OPERATOR and unary(parent) in ("&", "x++", "x--", "++x", "--x"):
                access = "both"
            break
        self.sites.append({"where": self.where(cur), "function": self.function or "", "call": MARK_FIELD,
                           "access": access, "width": "mark", "id": [], "id_text": ""})


_sources = {}


def _source(path):
    data = _sources.get(path)
    if data is None:
        with open(path, "rb") as f:
            data = _sources[path] = f.read()
    return data


def stamp(root, rel):
    try:
        st = os.stat(os.path.join(root, rel))
    except OSError:
        return None
    return (st.st_mtime_ns, st.st_size)


def scan_unit(job):
    rel, args, root = job
    sys.setrecursionlimit(20000)
    tu = cref.parse_tu(rel, args, root)
    if tu is None:
        return rel, None
    deps = {rel}
    for inc in tu.get_includes():
        if inc.include is not None:
            r = cref.relpath(inc.include.name, root)
            if not r.startswith("..") and not r.startswith("include/psyq"):
                deps.add(r)
    scan = Scan(tu, root)
    scan.run()
    _sources.clear()
    return rel, {
        "deps": {d: stamp(root, d) for d in deps},
        "sites": scan.sites, "calls": scan.calls, "stores": scan.stores, "loose": scan.loose,
        "taken": scan.taken, "names": scan.names, "expects": scan.expects,
    }


def code_key() -> str:
    with open(os.path.abspath(__file__), "rb") as f:
        return hashlib.sha1(f.read()).hexdigest()


def scan_all(root, db, jobs):
    """Every unit's facts."""
    units = {}
    print(f"reading {len(db)} units...", file=sys.stderr)
    with Pool(jobs) as pool:
        work = [(rel, db[rel], root) for rel in sorted(db)]
        for rel, facts in pool.imap_unordered(scan_unit, work, chunksize=4):
            if facts is not None:
                units[rel] = facts
    return units


def overlay_of(unit):
    parts = unit.split("/")
    if len(parts) >= 3 and parts[0] == "src":
        if len(parts) == 3:
            return parts[1]
        return f"{parts[1]}/{parts[2]}"
    return unit


def together(a, b) -> bool:
    """Whether two overlays can be loaded at once."""
    if a == b or a in RESIDENT or b in RESIDENT:
        return True
    return a.split("/")[0] != b.split("/")[0]


class Facts:
    """All units merged, with the questions the report asks of them."""

    def __init__(self, units):
        self.sites = {}
        self.calls = {}
        self.stores = {}
        self.loose = set()
        self.taken = set()
        self.names = {}
        self.expects = {}
        self.homes = {}       # file -> the overlays whose units read it
        for unit, facts in units.items():
            home = overlay_of(unit)
            for dep in facts["deps"]:
                self.homes.setdefault(dep, set()).add(home)
            for site in facts["sites"]:
                self.sites.setdefault((site["where"], site["call"], site["function"]), site)
            for callee, calls in facts["calls"].items():
                bucket = self.calls.setdefault(callee, {})
                for where, caller, args, texts in calls:
                    bucket.setdefault((where, caller), (args, texts))
            for key, entries in facts["stores"].items():
                self.stores.setdefault(key, set()).update(entries)
            self.loose |= facts["loose"]
            self.taken |= facts["taken"]
            for key, name in facts["names"].items():
                self.names.setdefault(key, name)
            for key, found in facts["expects"].items():
                self.expects.setdefault(key, set()).update(found)
        self.memo = {}
        self.placed = {}
        self.deep = True
        self.deep_memo, self.shallow_memo = self.memo, {}

    def near(self, a, b) -> bool:
        """Whether code in file `a` and code in file `b` can run in one session."""
        ha, hb = self.homes.get(a, {a}), self.homes.get(b, {b})
        return any(together(x, y) for x in ha for y in hb)

    def open(self, key) -> bool:
        """Whether a node can hold values the source does not show."""
        kind = key[0]
        if kind in "FG" or key in self.loose:
            return True
        if kind == "P":
            fn, index = key[2:].rsplit("|", 1)
            return fn in self.taken or not self.arguments(fn, int(index))
        if kind == "R":
            return key not in self.stores
        return False

    def arguments(self, fn, index):
        out = []
        for (where, _caller), (args, _texts) in self.calls.get(fn, {}).items():
            out.append((args.get(index, [OTHER]), where[:2]))
        return out

    def sources(self, key):
        out = [([term], where) for term, where in self.stores.get(key, ())]
        if key[0] == "P":
            fn, index = key[2:].rsplit("|", 1)
            out += self.arguments(fn, int(index))
        return out

    def values(self, key):
        """(the values a node can hold, whether more are possible).

        Fields and globals are followed for an id only. A value stored through
        one is reported as dynamic: every vector and task word feeds another,
        and following them all costs gigabytes to say nothing.
        """
        if key in self.memo:
            return self.memo[key]
        feeds, todo = {}, [key]
        while todo:
            k = todo.pop()
            if k in feeds or k in self.memo:
                continue
            feeds[k] = self.sources(k) if self.deep or k[0] not in "FG" else []
            for terms, _where in feeds[k]:
                todo.extend(t[1] for t in terms if t[0] == "n")
        state = {k: [set(), self.open(k)] for k in feeds}
        changed, rounds, wide = True, 0, set()
        while changed:
            changed = False
            rounds += 1
            for k, sources in feeds.items():
                mine = state[k]
                if k in wide:
                    continue
                before = len(mine[0])
                for terms, _where in sources:
                    for term in terms:
                        if term[0] == "c":
                            mine[0].add(term[1])
                        elif term[0] == "n":
                            got, loose = self.memo.get(term[1]) or state[term[1]]
                            if loose and not mine[1]:
                                mine[1] = changed = True
                            if term[2]:
                                if rounds > 8:      # arithmetic feeding itself: no end to it
                                    mine[1] = True
                                    continue
                                mine[0].update([apply(term[2], v) for v in got])
                            elif got is not mine[0]:
                                mine[0] |= got
                        elif not mine[1]:
                            mine[1] = changed = True
                if len(mine[0]) > 4 * MANY_IDS:     # coordinates, counters: not a flag's carrier
                    wide.add(k)
                    mine[0], mine[1] = set(), True
                    changed = True
                elif len(mine[0]) != before:
                    changed = True
        for k, (found, more) in state.items():
            self.memo[k] = (found, more)
        return self.memo[key]

    def of(self, terms, deep=True):
        """(the values a list of terms can take, whether more are possible)."""
        self.deep = deep
        self.memo = self.deep_memo if deep else self.shallow_memo
        found, more = set(), False
        for term in terms:
            if term[0] == "c":
                found.add(term[1])
            elif term[0] == "n":
                got, loose = self.values(term[1])
                more = more or loose
                found.update(apply(term[2], v) for v in got)
            else:
                more = True
        return found, more

    def places(self, terms, where, value, busy=frozenset()):
        """Where the source stores `value` into what these terms read."""
        out = set()
        for term in terms:
            if term[0] == "c":
                if term[1] == value:
                    out.add(where)
            elif term[0] == "n":
                key = term[1]
                self.deep, self.memo = True, self.deep_memo
                for inner in {v for v in self.values(key)[0] if apply(term[2], v) == value}:
                    if (key, inner) in busy:
                        continue
                    known = self.placed.get((key, inner))
                    if known is None:
                        known = set()
                        for src, at in self.sources(key):
                            known |= self.places(src, at, inner, busy | {(key, inner)})
                        if not busy:
                            self.placed[(key, inner)] = known
                    out |= known
        return out

    def contexts(self, site, depth=0, seen=()):
        """A site as each caller sees it, where its id is a parameter."""
        fn = site["function"]
        prefix = f"P|{fn}|"
        own = [t for t in site["id"] if t[0] == "n" and t[1].startswith(prefix)]
        callers = self.calls.get(fn, {})
        if not own or not callers or depth > 5 or fn in seen:
            yield site
            return
        if fn in self.taken:
            yield site

        def swap(terms, args):
            out = []
            for t in terms:
                if t[0] == "n" and t[1].startswith(prefix):
                    index = int(t[1][len(prefix):])
                    passed = args.get(index, [OTHER])
                    for step in t[2]:
                        passed = moved(passed, step)
                    out.extend(passed)
                else:
                    out.append(t)
            return out

        name = self.names.get(fn, fn)
        for (where, caller), (args, texts) in callers.items():
            index = int(own[0][1][len(prefix):])
            if not self.near(where[0], site["where"][0]):
                continue
            inner = dict(site)
            inner["id"] = swap(site["id"], args)
            inner["id_text"] = texts.get(index, "?")
            if "value" in site:
                value = swap(site["value"], args)
                if value != site["value"]:
                    inner["value"] = value
                    for t in site["value"]:
                        if t[0] == "n" and t[1].startswith(prefix):
                            inner["value_text"] = texts.get(int(t[1][len(prefix):]), "?")
            inner["where"] = where
            inner["function"] = caller
            inner["via"] = (name,) + site.get("via", ())
            inner["refused"] = site.get("refused", ()) + ((REFUSED_ID[name],) if name in REFUSED_ID else ())
            yield from self.contexts(inner, depth + 1, seen + (fn,))


def half(flag, byte):
    """This flag's half of a whole byte: even positions are the high nibble."""
    return (byte >> 4) & 0xF if flag % 2 == 0 else byte & 0xF


def expected(facts, found, width):
    kind = found[0]
    if kind == "cmp":
        return f"{found[1]} byte {found[2]:#04x}" if width == "byte" else f"{found[1]} {found[2]}"
    if kind == "switch":
        return "switch " + ",".join(str(c) for c in found[1])
    if kind == "text":
        return found[1]
    if kind == "into":
        name = facts.names.get(found[1], "?")
        uses = sorted(facts.expects.get(found[1], ()), key=repr)
        if not uses:
            return f"kept in {name}"
        return f"kept in {name}: " + "; ".join(expected(facts, u, width) for u in uses[:6])
    return ""


def rows(facts):
    """One row for each flag access, as each caller sees it."""
    out, seen = [], set()
    for site in facts.sites.values():
        for ctx in facts.contexts(site):
            file, line, _col = ctx["where"]
            via = list(ctx.get("via", ()))
            if (ctx["where"], ctx["access"], tuple(via)) in seen:
                continue
            seen.add((ctx["where"], ctx["access"], tuple(via)))
            row = {"file": file, "line": line, "function": facts.names.get(ctx["function"], ctx["function"]),
                   "via": via, "access": ctx["access"], "width": ctx["width"]}
            if ctx["width"] == "mark":
                row.update(ids=list(MARK_FLAGS), more=False, direct=True)
                out.append(row)
                continue
            found, more = facts.of(ctx["id"])
            found -= set(ctx.get("refused", ()))
            if len(found) > MANY_IDS:
                found, more = set(), True
            origins = {}
            for v in sorted(found):
                places = sorted(p for p in facts.places(ctx["id"], (file, line), v) if facts.near(p[0], file))
                if places:
                    origins[v] = places
            row.update(ids=sorted(origins), more=more, direct=all(t[0] == "c" for t in ctx["id"]),
                       id_text=ctx["id_text"])
            if not row["direct"]:
                row["origins"] = {str(v): places for v, places in origins.items()}
                row["source"] = ", ".join(sorted({facts.names.get(t[1], t[1]) for t in ctx["id"] if t[0] == "n"})) \
                    or ctx["id_text"]
            if ctx["access"] == "write":
                values, loose = facts.of(ctx.get("value", [OTHER]), deep=False)
                row.update(values=sorted(v & 0xFF for v in values), dynamic=loose, value_text=ctx.get("value_text", "?"))
            elif ctx.get("expect"):
                row["expect"] = expected(facts, ctx["expect"], ctx["width"])
            out.append(row)
    out.sort(key=lambda r: (r["file"], r["line"], r["access"], r["via"]))
    return out


def tracked(root):
    stamps = {}
    for top in ("src", "include"):
        for folder, _dirs, files in os.walk(os.path.join(root, top)):
            for name in files:
                if name.endswith((".c", ".h")):
                    rel = os.path.relpath(os.path.join(folder, name), root)
                    stamps[rel] = list(stamp(root, rel))
    return stamps


def index(root):
    """The rows, rebuilt when a source or header changed since they were made."""
    path = os.path.join(root, INDEX)
    db = cref.load_db(root)
    if not any(rel.startswith("src/gameplay/") for rel in db):
        sys.exit(f"{root} is not a configured tree: its compile_commands.json has {len(db)} units and none "
                 "of gameplay.\nRun the copy in the tree the build runs in; it works from any directory.")
    key = [code_key(), hashlib.sha1(repr(sorted(db.items())).encode()).hexdigest()]
    stamps = tracked(root)
    try:
        with open(path) as f:
            saved = json.load(f)
        if saved["key"] == key and saved["stamps"] == stamps:
            return saved["rows"]
    except (OSError, ValueError, KeyError):
        pass
    jobs = int(os.environ.get("PE2_JOBS") or os.cpu_count() or 4)
    facts = Facts(scan_all(root, db, jobs))
    made = rows(facts)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + ".tmp", "w") as f:
        json.dump({"key": key, "stamps": stamps, "rows": made}, f, separators=(",", ":"))
    os.replace(path + ".tmp", path)
    return made


def detail(row, flag):
    """The value a write stores, or what a read's result is compared with."""
    if row["width"] == "mark":
        return "the play-time mark, which shares this flag's bytes"
    if row["access"] != "write":
        text = row.get("expect", "")
        return text if len(text) <= 90 else text[:87] + "..."
    if row["width"] == "byte":
        shown = [f"{half(flag, v)} (byte {v:#04x})" for v in row["values"]]
    else:
        shown = list(dict.fromkeys(str(v & 0xF) for v in row["values"]))
    if row["dynamic"]:
        return "value dynamic: " + row["value_text"] + (f" (seen {'|'.join(shown)})" if shown else "")
    return "value " + ("|".join(shown) if shown else "?")


def lines(row, flag, wanted):
    out = f"  {row['access']:5}  {row['file']}:{row['line']}  {row['function'] or '(file scope)'}"
    if row["via"]:
        out += "  via " + " > ".join(row["via"])
    if row["width"] == "byte":
        out += "  whole byte"
    about = detail(row, flag)
    if about:
        out += "  " + about
    made = [out]
    others = [v for v in row["ids"] if v not in wanted]
    if others and not row["more"] and row["width"] != "mark":
        shown = "|".join(f"{v:#05x}" for v in others[:8]) + (" ..." if len(others) > 8 else "")
        made.append(f"           or flag {shown}")
    if not row.get("direct", True):
        places = sorted({tuple(p) for v in wanted for p in row["origins"].get(str(v), ())})
        text = ", ".join(f"{f}:{n}" for f, n in places[:4]) + (f" and {len(places) - 4} more" if len(places) > 4 else "")
        made.append(f"           id {row['id_text']}, given {flag:#05x} at {text}")
    return made


def main() -> int:
    argv = sys.argv[1:]
    if len(argv) != 2 or argv[1] not in ("read", "write", "both"):
        print("usage: flag_refs.py FLAG read|write|both     (FLAG in hex)", file=sys.stderr)
        return 2
    try:
        flag = int(argv[0], 16)
    except ValueError:
        print(f"{argv[0]!r} is not a hex flag number", file=sys.stderr)
        return 2
    if not 0 <= flag < 504:
        print(f"flags run from 0x000 to 0x1f7; {flag:#x} is outside them", file=sys.stderr)
        return 2
    sys.setrecursionlimit(20000)
    mode = argv[1]
    static, reached, unknown = [], [], {}
    for row in index(cref.repo_root()):
        if mode != "both" and row["access"] not in (mode, "both"):
            continue
        wanted = (flag & ~1, flag | 1) if row["width"] == "byte" else (flag,)
        hit = any(v in row["ids"] for v in wanted)
        if hit and not row["more"]:
            static.append((row, wanted))
        elif hit:
            reached.append((row, wanted))
        elif row["more"]:
            unknown.setdefault((row["access"], row.get("source", "?")), []).append(row)
    what = {"read": "reads", "write": "writes", "both": "reads and writes"}[mode]
    print(f"flag {flag:#05x}, {what}")
    print(f"\nstatic: {len(static)}")
    for group in (static, reached):
        group.sort(key=lambda e: (e[0]["access"] != "write", e[0]["file"], e[0]["line"]))
    for row, wanted in static:
        print("\n".join(lines(row, flag, wanted)))
    rest = sum(len(v) for v in unknown.values())
    print(f"\ndynamic: {len(reached)} given this flag by the source, {rest} with no known source of it")
    for row, wanted in reached:
        print("\n".join(lines(row, flag, wanted)))
    for (access, name), found in sorted(unknown.items(), key=lambda kv: (kv[0][0] != "write", -len(kv[1]), kv[0][1])):
        files = sorted({r["file"] for r in found})
        shown = ", ".join(files[:3]) + (f" and {len(files) - 3} more files" if len(files) > 3 else "")
        print(f"  {access:5}  id from {name}: {len(found)} sites ({shown})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
