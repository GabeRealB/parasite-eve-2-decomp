#!/usr/bin/env python3
"""Make projects from the repository's declarations, and fill them from the discs.

    pe2project.py create [DIR] [--release R ...] [--roms PATH] [--no-extract] [--no-extensions]
    pe2project.py init [--release R ...] [--roms PATH] [--no-extract] [--no-extensions]
    pe2project.py extract [--project DIR] [--roms PATH]

`create` makes a new project in DIR (default `project/` at the repository root; it must not exist or be empty), `init`
makes the current directory one. The editor makes it (`pe2-cli project new`, then `layer add`, `mount add`, `mount
locate`, `config add`): the layer of the game's nodes `.layers/game` below the project's own `.layers/project`, the
generated layer `.layers/game-files` its plan fills with their files, the unmanaged mount `roms` where the disc images
are (`roms/` in the project, or wherever `--roms` says, which this machine's `project.local.toml` keeps), a read-only
mount of this checkout's `packages/`, a configuration per release the decompilation builds; then the mount is
realized as a link, the extensions the project builds and plays its movies with are added (`pe2.psx`, `pe2.ffmpeg`, by
`pe2-cli project extension add`), and it is extracted. A project is of every release declared (`--release` for fewer),
the plan preferring those the decompilation builds. The editor keeps what is this machine's out of the project's `.gitignore`; what this script adds,
the links and the builds' output, it keeps out itself.

`extract` writes the game's nodes from the declarations (`editor_project/`): every asset's node and the layer's
changeset, and the plan that fills them (`.layers/game-files.plan.jsonl`, beside the generated layer) - stating the
discs it reads, by name, size and sha1 - and then has the editor run the plan (`pe2-cli project extract`), which finds
the discs in `roms` and makes only what changed since it last ran. The nodes are made by what the project's extensions
offer (`pe2-cli project registry`).

The editor comes from `cargo xtask dist`, laid out again first where Rust is installed; without one, no project is made.
"""
from __future__ import annotations

import argparse
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

EDITOR = ROOT / "editor"
# What every project holds, in the layer of the game's nodes: the PlayStation target its packages build for, and ffmpeg,
# its movies' video files.
EXTENSIONS = ["pe2.psx", "pe2.ffmpeg"]
NODES = "game"                      # the layer of the game's nodes, `.layers/game`, its changeset `.layers/game.toml`
FILES = "game-files"                # the generated layer filling them, its plan `.layers/game-files.plan.jsonl`
MOUNTS = [("packages", ROOT / "packages", True)]
# What this script makes that is this machine's: the mounts' links, and what builds write.
IGNORE = ["/build/"] + [f"/{at}" for at, _p, _r in MOUNTS]


def built_releases() -> list[str]:
    """The releases the decompilation builds, in its order (ninja_config's table)."""
    sys.path.insert(0, str(ROOT))
    import ninja_config
    return [v.metadata.version_dir for v in ninja_config.GAME_VERSIONS]


def declared_releases() -> list[str]:
    """The releases the declarations know, in the order a plan prefers them: those the decompilation builds first, as
    it orders them, then the rest by name."""
    declared = {p.name for p in (declared_root() / "disc").iterdir() if p.is_dir()}
    built = built_releases()
    return [r for r in built if r in declared] + sorted(declared - set(built))


def declared_root() -> Path:
    return Path(declared.DECLARATIONS)


def relative(target: Path, start: Path) -> str:
    return Path(os.path.relpath(target.resolve(), start.resolve())).as_posix()


def make_link(path: Path, target: Path) -> None:
    """A mount, realized as a link (a junction on Windows)."""
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


def setup(directory: Path, wanted: list[str], roms: Path | None, cli: Path) -> None:
    """Make a new project in `directory` (absent or empty), by the editor's own commands: an empty project, then the
    layer of the game's nodes and the generated layer filling them, where the disc images are, its mounts and its
    configurations. What is not the project's file - the links and their `.gitignore` entries - is written here."""
    directory = directory.absolute()
    if directory.exists() and any(directory.iterdir()):
        raise SystemExit(f"{directory} is not empty: a project is made in a new or empty folder")
    known = declared_releases()
    unknown = [r for r in wanted if r not in known]
    if unknown:
        raise SystemExit(f"unknown release {', '.join(unknown)}; declared: {', '.join(known)}")
    subprocess.run([str(cli), "project", "new", str(directory)], check=True)
    edit = editing(directory, cli)
    edit("layer", "add", NODES, "--bottom")
    edit("layer", "add", FILES, "--generated", "--fills", NODES)
    edit("mount", "add", "roms", "--unmanaged")
    locate(directory, roms, cli)
    for at, path, read_only in MOUNTS:
        edit("mount", "add", at, relative(path, directory), *(["--read-only"] if read_only else []))
        make_link(directory / at, path)
    # A configuration for each release the decompilation builds.
    for release in (r for r in wanted if r in built_releases()):
        edit("config", "add", release.lower(), "--release", release, "--target", "psx")
    ignore = directory / ".gitignore"
    text = ignore.read_text(encoding="utf-8") if ignore.is_file() else ""
    ignore.write_text(text + "".join(f"{entry}\n" for entry in IGNORE if entry not in text.splitlines()),
                      encoding="utf-8")
    print(f"made the project {directory}")


