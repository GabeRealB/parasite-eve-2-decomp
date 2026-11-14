"""TOML written from plain values, the same text for the same values: what the standard library reads (`tomllib`) but
does not write. Tables nested in a section's values are written inline, arrays of tables (`[[stack]]`) and tables of
tables (`[original.USA]`) as sections; an array holding tables or arrays, or too long for a line, one item a line.
"""
from __future__ import annotations

import json
import math
import re

BARE = re.compile(r"^[A-Za-z0-9_-]+$")
WIDTH = 120


def key(name: str) -> str:
    return name if BARE.match(name) else string(name)


def string(text: str) -> str:
    # JSON's escapes are TOML's basic string's (`\"`, `\\`, `\n`, `\uXXXX`, ...).
    return json.dumps(text, ensure_ascii=False)


def value(item, indent: str = "") -> str:
    if isinstance(item, bool):
        return "true" if item else "false"
    if isinstance(item, int):
        return str(item)
    if isinstance(item, float):
        if math.isnan(item):
            return "nan"
        if math.isinf(item):
            return "inf" if item > 0 else "-inf"
        return repr(item)
    if isinstance(item, str):
        return string(item)
    if isinstance(item, dict):
        if not item:
            return "{}"
        return "{ " + ", ".join(f"{key(k)} = {value(v, indent)}" for k, v in item.items()) + " }"
    if isinstance(item, list):
        inline = "[" + ", ".join(value(v, indent) for v in item) + "]"
        if len(inline) <= WIDTH and not any(isinstance(v, (dict, list)) for v in item):
            return inline
        inner = indent + "    "
        return "[\n" + "".join(f"{inner}{value(v, inner)},\n" for v in item) + indent + "]"
    raise TypeError(f"no TOML for {type(item).__name__}")


def is_section(item) -> bool:
    return isinstance(item, dict)


def is_sections(item) -> bool:
    return isinstance(item, list) and bool(item) and all(isinstance(v, dict) for v in item)


def document(doc: dict, header: str = "") -> str:
    """A document: its plain keys, then each array of tables (`[[name]]`), then each table - a table of tables as one
    section each (`[name.inner]`)."""
    out: list[str] = [header.rstrip()] if header else []
    plain = [(k, v) for k, v in doc.items() if not is_section(v) and not is_sections(v)]
    if plain:
        out.append("\n".join(f"{key(k)} = {value(v)}" for k, v in plain))
    for name, items in doc.items():
        if is_sections(items):
            for item in items:
                out.append(f"[[{key(name)}]]\n" + "\n".join(f"{key(k)} = {value(v)}" for k, v in item.items()))
    for name, table in doc.items():
        if not is_section(table):
            continue
        own = {k: v for k, v in table.items() if not is_section(v)}
        inner = {k: v for k, v in table.items() if is_section(v)}
        if own or not inner:
            out.append(f"[{key(name)}]\n" + "\n".join(f"{key(k)} = {value(v)}" for k, v in own.items()))
        for sub, fields in inner.items():
            out.append(f"[{key(name)}.{key(sub)}]\n" + "\n".join(f"{key(k)} = {value(v)}" for k, v in fields.items()))
    return "\n\n".join(part.rstrip() for part in out) + "\n"
