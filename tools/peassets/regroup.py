#!/usr/bin/env python3
"""Bring an extraction into line with the groups in the asset table, without extracting again.

    regroup.py [--assets assets/USA] [--table asset_data.py] [--dry-run] [-v]

An asset's `group` ("weapons/m4a1") files its decoded form under that subtree of the extraction; an asset
without one is under its type's directory (`asset_schema.asset_dir` is the rule, and states what never moves).
The extractor follows the table as it writes. This does the same to a tree that is already extracted: it moves
each asset's decoded files to where the table says now - from its type's directory into a group, from a group
back, or from one group to another - and then repairs what records those places:

* a bank's `meta.json`, which names each of its samples by the path from the bank's directory;
* `stages.json`, whose `path` of each chunk is its decoded file.

Nothing under `raw/` is touched, nor `pe2pkg/`, the streams or the pack sidecars. A file that is already in
place is left alone, so a second run does nothing. A file whose destination holds different bytes is not moved
and the run ends with status 1; one whose destination holds the same bytes is a leftover, and is removed. A
group directory the run emptied is removed, with every directory above it that it leaves empty; a type's
directory stays.

`--dry-run` reports what would be done and changes nothing.
"""
from __future__ import annotations

import argparse
import filecmp
import json
import os
import shutil
import sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import asset_schema  # noqa: E402

ROOT = HERE.parent.parent


def same(a: Path, b: Path) -> bool:
    """Whether two files, or two directories all the way down, hold the same bytes."""
    if a.is_dir() != b.is_dir():
        return False
    if not a.is_dir():
        return filecmp.cmp(a, b, shallow=False)
    names = sorted(p.name for p in a.iterdir())
    return names == sorted(p.name for p in b.iterdir()) and all(same(a / n, b / n) for n in names)


class Extraction:
    """What an extraction holds outside the directories that never move, by entry name."""

    def __init__(self, root: Path, tables: tuple[dict, dict]):
        self.root = root
        self.tables = tables
        reserved = asset_schema.reserved_roots(tables)
        # A directory that is an asset: a bank or the save header, decoded as `<name>/meta.json`.
        self.directories = {key for table in tables for key, rec in table.items()
                            if asset_schema.stored_ext(rec) in asset_schema.DIRECTORY_EXTS}
        # type directory -> the names in it
        self.in_type: dict[str, set[str]] = {}
        for type_dir in sorted(reserved - asset_schema.EXTRACTION_ROOTS - asset_schema.PINNED_TYPE_DIRS):
            if (root / type_dir).is_dir():
                self.in_type[type_dir] = {p.name for p in (root / type_dir).iterdir()}
        # entry name -> the group directories holding one
        self.in_group: dict[str, list[str]] = {}
        self.group_dirs: list[str] = []
        for top in sorted(root.iterdir()) if root.is_dir() else []:
            if top.is_dir() and top.name not in reserved and not asset_schema.STAGE_ROOT.match(top.name):
                self._walk(top)

    def _walk(self, folder: Path) -> None:
        rel = folder.relative_to(self.root).as_posix()
        self.group_dirs.append(rel)
        for entry in sorted(folder.iterdir()):
            if entry.is_dir() and not (entry.name in self.directories and (entry / asset_schema.SPK_META).is_file()):
                self._walk(entry)
            else:
                self.in_group.setdefault(entry.name, []).append(rel)

    def holders(self, name: str, type_dir: str) -> list[str]:
        """Every directory an entry of this name, of this type, is found in."""
        out = [type_dir] if name in self.in_type.get(type_dir, ()) else []
        return out + self.in_group.get(name, [])


def plan_moves(found: Extraction) -> tuple[list[tuple[str, str, str]], int]:
    """(asset, from, to) of every entry that is not where the table files it, and how many already are."""
    moves, settled = [], 0
    for table in found.tables:
        for key, rec in table.items():
            ext = asset_schema.stored_ext(rec)
            type_dir = asset_schema.type_dir_of(rec)
            if ext in asset_schema.RAW_ONLY_EXTS or type_dir in asset_schema.PINNED_TYPE_DIRS:
                continue
            home = asset_schema.asset_dir(key, rec, tables=found.tables)
            for name in asset_schema.decoded_names(key, ext):
                for holder in found.holders(name, type_dir):
                    if holder == home:
                        settled += 1
                    else:
                        moves.append((key, f"{holder}/{name}", f"{home}/{name}"))
    return moves, settled


