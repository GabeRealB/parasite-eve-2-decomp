#!/usr/bin/env python3
"""Rename assets, moving them between groups, and carry every reference along.

    rename_assets.py MAPPING [--dry-run]

MAPPING is a text file of `old new` pairs, one a line (`#` starts a comment): full asset names, each new one under
the group its record will have - the name less its last part. A group named by no asset any more is gone, and a new
one is made by naming assets in it.

What it rewrites:

* the asset table: each record's key and `group` - an asset's, or an embedded record's that has no asset (a
  camera path, a palette) - and every name the table holds - in other records' fields (a
  bank's clips, a binding's model and textures, a file's contents, a package's slots and embedded placements, a
  texture's variant as `<name>.<variant>`) and in the tree's chunks - written back through dump_asset_db's own
  writer, so asset_data.py and the group files are what a dump would write;
* every other tracked text file naming one - usage records, notes, residency, sources and docs - where the name
  stands whole: not part of a longer name, a name with a variant or an extension after it counts;
* the generated includes sources name by path: `assets/<name>.inc` and `.h`, an include named after an asset with a
  suffix (a package's clip runs: `<local>_clips_1.inc`, `clips_1.inc` for a group's `package`), and a room's or a
  stage's module includes, which are in its package's group (room.own_dir);
* the C names the build derives from an asset's name, where they change: a clip's (gen_animation_source.ident)
  and the names of its parts, everywhere they are written (a source's forward declaration, a symbol map), and a
  schema's constants (schema_prefix) in the sources that include its header.

It does not touch the extractions: extract again (`config.py editor --fresh` for the project, `config.py splat
extract` for the build's), which lays every asset, instance and
manifest out by the table, then rebuild with --clean - a renamed include's old file stays in build/include and would
hide a source that still names it.

It refuses a mapping whose old name is no asset, whose new name is taken by an asset that is not renamed away, or
that names one asset twice.
"""
from __future__ import annotations

import argparse
import copy
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))

TABLE_FILES = {"tools/peassets/asset_data.py"}           # written through dump_asset_db
TABLE_DIRS = ("tools/peassets/groups/",)
TEXT_SUFFIXES = {".py", ".c", ".h", ".tsv", ".toml", ".md", ".txt", ".json", ".yaml", ".evs", ".sh", ".s"}


def read_mapping(path: Path) -> dict[str, str]:
    out: dict[str, str] = {}
    for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        cells = line.split()
        if len(cells) != 2:
            raise SystemExit(f"{path}:{n}: a line is `old new`")
        if cells[0] in out:
            raise SystemExit(f"{path}:{n}: {cells[0]} is renamed twice")
        out[cells[0]] = cells[1]
    if len(set(out.values())) != len(out):
        raise SystemExit(f"{path}: two assets are given one name")
    return {old: new for old, new in out.items() if old != new}


def check(mapping: dict[str, str], assets: dict, embedded: dict) -> list[str]:
    problems = [f"{old}: no such asset" for old in mapping if old not in assets and old not in embedded]
    problems += [f"{new}: already an asset" for new in mapping.values()
                 if (new in assets or new in embedded) and new not in mapping]
    problems += [f"{new}: a name is a group and a local name" for new in mapping.values() if "/" not in new]
    return problems


def _renamer(mapping: dict[str, str]):
    """(the string with any old name standing whole in it renamed, for table values: exact, or with a variant)."""
    alternatives = "|".join(re.escape(o) for o in sorted(mapping, key=len, reverse=True))
    whole = re.compile(rf"(?<![\w-])(?<![\w/-]\.)({alternatives})(?![\w/])")   # `a..b` ranges too
    value = re.compile(rf"^({alternatives})(\.[\w.]+)?$")
    return (lambda s: whole.sub(lambda m: mapping[m.group(1)], s),
            lambda s: value.sub(lambda m: mapping[m.group(1)] + (m.group(2) or ""), s))


def rename_value(v, in_value):
    if isinstance(v, str):
        return in_value(v)
    if isinstance(v, list):
        return [rename_value(e, in_value) for e in v]
    if isinstance(v, tuple):
        return tuple(rename_value(e, in_value) for e in v)
    if isinstance(v, dict):
        return {(in_value(k) if isinstance(k, str) else k): rename_value(e, in_value) for k, e in v.items()}
    return v


def rename_table(mapping: dict[str, str], assets: dict, tree: dict, embedded: dict) -> tuple[dict, dict, dict]:
    _in_text, in_value = _renamer(mapping)
    out = {}
    for key, rec in assets.items():
        rec = rename_value(copy.deepcopy(rec), in_value)
        if key in mapping:
            key = mapping[key]
            rec["group"] = key.rsplit("/", 1)[0]
        out[key] = rec
    placed = rename_value(copy.deepcopy(embedded), in_value)   # an embedded record is keyed by its name
    for old, new in mapping.items():
        if new in placed and "group" in placed[new]:
            placed[new]["group"] = new.rsplit("/", 1)[0]
    return out, rename_value(copy.deepcopy(tree), in_value), placed


def _derived(name: str, suffix: str) -> str:
    """The include an asset's generator names with a suffix: a package's clip runs are `<local>_clips<n>`, or
    `clips<n>` for a group's `package` and `<variant>_clips<n>` for its `<variant>_package` (tools/config/splat/configure.py)."""
    group, local = name.rsplit("/", 1)
    if local == "package":
        return f"{group}/{suffix.lstrip('_')}"
    return f"{group}/{local.removesuffix('_package')}{suffix}"


