"""Shaped assets: data of a declared shape, one instance a group holds, written to the stored places it fills.

A shape is like a class: `shape/<family>.toml` declares it - its fields with their types, defaults and docs, the
shape it extends (`base`), and the outputs it is written to - and `shape/<family>.py` is its codec, which decodes
an instance from the stored bytes of those outputs and encodes it back exactly. An asset of type `shaped` names
its `shape` and, in `outputs`, the stored asset each output is:

    [item]
    type = "shaped"
    shape = "effect_item"
    outputs = { catalog = "catalog", info = "text" }    # another group's: { asset = "<path>" }

Its instance is `<group>/<local name>.toml` in the extraction, the form that is edited; it states its `shape` first,
which has to be its asset's. An asset with `include = true` is written as C by its codec (`c_source`), which the
build generates as `assets/<name>.inc` (tools/buildsteps/gen_shape_inc.py) for a source to include whole. The extractor writes it
from the outputs (and not their stored texts), and checks that it encodes back to them; the build takes an output
it compiles from the instance (tools/buildsteps/gen_shape_inc.py). An instance states a field only where it is not
the default.

    python3 tools/peassets/shape decode|check [--assets assets/USA] [--only NAME]
"""
from __future__ import annotations

import importlib
import json
import tomllib
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
FIELD_TYPES = ("text", "lines", "bool", "int", "enum", "record", "list", "table")


def declarations() -> dict[str, dict[str, Any]]:
    """Every declared shape and record by name, from shape/*.toml, each with the module of its family."""
    out: dict[str, dict[str, Any]] = {}
    for path in sorted(HERE.glob("*.toml")):
        for name, decl in tomllib.loads(path.read_text(encoding="utf-8")).items():
            if name in out:
                raise SystemExit(f"shape {name} is declared twice")
            out[name] = {**decl, "family": path.stem}
    return out


def fields(shape: str, decls: dict[str, dict[str, Any]]) -> dict[str, dict[str, Any]]:
    """A shape's fields, its bases' first."""
    decl = decls[shape]
    inherited = fields(decl["base"], decls) if "base" in decl else {}
    return {**inherited, **decl.get("fields", {})}


def outputs(shape: str, decls: dict[str, dict[str, Any]]) -> list[str]:
    decl = decls[shape]
    return decl.get("outputs") or (outputs(decl["base"], decls) if "base" in decl else [])


def codec(shape: str, decls: dict[str, dict[str, Any]]):
    return importlib.import_module(f"shape.{decls[shape]['family']}")


# --- instances: checked against their shape, defaults filled and left out ---

def _check_value(where: str, spec: dict[str, Any], value: Any, decls: dict) -> list[str]:
    kind = spec["type"]
    if kind == "text":
        return [] if isinstance(value, str) else [f"{where}: a text"]
    if kind == "bool":
        return [] if isinstance(value, bool) else [f"{where}: true or false"]
    if kind == "int":
        return [] if isinstance(value, int) and not isinstance(value, bool) else [f"{where}: a number"]
    if kind == "lines":
        if not (isinstance(value, list) and all(isinstance(v, str) for v in value)):
            return [f"{where}: a list of lines"]
        return [f"{where}: at most {spec['max']} lines"] if "max" in spec and len(value) > spec["max"] else []
    if kind == "enum":
        return [] if value in spec["values"] else [f"{where}: one of {', '.join(spec['values'])}"]
    if kind == "record":
        return check_record(where, spec["of"], value, decls)
    if kind == "table":            # keys to values, checked by the codec (a catalog's ids)
        return [] if isinstance(value, dict) else [f"{where}: a table"]
    if kind == "list":
        if not isinstance(value, list):
            return [f"{where}: a list"]
        of = spec["of"]
        element = ({"type": "enum", "values": of.split(":", 1)[1].split(",")} if of.startswith("enum:")
                   else {"type": of} if of in FIELD_TYPES else {"type": "record", "of": of})
        return [p for n, v in enumerate(value) for p in _check_value(f"{where}[{n}]", element, v, decls)]
    return [f"{where}: its shape states the unknown field type {kind!r}"]