def repair_banks(root: Path, tables: tuple[dict, dict], moved: dict[str, str], stuck: dict[str, str],
                 write: bool) -> list[str]:
    """Banks whose `meta.json` names a sample by a path that is no longer the way to it.

    `moved` maps where an entry will be to where it is now: a dry run reads the bank from the place it has
    not left yet. `stuck` maps an entry that could not be moved to the directory it is still in.
    """
    assets, _embedded = tables
    changed = []
    for key, rec in assets.items():
        if rec.get("type") != "spk":
            continue
        bank_dir = f"{stuck.get(key) or asset_schema.asset_dir(key, rec, tables=tables)}/{key}"
        meta_path = root / (bank_dir if write else moved.get(bank_dir, bank_dir)) / asset_schema.SPK_META
        if not meta_path.is_file():
            continue
        meta = json.loads(meta_path.read_text(encoding="utf-8"))
        dirty = False
        for slot in meta.get("samples") or []:
            if "name" not in slot:
                continue                                     # a bank the table did not know: samples beside it
            sample = f"{slot['name']}.wav"
            sample = f"{stuck[sample]}/{sample}" if sample in stuck else asset_schema.sample_rel(slot["name"], tables)
            want = asset_schema.bank_sample_file(bank_dir, sample)
            if slot.get("file") != want:
                slot["file"], dirty = want, True
        if dirty:
            changed.append(f"{bank_dir}/{asset_schema.SPK_META}")
            if write:
                replace_text(meta_path, json.dumps(meta, indent=2) + "\n")
    return changed


def repair_stages(root: Path, tables: tuple[dict, dict], stuck: dict[str, str], write: bool) -> list[tuple[str, str]]:
    """(old, new) of every `path` in `stages.json` that is not where the table files the asset.

    An entry that could not be moved (`stuck`) keeps the path it has, which is still where it is.
    """
    stages = root / "stages.json"
    if not stages.is_file():
        return []
    data = json.loads(stages.read_text(encoding="utf-8"))
    changed: list[tuple[str, str]] = []

    def walk(node) -> None:
        if isinstance(node, dict):
            if isinstance(node.get("path"), str) and isinstance(node.get("type"), str):
                want = asset_schema.manifest_path(node["path"], node["type"], tables)
                if want != node["path"] and node["path"].split("/")[-1] not in stuck and (
                        asset_schema.manifest_asset(node["path"]) or ("",))[0] not in stuck:
                    changed.append((node["path"], want))
                    node["path"] = want
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            for value in node:
                walk(value)

    walk(data)
    if changed and write:
        replace_text(stages, json.dumps(data, indent=2) + "\n")           # as write_manifest.dump_json writes it
    return changed