def include_renamer(mapping: dict[str, str], assets: dict, groups: dict[str, str]):
    """`#include "assets/..."` paths: an asset's own, one derived from an asset's name, a room's module include."""
    derived = {}
    for old, new in mapping.items():
        for suffix in ("_clips", "_bank", "_schema", "_sounds"):
            derived[_derived(old, suffix)] = _derived(new, suffix)
    pattern = re.compile(r'(#include\s+"assets/)([\w/.-]+?)(\.(?:inc|h))"')

    def sub(m: re.Match) -> str:
        path, run = m.group(2), ""
        if path not in mapping and (r := re.fullmatch(r"(.+?)(_\d+)", path)) and r.group(1) in derived:
            path, run = r.groups()                            # a numbered run of a derived include
        if path in mapping:
            path = mapping[path]
        elif path in derived:
            path = derived[path]
        else:
            group, file = path.rsplit("/", 1) if "/" in path else ("", path)
            if group in groups and path not in assets:
                path = f"{groups[group]}/{file}"              # a room's module, in its package's group
        return f'{m.group(1)}{path}{run}{m.group(3)}"'
    return lambda s: pattern.sub(sub, s)


def instance_dirs(mapping: dict[str, str], old: dict, new: dict) -> dict[str, str]:
    """{old directory: new} of the rooms', stages' and actors' module includes (room.own_dir) that move with their
    package."""
    import room
    return {room.own_dir(o, old): room.own_dir(n, new) for o, n in mapping.items()
            if o in old and room.kind_of(o, old) and room.own_dir(o, old) != room.own_dir(n, new)}


def derived_names(mapping: dict[str, str], assets: dict, renamed: dict) -> tuple[dict[str, str], dict[str, tuple[str, str]]]:
    """({old clip C name: new}, {header: (old constant prefix, new)}) of the renamed assets whose derived C names
    change - a schema's, and any sound bank's, whose prefix depends on every bank's name (gen_sound_header.prefixes);
    a clip that states its `symbol` keeps it."""
    sys.path.insert(0, str(ROOT / "tools" / "config" / "splat"))
    from gen_animation_source import ident, schema_prefix
    from gen_sound_header import prefixes
    clips, schemas = {}, {}
    for old, new in mapping.items():
        rec = assets.get(old, {})
        if rec.get("type") == "clip" and not rec.get("symbol") and ident(old) != ident(new):
            clips[ident(old)] = ident(new)
        if rec.get("type") == "animation_schema" and schema_prefix(old) != schema_prefix(new):
            schemas[f'"assets/{new}.h"'] = (schema_prefix(old), schema_prefix(new))
    before, after = prefixes(assets), prefixes(renamed)
    back = {new: old for old, new in mapping.items()}
    for bank, p in after.items():
        if before.get(back.get(bank, bank)) not in (None, p):
            schemas[f'"assets/{bank}.h"'] = (before[back.get(bank, bank)], p)
    return clips, schemas


def tracked_texts() -> list[Path]:
    files = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True).stdout.split("\n")
    return [ROOT / f for f in files if f and Path(f).suffix in TEXT_SUFFIXES and f not in TABLE_FILES
            and not f.startswith(TABLE_DIRS) and (ROOT / f).is_file()]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mapping", type=Path)
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    import asset_data
    import dump_asset_db
    mapping = read_mapping(args.mapping)
    problems = check(mapping, asset_data.ASSETS, asset_data.EMBEDDED_ASSETS)
    if problems:
        print("\n".join(problems), file=sys.stderr)
        return 1
    assets, tree, embedded = rename_table(mapping, asset_data.ASSETS, asset_data.TREE, asset_data.EMBEDDED_ASSETS)
    moved_groups = instance_dirs(mapping, asset_data.ASSETS, assets)
    in_text, _in_value = _renamer(mapping)
    in_includes = include_renamer(mapping, asset_data.ASSETS, moved_groups)
    clips, schemas = derived_names(mapping, asset_data.ASSETS, assets)
    clip_names = re.compile(r"\b(" + "|".join(map(re.escape, sorted(clips, key=len, reverse=True))) + r")(Records|TrackStarts|Bank\d+)?\b") if clips else None
    changed = []
    for path in tracked_texts():
        text = path.read_text(encoding="utf-8", errors="surrogateescape")
        new = in_text(in_includes(text)) if path.suffix in (".c", ".h") else in_text(text)
        if clip_names:                                  # a clip's C name, and its generated parts' (...ClipRecords)
            new = clip_names.sub(lambda m: clips[m.group(1)] + (m.group(2) or ""), new)
        for header, (old_prefix, new_prefix) in schemas.items():
            if path.suffix in (".c", ".h") and header in new:
                new = re.sub(rf"\b{old_prefix}", new_prefix, new)
        if new != text:
            changed.append(path)
            if not args.dry_run:
                path.write_text(new, encoding="utf-8", errors="surrogateescape")
    print(f"{len(mapping)} assets renamed; {len(changed)} other files name them")
    for path in changed:
        print(f"  {path.relative_to(ROOT)}")
    if args.dry_run:
        return 0
    dump_asset_db.write_module(assets, tree, embedded)
    print("extract again (config.py editor --fresh; config.py splat extract), then build with --clean")
    return 0


if __name__ == "__main__":
    sys.exit(main())
