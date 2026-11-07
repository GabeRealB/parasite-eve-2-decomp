#!/usr/bin/env python3
"""Make projects from the repository's declarations, and fill their game layer.

    pe2project.py create [DIR] [--release R ...] [--roms PATH] [--no-extract] [--no-extensions]
    pe2project.py init [--release R ...] [--roms PATH] [--no-extract] [--no-extensions]
    pe2project.py extract [--project DIR] [--roms PATH]

`create` makes a new project in DIR (default `project/` at the repository root; it must not exist or be empty), `init`
makes the current directory one. Either writes `project.toml` - the releases it is built for with the discs each needs
(names, sizes and sha1s, read off the images), the game layer `.layers/game` and the project's own `.layers/project`,
the `roms` folder, a read-only mount of this checkout's `packages/`, a configuration per release - realizes the mount
as a link, writes the `.gitignore` that keeps the game's content out (the game layer and its changeset, `roms`, the
links), adds the extensions the project builds and plays its movies with (`pe2.psx`, `pe2.ffmpeg`, by `pe2-cli project
extension add`), and then extracts.

`extract` writes the game layer from the declarations (`editor_project/`): every asset's node, the layer's changeset and
its extraction plan - what the editor reads of the layer before anything is extracted - and then has the editor run the
plan (`pe2-cli project extract`), which reads the discs in `roms` and makes only what changed since it last ran. The
nodes are made by what the project's extensions offer (`pe2-cli project registry`).

The editor comes from `cargo xtask dist`, laid out again first where Rust is installed; without Rust and without one laid
out, the project is made without extensions or a game layer, saying how to make them.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tools" / "editor_project"))
sys.path.insert(0, str(ROOT / "tools" / "buildsteps"))
import declarations as declared  # noqa: E402
import init as layer_init  # noqa: E402
import plan as plans  # noqa: E402
import project_lock  # noqa: E402
import toml_out  # noqa: E402

EDITOR = ROOT / "editor"
# What every project holds: the PlayStation target its packages build for, and ffmpeg, its movies' video files.
EXTENSIONS = ["pe2.psx", "pe2.ffmpeg"]
LAYER = ".layers/game"             # the game layer; its changeset beside it, `.layers/game.toml`
OWN = ".layers/project"             # the project's own layer
MOUNTS = [("packages", ROOT / "packages", True)]
IGNORE = [f"/{LAYER}/", f"/{LAYER}.toml", "/roms/", "/build/", "/project.local.toml"] + [f"/{at}" for at, _p, _r in MOUNTS]


def releases() -> dict[str, dict]:
    """What the repository knows of each release: its discs' image names (ninja_config's table)."""
    sys.path.insert(0, str(ROOT))
    import ninja_config
    return {v.metadata.version_dir: {"images": [f"{v.metadata.name_disk1}.bin", f"{v.metadata.name_disk2}.bin"]}
            for v in ninja_config.GAME_VERSIONS}


def sha1(path: Path) -> str:
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 22), b""):
            h.update(block)
    return h.hexdigest()


def relative(target: Path, start: Path) -> str:
    return Path(os.path.relpath(target.resolve(), start.resolve())).as_posix()


def make_link(path: Path, target: Path) -> None:
    """A mount or the roms folder, realized as a link (a junction on Windows)."""
    if path.is_symlink():
        path.unlink()
    elif path.exists():
        raise SystemExit(f"{path} exists and is not a link")
    try:
        path.symlink_to(relative(target, path.parent), target_is_directory=True)
    except OSError:
        if sys.platform != "win32":
            raise
        subprocess.run(["cmd", "/c", "mklink", "/J", str(path), str(target.resolve())], check=True, capture_output=True)


def setup(directory: Path, wanted: list[str], roms: Path | None) -> None:
    """Write a new project into `directory` (absent or empty)."""
    directory = directory.absolute()
    if directory.exists() and any(directory.iterdir()):
        raise SystemExit(f"{directory} is not empty: a project is made in a new or empty folder")
    known = releases()
    unknown = [r for r in wanted if r not in known]
    if unknown:
        raise SystemExit(f"unknown release {', '.join(unknown)}; known: {', '.join(known)}")
    directory.mkdir(parents=True, exist_ok=True)
    if roms is not None and roms.resolve() != (directory / "roms").resolve():
        make_link(directory / "roms", roms)
    else:
        (directory / "roms").mkdir(exist_ok=True)
    found = directory / "roms"
    game = {"layer": "game", "roms": "roms", "releases": {}}
    for release in wanted:
        discs = []
        for name in known[release]["images"]:
            image = found / name
            if not image.is_file():
                raise SystemExit(f"{release}: {name} is not in {found}: the project records the discs it is built for "
                                 "from the images themselves")
            discs.append({"name": name, "size": image.stat().st_size, "sha1": sha1(image)})
        game["releases"][release] = {"discs": discs}
    doc = {
        "game": game,
        "layer": [{"name": "game", "path": LAYER}, {"name": "project", "path": OWN}],
        "mount": [{"at": at, "path": relative(path, directory), **({"read_only": True} if ro else {})}
                  for at, path, ro in MOUNTS],
        "config": {r.lower(): {"release": r, "target": "psx"} for r in wanted},
    }
    (directory / OWN).mkdir(parents=True, exist_ok=True)
    (directory / "project.toml").write_text(project_toml(doc), encoding="utf-8")
    for at, path, _ro in MOUNTS:
        make_link(directory / at, path)
    (directory / ".gitignore").write_text("# The game's content and this machine's links; see project.toml.\n"
                                          + "\n".join(IGNORE) + "\n", encoding="utf-8")
    print(f"made the project {directory}")


def project_toml(doc: dict) -> str:
    """A `project.toml`: its game and releases, its layers, mounts and configurations."""
    game = doc["game"]
    parts = ["# A project. `res://` paths are absolute from this directory.",
             "[game]\n" + "\n".join(f"{k} = {toml_out.value(v)}" for k, v in game.items() if k != "releases")]
    for release, spec in game["releases"].items():
        parts.append(f"[game.releases.{release}]\ndiscs = {toml_out.value(spec['discs'])}")
    for kind in ("layer", "mount"):
        for item in doc[kind]:
            parts.append(f"[[{kind}]]\n" + "\n".join(f"{k} = {toml_out.value(v)}" for k, v in item.items()))
    for name, keys in doc["config"].items():
        parts.append(f"[config.{name}]\n" + "\n".join(f"{k} = {toml_out.value(v)}" for k, v in keys.items()))
    return "\n\n".join(parts) + "\n"


def editor_cli() -> Path | None:
    """`pe2-cli` as `cargo xtask dist` lays the editor out - beside the extensions built apart from it - laid out again
    first where Rust is installed, so that what a project is given is this checkout's; the newest there is."""
    if shutil.which("cargo"):
        print("laying out the editor and its extensions (cargo xtask dist)")
        subprocess.run(["cargo", "xtask", "dist"], cwd=EDITOR, check=True, stdout=subprocess.DEVNULL)
    cli = "pe2-cli.exe" if os.name == "nt" else "pe2-cli"
    trees = [t for t in (EDITOR / "target" / "dist").glob("pe2-*") if (t / cli).is_file() and (t / "extensions").is_dir()]
    tree = max(trees, key=lambda t: (t / cli).stat().st_mtime, default=None)
    return tree / cli if tree else None