def check_record(where: str, shape: str, value: Any, decls: dict) -> list[str]:
    if not isinstance(value, dict):
        return [f"{where}: a table of {shape}"]
    specs = fields(shape, decls)
    problems = [f"{where}: {shape} has no field {k!r}" for k in value if k not in specs]
    for key, spec in specs.items():
        if key in value:
            problems += _check_value(f"{where}.{key}", spec, value[key], decls)
        elif "default" not in spec:
            problems.append(f"{where}: {shape} needs {key!r}")
    return problems


def _of_records(spec: dict[str, Any]) -> bool:
    return not spec["of"].startswith("enum:") and spec["of"] not in FIELD_TYPES


def filled(shape: str, value: dict[str, Any], decls: dict) -> dict[str, Any]:
    """The record with every field, defaults where it states none, its records filled too."""
    out = {}
    for key, spec in fields(shape, decls).items():
        v = value.get(key, spec.get("default"))
        if spec["type"] == "record":
            v = filled(spec["of"], v or {}, decls)
        elif spec["type"] == "list" and _of_records(spec):
            v = [filled(spec["of"], e, decls) for e in v]
        out[key] = v
    return out


def stripped(shape: str, value: dict[str, Any], decls: dict) -> dict[str, Any]:
    """The record less every field at its default: what an instance states."""
    out = {}
    for key, spec in fields(shape, decls).items():
        v = value.get(key, spec.get("default"))
        if spec["type"] == "record":
            v = stripped(spec["of"], v or {}, decls)
        elif spec["type"] == "list" and _of_records(spec):
            v = [stripped(spec["of"], e, decls) for e in v]
        if "default" in spec and v == spec["default"]:
            continue
        out[key] = v
    return out


# --- TOML, as an instance is written ---

def _string(s: str) -> str:
    # Markup is backslashes: a literal string keeps it readable where it can hold the text.
    if "\\" in s and "'" not in s and not any(ord(c) < 0x20 for c in s):
        return f"'{s}'"
    return json.dumps(s, ensure_ascii=False)


def _inline(v: Any) -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, int):
        return str(v)
    if isinstance(v, str):
        return _string(v)
    if isinstance(v, list):
        return "[" + ", ".join(_inline(e) for e in v) + "]"
    if isinstance(v, dict):
        return "{ " + ", ".join(f"{k} = {_inline(e)}" for k, e in v.items()) + " }"
    raise TypeError(f"no TOML for {v!r}")


def _is_tables(v: Any) -> bool:
    return isinstance(v, list) and bool(v) and all(isinstance(e, dict) for e in v)


def _body(value: dict[str, Any], path: str) -> list[str]:
    """A table's keys: scalars and short lists inline, long lists one element a line, a table of more than a few
    keys as [path.key] and lists of tables as [[path.key]] after them, nested as deep as they go."""
    lines, tables, sections = [], [], []
    for key, v in value.items():
        if _is_tables(v):
            tables.append((key, v))
        elif isinstance(v, dict) and len(v) > 3:
            sections.append((key, v))
        elif isinstance(v, list) and len(v) > 2:
            lines += [f"{key} = ["] + [f"    {_inline(e)}," for e in v] + ["]"]
        else:
            lines.append(f"{key} = {_inline(v)}")
    for key, section in sections:
        name = f"{path}.{key}" if path else key
        lines += ["", f"[{name}]"] + [f"{k} = {_inline(v)}" for k, v in section.items()]
    for key, elements in tables:
        name = f"{path}.{key}" if path else key
        for e in elements:
            lines += ["", f"[[{name}]]"] + _body(e, name)
    return lines


def to_toml(value: dict[str, Any], header: list[str]) -> str:
    return "\n".join([f"# {h}" if h else "#" for h in header] + _body(value, "")) + "\n"


# --- assets ---

ROOT = HERE.parents[2]
GAMEPLAY_SYMBOLS = ROOT / "configs" / "USA" / "sym.gameplay.txt"


