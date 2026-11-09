#!/usr/bin/env python3
"""Write a layer's nodes from the declarations: every asset's `node.toml`, every disc layout's, and the layer's
changeset - what the editor reads of a game layer before anything is extracted into it.

    init.py nodes --registry REGISTRY.json --layer DIR [--release USA ...] [--declarations DIR]
    init.py plan --registry REGISTRY.json --layer DIR [--release USA ...] [--declarations DIR]
    init.py compare --layer DIR --against DIR

`nodes` writes `<path>.node/node.toml` under a folder and its changeset beside it (`<folder>.toml`) - a tree the editor
imports as a layer's records (`pe2-cli project layer import`, as `tools/pe2project.py` does); the
registry is what the project's extensions offer (`pe2-cli project registry --project <project>`). `plan` writes the
layer's extraction plan (`plan.py`), which the editor runs to fill the bundles from the discs. `compare` (a records
layer exported as a tree first, `pe2-cli project layer export`) says
where two layers' nodes differ as TOML read (their tables, not their text), and where their changesets list other
assets - a folder's uid is the layer's own, not compared.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
import tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / "buildsteps"))
import declarations as declared  # noqa: E402
import nodes  # noqa: E402
import plan as plans  # noqa: E402
import toml_out  # noqa: E402

NODE = ".node"
HEADER = "# A node, written by extraction."
LAYOUT_HEADER = "# A node, written by extraction: the layout and its slots."
CHANGESET = "# A layer's changeset: what it creates, and what it does to the layers below."
DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz"


def uid_text(n: int) -> str:
    out = ""
    while True:
        out = DIGITS[n % 36] + out
        n //= 36
        if n == 0:
            return "uid://" + out


def derived_uid(path: str, taken: set[str]) -> str:
    """A folder's uid, the same for the same path: 63 bits of a hash of it, drawn again where held."""
    salt = 0
    while True:
        digest = hashlib.sha1(f"folder:{path}:{salt}".encode()).digest()
        uid = uid_text(int.from_bytes(digest[:8], "big") >> 1)
        if uid not in taken:
            return uid
        salt += 1


def layer_nodes(decl: declared.Declarations, registry: dict, releases: list[str]) -> dict[str, tuple[str, dict]]:
    """Every node of the layer by its path: its uid and its table."""
    made: dict[str, tuple[str, dict]] = {}
    for path, d in decl.by_path.items():
        layout = path.startswith("disc/") and path.count("/") == 2
        if layout:
            if path.split("/")[1] in releases:
                made[path] = (d.uid, nodes.layout_node(d.table, registry))
        else:
            made[path] = (d.uid, nodes.asset_node(path, d.table, registry, releases))
    return made


def write_nodes(layer: Path, made: dict[str, tuple[str, dict]]) -> None:
    # A bundle no longer declared goes: the layer's nodes are the declarations' alone.
    wanted = {f"{path}{NODE}" for path in made}
    if layer.is_dir():
        for folder in sorted(layer.rglob(f"*{NODE}")):
            if folder.is_dir() and folder.relative_to(layer).as_posix() not in wanted:
                __import__("shutil").rmtree(folder, ignore_errors=True)
    entries: dict[str, str] = {}
    for path, (uid, node) in sorted(made.items()):
        folder = layer / f"{path}{NODE}"
        folder.mkdir(parents=True, exist_ok=True)
        header = LAYOUT_HEADER if path.startswith("disc/") and path.count("/") == 2 else HEADER
        (folder / "node.toml").write_text(toml_out.document(node, header), encoding="utf-8")
        entries[uid] = f"{path}{NODE}"
    folders = sorted({parent for at in entries.values() for parent in parents(at)})
    taken = set(entries)
    for folder in folders:
        uid = derived_uid(folder, taken)
        taken.add(uid)
        entries[uid] = folder
    lines = [CHANGESET, "read_only = true", "", "[entries]"]
    lines += [f"{toml_out.string(uid)} = {toml_out.string(at)}" for uid, at in sorted(entries.items(), key=lambda e: e[1])]
    layer.with_suffix(".toml").write_text("\n".join(lines) + "\n", encoding="utf-8")


def parents(path: str):
    while "/" in path:
        path = path.rsplit("/", 1)[0]
        yield path


def read_nodes(layer: Path) -> dict[str, dict]:
    return {
        file.parent.relative_to(layer).as_posix()[: -len(NODE)]: tomllib.loads(file.read_text(encoding="utf-8"))
        for file in layer.rglob(f"*{NODE}/node.toml")
    }