def add_extensions(directory: Path, cli: Path) -> None:
    """Add the extensions every project holds (EXTENSIONS) to a new project: each copied into its own layer and
    required."""
    for extension in EXTENSIONS:
        subprocess.run([str(cli), "project", "extension", "add", extension, "--project", str(directory.absolute())],
                       check=True)


def write_layer(directory: Path, cli: Path) -> Path:
    """The game layer as the declarations make it: its nodes, its changeset and its plan. Returns the layer."""
    registry = json.loads(subprocess.run([str(cli), "project", "registry", "--project", str(directory)],
                                         check=True, capture_output=True, text=True).stdout)
    decl = declared.Declarations()
    toml = __import__("tomllib").loads((directory / "project.toml").read_text(encoding="utf-8"))
    game = toml.get("game")
    if not game:
        raise SystemExit(f"{directory}: the project has no [game]")
    wanted = list(game.get("releases", {}))
    layer = directory / next(l["path"] for l in toml.get("layer", []) if l["name"] == game["layer"])
    layer_init.write_nodes(layer, layer_init.layer_nodes(decl, registry, wanted))
    made = plans.make(decl, registry, wanted, game["layer"])
    (layer / layer_init.OWN[0]).write_text(made.text(), encoding="utf-8")
    print(f"wrote the game layer's nodes and its plan ({len(made.ops)} operations) into {layer}")
    return layer


def extract(directory: Path, roms: Path | None, cli: Path) -> None:
    """Write the game layer from the declarations and have the editor fill it from the discs, holding the lock."""
    directory = directory.resolve()
    _lock = project_lock.hold(directory, "tools/pe2project.py extract")
    write_layer(directory, cli)
    command = [str(cli), "project", "extract", "--project", str(directory)]
    if roms is not None:
        command += ["--roms", str(roms)]
    subprocess.run(command, check=True)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    for name in ("create", "init"):
        c = sub.add_parser(name)
        if name == "create":
            c.add_argument("directory", nargs="?", type=Path, default=ROOT / "project")
        c.add_argument("--release", action="append", help="a release the project is built for (default USA)")
        c.add_argument("--roms", type=Path, help="a folder of disc images kept elsewhere: `roms` links to it")
        c.add_argument("--no-extract", action="store_true", help="write the game layer, but read nothing off the discs")
        c.add_argument("--no-extensions", action="store_true", help="add no extensions (pe2.psx, pe2.ffmpeg)")
    e = sub.add_parser("extract")
    e.add_argument("--project", type=Path, default=Path.cwd())
    e.add_argument("--roms", type=Path)
    args = ap.parse_args()
    cli = editor_cli()
    if cli is None:
        print("warning: no editor laid out (`cargo xtask dist`, which needs Rust): no extensions, no game layer")
    if args.command in ("create", "init"):
        directory = (args.directory if args.command == "create" else Path.cwd()).absolute()
        setup(directory, args.release or ["USA"], args.roms)
        if cli is None:
            return 0
        if not args.no_extensions:
            add_extensions(directory, cli)
        if args.no_extract:
            _lock = project_lock.hold(directory, "tools/pe2project.py init")
            write_layer(directory, cli)
        else:
            extract(directory, None, cli)
    else:
        if cli is None:
            raise SystemExit("the editor is needed to extract: `cargo xtask dist`")
        extract(args.project, args.roms, cli)
    return 0


if __name__ == "__main__":
    sys.exit(main())