def replace_text(path: Path, text: str) -> None:
    """Rewrite a file so that a reader sees the old text or the new, never part of one."""
    tmp = path.with_name(path.name + ".regroup")
    tmp.write_text(text, encoding="utf-8")
    os.replace(tmp, path)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--assets", type=Path, default=ROOT / "assets" / "USA", help="the root of the extraction")
    ap.add_argument("--table", type=Path, help="read the asset table from this file instead of asset_data.py")
    ap.add_argument("--dry-run", action="store_true", help="report what would be done; change nothing")
    ap.add_argument("-v", "--verbose", action="store_true", help="list every move")
    args = ap.parse_args()
    if args.table:
        asset_schema.use_table(args.table)
    tables = asset_schema.load()
    root = args.assets
    if not (root / asset_schema.RAW_ROOT).is_dir():
        print(f"{root} is not an extraction: it has no {asset_schema.RAW_ROOT}/", file=sys.stderr)
        return 1
    problems = asset_schema.layout_problems(*tables)
    if problems:
        print("\n".join(problems[:40]), file=sys.stderr)
        print(f"{len(problems)} problem(s) with the groups in the asset table; nothing moved", file=sys.stderr)
        return 1

    found = Extraction(root, tables)
    moves, settled = plan_moves(found)
    write = not args.dry_run
    would = "" if write else "would be "
    counts: Counter = Counter()
    conflicts: list[str] = []
    left: set[str] = set()
    moved: dict[str, str] = {}
    stuck: dict[str, str] = {}                       # entry name -> the directory it could not leave
    type_dirs = asset_schema.reserved_roots(tables)
    # What each group directory holds as the moves are made, so that a dry run knows which it would empty.
    holds: dict[str, set[str]] = {rel: {p.name for p in (root / rel).iterdir()} for rel in found.group_dirs}
    for key, source, target in moves:
        src, dst = root / source, root / target
        source_dir, name = source.rsplit("/", 1)
        target_dir = target.rsplit("/", 1)[0]
        if dst.exists() or dst.is_symlink():
            if not same(src, dst):
                conflicts.append(f"{key}: {source} is not moved, because {target} exists and differs")
                stuck[name] = source_dir
                continue
            counts["leftover"] += 1
            if args.verbose or not write:
                print(f"remove {source} (the same as {target})")
            if write:
                shutil.rmtree(src) if src.is_dir() and not src.is_symlink() else src.unlink()
        else:
            counts["to a group" if target_dir not in type_dirs else "back to a type directory"] += 1
            moved[target] = source
            if args.verbose or not write:
                print(f"{source} -> {target}")
            if write:
                dst.parent.mkdir(parents=True, exist_ok=True)
                os.replace(src, dst)
            if target_dir not in type_dirs:
                parts = target_dir.split("/")
                for n in range(len(parts), 0, -1):          # the group directory, and those made above it
                    holds.setdefault("/".join(parts[:n]), set()).add(name if n == len(parts) else parts[n])
        if source_dir not in type_dirs:
            holds[source_dir].discard(name)
            left.add(source_dir)

    removed: list[str] = []
    for folder in sorted(left, key=lambda rel: -rel.count("/")):
        while folder and folder not in type_dirs and not holds.get(folder) and folder not in removed:
            if write:
                try:
                    (root / folder).rmdir()
                except OSError:
                    break
            removed.append(folder)
            parent, _, name = folder.rpartition("/")
            holds.get(parent, set()).discard(name)
            folder = parent

    banks = repair_banks(root, tables, moved, stuck, write)
    stages = repair_stages(root, tables, stuck, write)
    if args.verbose or not write:
        for folder in removed:
            print(f"remove the empty directory {folder}/")
        for path in banks:
            print(f"rewrite the sample paths in {path}")
        for old, new in stages:
            print(f"stages.json: {old} -> {new}")

    groups = sorted({g for table in tables for key, rec in table.items() if (g := asset_schema.group_of(key, rec))})
    print(f"{root}: {len(groups)} group{'s' if len(groups) != 1 else ''} in the asset table"
          + (f" ({', '.join(groups[:6])}{', ...' if len(groups) > 6 else ''})" if groups else ""))
    print(f"  {settled} decoded entr{'ies' if settled != 1 else 'y'} already in place")
    print(f"  {counts['to a group']} {would}moved to a group, {counts['back to a type directory']} {would}moved back "
          f"to a type directory, {counts['leftover']} identical leftover{'s' if counts['leftover'] != 1 else ''} "
          f"{would}removed")
    print(f"  {len(removed)} emptied group director{'ies' if len(removed) != 1 else 'y'} {would}removed")
    print(f"  {len(banks)} bank meta.json {would}rewritten, {len(stages)} path{'s' if len(stages) != 1 else ''} in "
          f"stages.json {would}rewritten")
    if conflicts:
        print("\n".join(conflicts), file=sys.stderr)
        print(f"{len(conflicts)} entr{'ies' if len(conflicts) != 1 else 'y'} not moved", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