def read_entries(changeset: Path) -> dict[str, str]:
    entries = tomllib.loads(changeset.read_text(encoding="utf-8")).get("entries", {})
    return {uid: at for uid, at in entries.items() if at.endswith(NODE)}


def differences(ours, theirs, at: str = "") -> list[str]:
    if isinstance(ours, dict) and isinstance(theirs, dict):
        found = []
        for k in sorted(set(ours) | set(theirs)):
            if k not in ours:
                found.append(f"{at}.{k}: missing")
            elif k not in theirs:
                found.append(f"{at}.{k}: extra")
            else:
                found += differences(ours[k], theirs[k], f"{at}.{k}")
        return found
    if isinstance(ours, list) and isinstance(theirs, list) and len(ours) == len(theirs):
        return [d for i, (a, b) in enumerate(zip(ours, theirs)) for d in differences(a, b, f"{at}[{i}]")]
    return [] if ours == theirs else [f"{at}: {json.dumps(ours)[:120]} against {json.dumps(theirs)[:120]}"]


def as_tree(layer: Path, keep: list) -> Path:
    """A layer as a tree of files: itself, or - a records layer - written out by the editor's command
    (`pe2-cli project layer export`) into a temporary folder, removed when `keep` goes."""
    if not (layer / ".records" / "layer.toml").is_file():
        return layer
    import subprocess
    import tempfile
    sys.path.insert(0, str(HERE.parent))
    from peassets.project import dist_cli  # noqa: E402
    cli = dist_cli()
    if cli is None:
        raise SystemExit(f"{layer}: a records layer is read through the editor's command: lay it out (cargo xtask dist)")
    out = tempfile.TemporaryDirectory(prefix="pe2-layer-")
    keep.append(out)
    tree = Path(out.name) / layer.name
    project = layer.parent.parent
    subprocess.run([str(cli), "project", "layer", "export", layer.name, "--to", str(tree), "--project", str(project)],
                   check=True, stdout=subprocess.DEVNULL)
    return tree


def compare(layer: Path, against: Path) -> int:
    keep: list = []
    layer, against = as_tree(layer, keep), as_tree(against, keep)
    ours, theirs = read_nodes(layer), read_nodes(against)
    problems = [f"{p}: only in {against}" for p in sorted(set(theirs) - set(ours))]
    problems += [f"{p}: only in {layer}" for p in sorted(set(ours) - set(theirs))]
    for path in sorted(set(ours) & set(theirs)):
        problems += [f"{path}{d}" for d in differences(ours[path], theirs[path])]
    a, b = read_entries(layer.with_suffix(".toml")), read_entries(against.with_suffix(".toml"))
    problems += [f"changeset: {uid} = {a.get(uid)} against {b.get(uid)}" for uid in sorted(set(a) | set(b)) if a.get(uid) != b.get(uid)]
    for problem in problems[:60]:
        print(problem)
    print(f"{len(ours)} nodes against {len(theirs)}: {len(problems)} differences")
    return 1 if problems else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    write = sub.add_parser("nodes")
    write.add_argument("--registry", type=Path, required=True)
    write.add_argument("--layer", type=Path, required=True)
    write.add_argument("--release", action="append", default=[])
    write.add_argument("--declarations", type=Path, default=declared.DECLARATIONS)
    planned = sub.add_parser("plan")
    planned.add_argument("--registry", type=Path, required=True)
    planned.add_argument("--out", type=Path, required=True, help="the plan's file, beside the layer it fills")
    planned.add_argument("--release", action="append", default=[])
    planned.add_argument("--declarations", type=Path, default=declared.DECLARATIONS)
    check = sub.add_parser("compare")
    check.add_argument("--layer", type=Path, required=True)
    check.add_argument("--against", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "compare":
        return compare(args.layer, args.against)
    registry = json.loads(args.registry.read_text(encoding="utf-8"))
    decl = declared.Declarations(args.declarations)
    # USA, which the decompilation builds, first: the release a plan prefers.
    found = sorted(p.name for p in (args.declarations / "disc").iterdir() if p.is_dir())
    releases = args.release or [r for r in ["USA"] if r in found] + [r for r in found if r != "USA"]
    if args.command == "plan":
        made = plans.make(decl, registry, releases)
        args.out.write_text(made.text(), encoding="utf-8")
        print(f"{len(made.ops)} operations, {len(made.emits)} bundles given files")
        return 0
    write_nodes(args.layer, layer_nodes(decl, registry, releases))
    return 0


if __name__ == "__main__":
    sys.exit(main())