def shaped_assets(assets: dict) -> list[str]:
    """Every shaped asset, those whose codec reads other instances (a catalog) after the instances they read."""
    decls = declarations()
    return sorted((k for k, r in assets.items() if r.get("type") == "shaped"),
                  key=lambda k: (bool(decls.get(assets[k]["shape"], {}).get("reads_instances")), k))


def instance_path(name: str, assets: dict, assets_root: Path) -> Path:
    import asset_schema
    return assets_root / asset_schema.asset_dir(name, assets[name]) / f"{asset_schema.file_name(name)}.toml"


class Context:
    """What a codec may ask beyond its own bytes: where an embedded output sits in memory, gameplay's symbols by
    name and address (configs/USA/sym.gameplay.txt), and other shaped assets - their instances and outputs."""

    def __init__(self, assets: dict, embedded: dict, assets_root: Path):
        self.assets, self.embedded, self.assets_root = assets, embedded, assets_root
        self._symbols: dict[str, int] | None = None

    def address(self, name: str) -> int:
        import asset_schema
        return asset_schema.embedded_address(self.embedded[name])

    @property
    def symbols(self) -> dict[str, int]:
        if self._symbols is None:
            import re
            self._symbols = {m[1]: int(m[2], 16) for m in re.finditer(r"^(\w+) = (0x[0-9A-Fa-f]+);",
                                                                      GAMEPLAY_SYMBOLS.read_text(), re.M)}
        return self._symbols

    def symbol_at(self, address: int) -> str:
        names = [n for n, a in self.symbols.items() if a == address]
        if len(names) != 1:
            raise SystemExit(f"gameplay's symbol map names {len(names)} symbols at 0x{address:08X}")
        return names[0]

    def shaped(self) -> list[str]:
        return shaped_assets(self.assets)

    def instance(self, name: str) -> dict[str, Any]:
        return filled(self.assets[name]["shape"], read(name, self.assets, self.assets_root), declarations())

    def output_owner(self, output: str) -> tuple[str, str] | None:
        """(shaped asset, output) whose output this asset is, its own group's first."""
        owners = [(k, o) for k in self.shaped() for o, n in self.assets[k]["outputs"].items() if n == output]
        group = (self.assets.get(output) or {}).get("group")
        owners.sort(key=lambda kv: kv[0].rsplit("/", 1)[0] != group)
        return owners[0] if owners else None


PACKAGE_TYPES = ("pe2pkg", "room", "stage")
_INFLATED: dict[tuple[str, int], bytes] = {}


def stored_bytes(name: str, ctx: Context) -> bytes:
    """An output's stored bytes as the codec reads them: a text less its zero padding or NUL terminator, a package
    inflated to its image, any other type as stored. A package's image is the one the extraction wrote to
    `pe2pkg/<package>.pe2pkg` where it is there (inflated once per process), else inflated from the stored chunk."""
    import asset_schema
    from asset_decode import decode_ascii_payload
    from lzss import decode_lzss
    rec = ctx.embedded.get(name) or ctx.assets[name]
    if rec.get("type") in PACKAGE_TYPES:
        inflated = ctx.assets_root / "pe2pkg" / f"{rec.get('package_name', name.rsplit('/', 1)[-1])}.pe2pkg"
        path = inflated if inflated.exists() else ctx.assets_root / asset_schema.raw_rel(name, rec)
        key = (str(path), path.stat().st_mtime_ns)
        if key not in _INFLATED:
            data = path.read_bytes()
            _INFLATED[key] = data if path == inflated else decode_lzss(data)
        return _INFLATED[key]
    data = (ctx.assets_root / asset_schema.raw_rel(name, rec)).read_bytes()
    return decode_ascii_payload(data) if rec.get("type") == "txt" else data


def check_asset(name: str, rec: dict, assets: dict, decls: dict) -> list[str]:
    """The schema's check of a shaped asset's record."""
    shape = rec.get("shape")
    if shape not in decls or not outputs(shape, decls):           # a shape has outputs; a record has none
        return [f"{name}: shape must name a declared shape (tools/peassets/shape/*.toml)"]
    wanted = outputs(shape, decls)
    given = rec.get("outputs", {})
    problems = [f"{name}: {shape} has no output {k!r}" for k in given if k not in wanted]
    problems += [f"{name}: output {k} = {v!r} is not an asset" for k, v in given.items() if v not in assets]
    return problems


