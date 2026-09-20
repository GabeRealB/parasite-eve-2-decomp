"""Structured, function-local experiments for the pinned C compiler.

Parse retained preprocessed input with the bundled permuter's GNU C parser.
Only the selected body is printed back into the original source. This expands
body macros for the current build configuration; printing is a separate search
control, not assumed to be codegen-neutral. No global declarations are edited.
"""
from __future__ import annotations

import copy
from dataclasses import dataclass
from pathlib import Path
import re
import sys

from source_edits import masked

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "decomp-permuter"))
from perm_pycparser import c_ast as ca, c_generator, c_parser


@dataclass(frozen=True)
class Operator:
    families: tuple[str, ...]
    hypothesis: str
    prerequisite: str
    effect: str


OPERATORS = {
    "invert_if": Operator(("branch", "delay_slot"), "Invert the predicate and exchange its arms.",
                          "Evaluate the same scalar condition once; retain both arms.", "jump/jump2 layout and dbr eligibility"),
    "guard_else": Operator(("branch", "delay_slot"), "Move the else arm after an arm that always returns.",
                           "The true arm ends in return; preserve false-arm scope; no labels or gotos in the conditional.", "jump/jump2 layout and shared tails"),
    "join_returns": Operator(("branch", "delay_slot"), "Put the following return in an explicit else arm.",
                             "Both alternatives are direct returns; preserve the condition and return expressions.", "jump/jump2 exit shape"),
    "temporary": Operator(("register", "scheduling", "stack"), "Assign through a fresh temporary of the destination's integer type.",
                          "Destination is an unambiguously declared nonvolatile automatic integer; retain assignment conversion and RHS evaluation.", "rtl expansion, CSE and allocation lifetimes"),
    "dead_store": Operator(("register", "scheduling"), "Initialize a fresh temporary to zero before overwriting it with the assignment value.",
                           "Fresh nonvolatile integer temporary does not escape; initial store has a constant RHS and is overwritten before use.", "early RTL creation/deletion and allocation ordering"),
    "sched_barrier": Operator(("scheduling", "delay_slot", "hazard"), "Insert an empty volatile asm before this statement.",
                              "GCC-specific empty asm; inspect actual motion/dependency effects with the pinned compiler.", "sched/sched2 and dbr motion"),
    "memory_barrier": Operator(("scheduling", "register", "delay_slot"), "Insert an empty volatile asm with a memory clobber.",
                               "GCC-specific memory dependency experiment; does not emit a hardware memory fence.", "memory CSE, reloads and scheduling dependencies"),
    "volatile_temp": Operator(("register", "stack", "scheduling"), "Materialize the assignment through a fresh volatile integer temporary.",
                              "Adds volatile accesses to private local storage; this is an access-changing experiment, not C semantic equivalence.", "forced materialization, stack accesses and scheduling"),
}
DEFAULT_OPERATORS = tuple(name for name in OPERATORS if name != "volatile_temp")


def walk(node, path=()):
    yield path, node
    for name, child in node.children():
        yield from walk(child, path + (name,))


def get(node, path):
    for component in path:
        match = re.fullmatch(r"(\w+)\[(\d+)\]", component)
        node = getattr(node, match[1])[int(match[2])] if match else getattr(node, component)
    return node


def body_span(source, function):
    """Locate one literal definition, ignoring comments, strings and directives."""
    clean = masked(source)
    spans = []
    for match in re.finditer(r"\b" + re.escape(function) + r"\s*\(", clean):
        pos, depth = match.end(), 1
        while pos < len(clean) and depth:
            depth += (clean[pos] == "(") - (clean[pos] == ")")
            pos += 1
        while pos < len(clean) and clean[pos].isspace():
            pos += 1
        if pos == len(clean) or clean[pos] != "{":
            continue
        start, depth = pos, 1
        pos += 1
        while pos < len(clean) and depth:
            depth += (clean[pos] == "{") - (clean[pos] == "}")
            pos += 1
        if depth == 0:
            spans.append((start, pos))
    if len(spans) != 1:
        raise ValueError(f"need one literal definition of {function} in the original source")
    start, end = spans[0]
    if re.search(r"(?m)^\s*#|\b__(?:LINE|FILE|COUNTER)__\b", source[start:end]):
        raise ValueError("body directives and location-dependent macros are unsupported")
    return start, end