def editing(directory: Path, cli: Path):
    def edit(*arguments: str) -> None:
        subprocess.run([str(cli), "project", *arguments, "--project", str(directory)], check=True)
    return edit


def locate(directory: Path, roms: Path | None, cli: Path) -> None:
    """Where the disc images are on this machine: `roms/` in the project unless said (`project.local.toml`)."""
    if roms is None:
        (directory / "roms").mkdir(exist_ok=True)
    else:
        editing(directory, cli)("mount", "locate", "roms", str(roms.absolute()))


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
    """Add the extensions every project holds (EXTENSIONS) to a new project: each copied into the layer of the game's
    nodes, which ships with them, and required."""
    for extension in EXTENSIONS:
        subprocess.run([str(cli), "project", "extension", "add", extension, "--layer", NODES,
                        "--project", str(directory.absolute())], check=True)


def generated(directory: Path) -> tuple[Path, Path]:
    """The project's generated layer and the layer of the nodes it fills, as folders."""
    toml = __import__("tomllib").loads((directory / "project.toml").read_text(encoding="utf-8"))
    layers = {l["name"]: directory / l.get("path", f".layers/{l['name']}") for l in toml.get("layer", [])}
    filling = [l for l in toml.get("layer", []) if l.get("kind") == "generated"]
    if not filling:
        raise SystemExit(f"{directory}: no layer of the project is generated by a plan")
    return layers[filling[0]["name"]], layers[filling[0]["fills"]]


def planned_releases(directory: Path, plan: Path) -> list[str]:
    """The releases a project is of, in the order a plan prefers them: what its plan reads, else every one declared."""
    if plan.is_file():
        with open(plan, encoding="utf-8") as f:
            planned = set(json.loads(f.readline()).get("inputs", {}))
        return [r for r in declared_releases() if r in planned]
    return declared_releases()


def write_layer(directory: Path, cli: Path, releases: list[str] | None = None) -> Path:
    """The game's nodes as the declarations make them, and the plan filling them. Returns the nodes' layer."""
    registry = json.loads(subprocess.run([str(cli), "project", "registry", "--project", str(directory)],
                                         check=True, capture_output=True, text=True).stdout)
    decl = declared.Declarations()
    files, layer = generated(directory)
    plan = files.with_name(files.name + ".plan.jsonl")
    wanted = [r for r in declared_releases() if r in releases] if releases else planned_releases(directory, plan)
    layer_init.write_nodes(layer, layer_init.layer_nodes(decl, registry, wanted))
    made = plans.make(decl, registry, wanted)
    plan.write_text(made.text(), encoding="utf-8")
    print(f"wrote the game's nodes into {layer}, and the plan filling them ({len(made.ops)} operations) into {plan}")
    return layer


def extract(directory: Path, roms: Path | None, cli: Path, releases: list[str] | None = None) -> None:
    """Write the game's nodes and their plan from the declarations and have the editor fill them from the discs,
    holding the lock."""
    directory = directory.resolve()
    if roms is not None:
        locate(directory, roms, cli)
    _lock = project_lock.hold(directory, "tools/pe2project.py extract")
    write_layer(directory, cli, releases)
    subprocess.run([str(cli), "project", "extract", "--project", str(directory)], check=True)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    for name in ("create", "init"):
        c = sub.add_parser(name)
        if name == "create":
            c.add_argument("directory", nargs="?", type=Path, default=ROOT / "project")
        c.add_argument("--release", action="append", help="a release the project is of (default: every one declared)")
        c.add_argument("--roms", type=Path, help="where the disc images are, if not in the project's `roms/`")
        c.add_argument("--no-extract", action="store_true", help="write the game's nodes, but read nothing off the discs")
        c.add_argument("--no-extensions", action="store_true", help="add no extensions (pe2.psx, pe2.ffmpeg)")
    e = sub.add_parser("extract")
    e.add_argument("--project", type=Path, default=Path.cwd())
    e.add_argument("--roms", type=Path, help="where the disc images are on this machine, from now on")
    args = ap.parse_args()
    cli = editor_cli()
    if cli is None:
        raise SystemExit("a project is made by the editor's own commands: lay the editor out (`cargo xtask dist`, "
                         "which needs Rust)")
    if args.command in ("create", "init"):
        directory = (args.directory if args.command == "create" else Path.cwd()).absolute()
        wanted = args.release or declared_releases()
        setup(directory, wanted, args.roms, cli)
        if not args.no_extensions:
            add_extensions(directory, cli)
        if args.no_extract:
            _lock = project_lock.hold(directory, "tools/pe2project.py init")
            write_layer(directory, cli, wanted)
        else:
            extract(directory, None, cli, wanted)
    else:
        extract(args.project, args.roms, cli)
    return 0


if __name__ == "__main__":
    sys.exit(main())
