"""The headers a generated room include needs: those defining the C types of its objects and declaring the symbols
outside the room it points to, so a generated file compiles wherever it is included and the room's source includes
only what its own code uses.

A type or symbol is looked up in the project's headers by its definition - a typedef's name, a declaration with
`extern`, a function's prototype - and spelled as the build's include path finds it (`gameplay/collision.h`, `types.h`). Sony's headers
include nothing themselves, so a psyq header comes with the ones it needs before it, in Sony's order. A name no
header declares is the including source's own (a label, a callback), and needs nothing.
"""
from __future__ import annotations

import re
from functools import lru_cache
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
INCLUDE = ROOT / "include"
PSYQ = ["psyq/sys/types.h", "psyq/libgte.h", "psyq/libgpu.h", "psyq/inline_c.h", "psyq/gtemac.h", "psyq/libgs.h"]
PSYQ_NEEDS = {"psyq/libgs.h": PSYQ[:3], "psyq/gtemac.h": [PSYQ[0], PSYQ[1], PSYQ[3]], "psyq/libgpu.h": PSYQ[:2],
              "psyq/inline_c.h": PSYQ[:2], "psyq/libgte.h": PSYQ[:1]}
BASIC = {"u8", "s8", "u16", "s16", "u32", "s32", "u64", "s64", "f32", "NULL"}
TYPEDEF_CLOSE = re.compile(r"^\}\s*(\w+)\s*;", re.M)
TYPEDEF_SIMPLE = re.compile(r"^typedef\s+[^;{}()]*?\b(\w+)\s*;", re.M)
EXTERN = re.compile(r"^extern\s+[^;()]*?\b(\w+)\s*(?:\[[^\]]*\]\s*)*;", re.M)
# a function prototype at file scope: a return type, the name and its parameters (a script operand names one)
PROTOTYPE = re.compile(r"^(?!static\b|typedef\b|#)[A-Za-z_][\w \t\*]*?\b(\w+)\s*\([^;{}()]*\)\s*;", re.M)


def _spelling(path: Path) -> str:
    rel = path.relative_to(INCLUDE).as_posix()
    return rel.removeprefix("decomp/")                      # include/decomp is on the search path itself


@lru_cache(maxsize=None)
def _index() -> tuple[dict[str, str], dict[str, str]]:
    """(type -> header, symbol -> header): the first definition in a sorted walk, psyq's only for psyq's names."""
    types: dict[str, str] = {}
    symbols: dict[str, str] = {}
    headers = sorted(INCLUDE.rglob("*.h"), key=lambda p: (p.parts[len(INCLUDE.parts)] == "psyq", p.as_posix()))
    for h in headers:
        if h.parts[len(INCLUDE.parts)] == "psyq" and _spelling(h) not in PSYQ:
            continue
        text = h.read_text(errors="replace")
        for m in list(TYPEDEF_CLOSE.finditer(text)) + list(TYPEDEF_SIMPLE.finditer(text)):
            types.setdefault(m.group(1), _spelling(h))
        for m in EXTERN.finditer(text):
            symbols.setdefault(m.group(1), _spelling(h))
        for m in PROTOTYPE.finditer(text):
            symbols.setdefault(m.group(1), _spelling(h))
    return types, symbols


def headers_for(types: set[str], symbols: set[str]) -> list[str]:
    """The includes, in the project's order (types.h, Sony's, then the others sorted), that define `types` and
    declare `symbols`."""
    type_of, symbol_of = _index()
    out = set()
    if types & BASIC or "NULL" in symbols:
        out.add("types.h")
    for t in types - BASIC:
        h = type_of.get(t.rstrip("*").strip())
        if h:
            out.add(h)
    for s in symbols:
        h = symbol_of.get(s)
        if h:
            out.add(h)
    for h in list(out):
        out |= set(PSYQ_NEEDS.get(h, ()))
    psyq = [h for h in PSYQ if h in out]
    rest = sorted(h for h in out if h not in PSYQ and h != "types.h")
    return (["types.h"] if "types.h" in out else []) + psyq + rest


def include_lines(types: set[str], symbols: set[str]) -> list[str]:
    hs = headers_for(types, symbols)
    return [f'#include <{h}>' if h.startswith("psyq/") else f'#include "{h}"' for h in hs]