def _stored(name: str, ctx: Context) -> dict[str, bytes]:
    return {k: stored_bytes(v, ctx) for k, v in ctx.assets[name]["outputs"].items()}


def decode(name: str, assets: dict, embedded: dict, assets_root: Path) -> Path:
    """Write the instance from the stored outputs; it must encode back to them."""
    decls = declarations()
    ctx = Context(assets, embedded, assets_root)
    rec = assets[name]
    stored = _stored(name, ctx)
    value = codec(rec["shape"], decls).decode(rec["shape"], stored, decls, ctx, name)
    problems = check_record(name, rec["shape"], value, decls)
    if not problems:
        again = encode_value(rec["shape"], value, decls, ctx, name)
        problems += [f"{name}: {k} does not encode back from the instance" for k in stored if again.get(k) != stored[k]]
    if problems:
        raise SystemExit("\n".join(problems))
    path = instance_path(name, assets, assets_root)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(to_toml({"shape": rec["shape"], **stripped(rec["shape"], value, decls)},
                            [f"{name}: an instance of the shape {rec['shape']} (tools/peassets/shape/{decls[rec['shape']]['family']}.toml)."]),
                    encoding="utf-8")
    return path


def read(name: str, assets: dict, assets_root: Path) -> dict[str, Any]:
    """The instance's fields. It states its `shape` first, which must be its asset's."""
    decls = declarations()
    path = instance_path(name, assets, assets_root)
    value = tomllib.loads(path.read_text(encoding="utf-8"))
    stated = value.pop("shape", None)
    if stated != assets[name]["shape"]:
        raise SystemExit(f"{path}: states shape = {stated!r}; its asset {name} has the shape {assets[name]['shape']!r}")
    problems = check_record(str(path), assets[name]["shape"], value, decls)
    if problems:
        raise SystemExit("\n".join(problems))
    return value


def encode_value(shape: str, value: dict[str, Any], decls: dict, ctx: Context, name: str) -> dict[str, bytes]:
    return codec(shape, decls).encode(shape, filled(shape, value, decls), decls, ctx, name)


def encode(name: str, assets: dict, embedded: dict, assets_root: Path) -> dict[str, bytes]:
    """Each output's bytes from the instance; a text without padding or terminator."""
    rec = assets[name]
    ctx = Context(assets, embedded, assets_root)
    out = encode_value(rec["shape"], read(name, assets, assets_root), declarations(), ctx, name)
    return {k: out[k] for k in rec["outputs"]}


def c_string(data: bytes) -> list[str]:
    """Bytes as C string literals, a line of the text per literal; the caller's array adds the terminator."""
    def literal(line: bytes) -> str:
        return '"' + "".join(chr(b) if 0x20 <= b < 0x7F and b not in b'"\\' else
                             {0x22: '\\"', 0x5C: "\\\\", 0x0A: "\\n"}.get(b, f"\\{b:03o}") for b in line) + '"'
    lines = data.split(b"\n")
    return [literal(line + b"\n") for line in lines[:-1]] + ([literal(lines[-1])] if lines[-1] else []) or ['""']


def c_source(name: str, assets: dict, embedded: dict, assets_root: Path) -> str:
    """The C a shaped asset's codec writes from its instance (`include = true`): the build includes it whole."""
    decls = declarations()
    rec = assets[name]
    ctx = Context(assets, embedded, assets_root)
    value = filled(rec["shape"], read(name, assets, assets_root), decls)
    return codec(rec["shape"], decls).c_source(rec["shape"], value, decls, ctx, name)


def check(name: str, assets: dict, embedded: dict, assets_root: Path) -> list[str]:
    ctx = Context(assets, embedded, assets_root)
    stored = _stored(name, ctx)
    mine = encode(name, assets, embedded, assets_root)
    return [f"{name}: {k} differs from the stored {assets[name]['outputs'][k]}" for k in stored if mine[k] != stored[k]]