def parse(text):
    # Keep string literals, physical line numbers and GNU asm intact. Discard
    # line directives so AST coordinates refer to the saved input, not headers.
    pattern = r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|^[ \t]*\#[^\n]*'
    def hide(match):
        part = match[0]
        return part if part.startswith(('"', "'")) else re.sub(r"[^\n]", " ", part)
    text = re.sub(pattern, hide, text, flags=re.S | re.M)
    try:
        return c_parser.CParser().parse(text)
    except c_parser.ParseError as error:
        raise ValueError(f"unsupported preprocessed C: {error}") from error


def integer_type(decl, typedefs):
    """Resolve only ordinary integer typedefs; reject volatile/atomic/const."""
    node, seen = decl.type, set()
    while isinstance(node, ca.TypeDecl) and not node.quals and isinstance(node.type, ca.IdentifierType):
        names = node.type.names
        if set(names) <= {"signed", "unsigned", "char", "short", "int", "long"}:
            return copy.deepcopy(node)
        if len(names) != 1 or names[0] in seen or names[0] not in typedefs:
            break
        seen.add(names[0])
        node = typedefs[names[0]].type
    return None


@dataclass(frozen=True)
class Mutation:
    operator: str
    path: tuple[str, ...]
    line: int | None

    def metadata(self):
        op = OPERATORS[self.operator]
        return {"operator": self.operator, "ast_path": list(self.path), "input_line": self.line,
                "hypothesis": op.hypothesis, "prerequisite": op.prerequisite,
                "expected_effect": op.effect,
                "semantics": "access_changing" if self.operator == "volatile_temp" else
                             "compiler_specific" if self.operator.endswith("barrier") else "restricted_rewrite"}


class Program:
    def __init__(self, source, preprocessed=None, function=None):
        self.source = source
        self.ast = parse(preprocessed if preprocessed is not None else source)
        functions = [n for n in self.ast.ext if isinstance(n, ca.FuncDef)]
        if function:
            functions = [n for n in functions if n.decl.name == function]
        elif len(functions) != 1:
            local = []
            for fn in functions:
                try:
                    body_span(source, fn.decl.name)
                    local.append(fn)
                except ValueError:
                    pass
            functions = local
        if len(functions) != 1:
            raise ValueError("select one source function with --function")
        self.fn = functions[0]
        self.start, self.end = body_span(source, self.fn.decl.name)
        self.typedefs = {n.name: n for n in self.ast.ext if isinstance(n, ca.Typedef)}
        # Shadowed names and local typedefs require scope resolution beyond
        # this first implementation. Disable typed rewrites in those cases.
        decls = {}
        self.types = {}
        for _, node in walk(self.fn):
            if isinstance(node, ca.Decl) and node.name:
                decls.setdefault(node.name, []).append(node)
        if not any(isinstance(n, ca.Typedef) for _, n in walk(self.fn.body)):
            for name, declarations in decls.items():
                if len(declarations) == 1:
                    decl = declarations[0]
                    if not set(decl.storage) & {"extern", "static", "typedef"}:
                        typ = integer_type(decl, self.typedefs)
                        if typ:
                            self.types[name] = typ
        identifiers = {n.name for _, n in walk(self.ast) if isinstance(n, ca.ID)} | set(decls) | set(self.typedefs)
        number = 0
        while f"divergence_tmp_{number}" in identifiers or re.search(r"\bdivergence_tmp_" + str(number) + r"\b", source):
            number += 1
        self.temporary = f"divergence_tmp_{number}"

    def render(self, body=None):
        text = c_generator.CGenerator().visit(body if body is not None else self.fn.body).rstrip()
        return self.source[:self.start] + text + self.source[self.end:]

    def mutations(self, operators=DEFAULT_OPERATORS):
        result = []
        def add(name, path, node):
            if name in operators:
                result.append(Mutation(name, path, node.coord.line if node.coord else None))
        for path, node in walk(self.fn.body):
            if isinstance(node, ca.If):
                add("invert_if", path, node)
                tail = node.iftrue.block_items if isinstance(node.iftrue, ca.Compound) else [node.iftrue]
                if node.iffalse and tail and isinstance(tail[-1], ca.Return) and not any(
                        isinstance(n, (ca.Label, ca.Goto, ca.Case, ca.Default)) for _, n in walk(node)):
                    # Replace only a direct compound child so the else block
                    # keeps its scope and execution position.
                    if path and path[-1].startswith("block_items["):
                        add("guard_else", path, node)
            if not isinstance(node, ca.Compound):
                continue
            items = node.block_items or []
            for index, stmt in enumerate(items):
                stmt_path = path + (f"block_items[{index}]",)
                if isinstance(stmt, ca.If) and stmt.iffalse is None and index + 1 < len(items):
                    tail = stmt.iftrue.block_items if isinstance(stmt.iftrue, ca.Compound) else [stmt.iftrue]
                    if tail and len(tail) == 1 and isinstance(tail[0], ca.Return) and isinstance(items[index + 1], ca.Return):
                        add("join_returns", stmt_path, stmt)
                if isinstance(stmt, ca.Assignment) and stmt.op == "=" and isinstance(stmt.lvalue, ca.ID) and stmt.lvalue.name in self.types:
                    for name in ("temporary", "dead_store", "volatile_temp"):
                        add(name, stmt_path, stmt)
                if isinstance(stmt, (ca.Decl, ca.Typedef, ca.Pragma, ca.Asm, ca.Label, ca.Case, ca.Default)):
                    continue
                # Avoid consecutive fences and C89 declarations after asm.
                if any(isinstance(n, (ca.Decl, ca.Typedef)) for n in items[index:]):
                    continue
                if index and isinstance(items[index - 1], ca.Asm):
                    continue
                for name in ("sched_barrier", "memory_barrier"):
                    add(name, stmt_path, stmt)
        return result

    def apply(self, mutation):
        body = copy.deepcopy(self.fn.body)
        node = get(body, mutation.path)
        kind = mutation.operator
        if kind == "invert_if":
            node.cond = node.cond.expr if isinstance(node.cond, ca.UnaryOp) and node.cond.op == "!" else ca.UnaryOp("!", node.cond)
            def block(arm):
                return arm if isinstance(arm, ca.Compound) else ca.Compound([arm] if arm else [])
            # A swapped else-if can contain an unmatched if. Braces prevent
            # the emitted else from binding to that inner conditional.
            node.iftrue, node.iffalse = block(node.iffalse), block(node.iftrue)
        else:
            parent = get(body, mutation.path[:-1])
            index = int(re.fullmatch(r"block_items\[(\d+)\]", mutation.path[-1])[1])
            if kind == "guard_else":
                arm, node.iffalse = node.iffalse, None
                parent.block_items.insert(index + 1, arm)
            elif kind == "join_returns":
                node.iffalse = parent.block_items.pop(index + 1)
            elif kind in ("temporary", "dead_store", "volatile_temp"):
                typ = copy.deepcopy(self.types[node.lvalue.name])
                typ.declname = self.temporary
                if kind == "volatile_temp":
                    typ.quals = ["volatile"]
                decl = ca.Decl(name=self.temporary, quals=list(typ.quals), align=[], storage=[],
                               funcspec=[], gcc_attributes=[], type=typ,
                               init=ca.Constant("int", "0") if kind == "dead_store" else None,
                               bitsize=None, asmlabel=None)
                assignment = ca.Assignment("=", ca.ID(self.temporary), node.rvalue)
                node.rvalue = ca.ID(self.temporary)
                parent.block_items[index] = ca.Compound([decl, assignment, node])
            elif kind in ("sched_barrier", "memory_barrier"):
                asm = '__asm__ volatile(""' + (' ::: "memory"' if kind == "memory_barrier" else '') + ');'
                fence = parse("void divergence_fence(void) {" + asm + "}").ext[0].body.block_items[0]
                parent.block_items.insert(index, fence)
            else:
                raise ValueError(f"unknown transformation: {kind}")
        return self.render(body)
