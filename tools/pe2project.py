#!/usr/bin/env python3
"""Make projects and fill their game layer.

    pe2project.py create [DIR] [--release R ...] [--roms PATH] [--no-extract]
    pe2project.py init [--release R ...] [--roms PATH] [--no-extract]
    pe2project.py extract [--project DIR] [--release R ...] [--roms PATH] [--declarations PATH]

`create` makes a new project in DIR (default `project/` at the repository root; it must not exist or be empty), `init`
makes the current directory one. Either writes `project.toml` - the releases it is built for with the discs each needs
(names, sizes and sha1s, read off the images), the game layer `.game`, the `roms` folder, the declarations it extracts
by, mounts of this checkout's `src/` and `include/` and of its declarations, a configuration per release - realizes the
mounts as links, writes the `.gitignore` that keeps the game's content out (the game layer, `roms`, the links), and
then extracts. `--roms PATH` links `roms` to a folder of images kept elsewhere.

`extract` (re)writes a project's game layer, and nothing else: it finds each release's discs in `roms` by size and
sha1, dumps them into the layer (`.rom/<release>/disk<n>`), extracts them with the project's declarations, and records
what it used in the layer's `extraction.toml`. The game layer is the extraction's alone, so it is cleared first.

This is the interim tool: it wraps the repository's full Python extraction (tools/peassets/extract.py) and its
declarations, which know the USA release only; the editor's Rust `project` crate takes over (`pe2-cli project ...`).
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools" / "peassets"))
import project as projects  # noqa: E402

LAYER = ".game"
DECLARATIONS = "res://declarations"
MOUNTS = [("src", ROOT / "src", False), ("include", ROOT / "include", False),
          ("declarations", ROOT / "tools" / "peassets", True)]   # today the declarations live beside their extractor
IGNORE = [f"/{LAYER}/", "/roms/", "/build/", "/project.local.toml"] + [f"/{at}" for at, _p, _r in MOUNTS]


def releases() -> dict[str, dict]:
    """What the repository knows of each release: its discs' image names and executables (ninja_config's table)."""
    sys.path.insert(0, str(ROOT))
    import ninja_config
    return {v.metadata.version_dir: {"images": [f"{v.metadata.name_disk1}.bin", f"{v.metadata.name_disk2}.bin"],
                                     "executables": [v.metadata.exe_disk1, v.metadata.exe_disk2]}
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
    game = {"layer": "game", "roms": "roms", "declarations": DECLARATIONS, "releases": {}}
    for release in wanted:
        discs = []
        for name in known[release]["images"]:
            image = found / name
            if not image.is_file():
                raise SystemExit(f"{release}: {name} is not in {found}: the project records the discs it is built for "
                                 "from the images themselves")
            discs.append({"name": name, "size": image.stat().st_size, "sha1": sha1(image)})
        game["releases"][release] = {"discs": discs}
    mounts = [{"at": at, "path": relative(path, directory), **({"read_only": True} if ro else {})}
              for at, path, ro in MOUNTS]
    text = projects.render(game, [{"name": "game", "path": LAYER}], mounts,
                           {r.lower(): {"release": r} for r in wanted})
    (directory / projects.FILE).write_text(text, encoding="utf-8")
    for at, path, _ro in MOUNTS:
        make_link(directory / at, path)
    (directory / ".gitignore").write_text("# The game's content and this machine's links; see project.toml.\n"
                                          + "\n".join(IGNORE) + "\n", encoding="utf-8")
    print(f"made the project {directory}")


def find_disc(roms: Path, disc: dict) -> Path:
    """A disc's image in `roms`, by size and sha1 (its name may differ)."""
    named = roms / disc["name"]
    candidates = ([named] if named.is_file() else []) + sorted(p for p in roms.iterdir() if p.is_file() and p != named)
    for p in candidates:
        if p.stat().st_size == disc["size"] and sha1(p) == disc["sha1"]:
            return p
    raise SystemExit(f"{disc['name']} (sha1 {disc['sha1']}) is not in {roms}")


def extract(directory: Path, wanted: list[str] | None, roms: Path | None, declarations: Path | None) -> None:
    """(Re)write a project's game layer from its discs."""
    p = projects.load(directory)
    if p.game is None:
        raise SystemExit(f"{directory}: the project has no [game]")
    wanted = wanted or list(p.game.releases)
    missing = [r for r in wanted if r not in p.game.releases]
    if missing:
        raise SystemExit(f"{', '.join(missing)}: not a release of this project ([game.releases])")
    if wanted != ["USA"]:
        raise SystemExit("the repository's declarations know the USA release only; extracting others needs their own")
    decl = declarations or p.resolve(p.game.declarations)
    if not decl.exists() or not (ROOT / "tools" / "peassets").samefile(decl):
        raise SystemExit(f"{decl}: the interim extraction goes by this checkout's declarations (tools/peassets) only")
    layer = p.layer(p.game.layer).path
    if layer.resolve() == p.root.resolve() or p.root.resolve() not in layer.resolve().parents:
        raise SystemExit(f"{layer}: the game layer must be a folder inside the project")
    roms = roms or p.game.roms
    images = {r: [find_disc(roms, d) for d in p.game.releases[r]] for r in wanted}
    if layer.is_symlink():
        raise SystemExit(f"{layer} is a link: the game layer is the extraction's own folder")
    shutil.rmtree(layer, ignore_errors=True)        # the layer is the extraction's alone
    layer.mkdir(parents=True)
    sys.path.insert(0, str(ROOT))
    import ninja_config
    for release in wanted:
        rom = layer / ".rom" / release
        for n, image in enumerate(images[release], 1):
            disk = rom / f"disk{n}"
            subprocess.run([str(ninja_config.DUMPSXISO), "-x", str(disk), "-s", str(disk / "layout.xml"), str(image)],
                           check=True, stdout=subprocess.DEVNULL)
        exe = releases()[release]["executables"]
        d1, d2 = rom / "disk1", rom / "disk2"
        subprocess.run([sys.executable, str(ROOT / "tools" / "peassets" / "extract.py"),
                        "-exe_d1", str(d1 / exe[0]), "-exe_d2", str(d2 / exe[1]),
                        "-s0_hdr", str(d1 / "STAGE0.HED"), "-s0_dat", str(d1 / "STAGE0.CDF"),
                        "-s1", str(d1 / "STAGE1.CDF"), "-s2", str(d1 / "STAGE2.CDF"), "-s3", str(d1 / "STAGE3.CDF"),
                        "-s4", str(d2 / "STAGE4.CDF"), "-s5", str(d2 / "STAGE5.CDF"), "-o", str(layer)], check=True)
    revision = subprocess.run(["git", "rev-parse", "HEAD"], cwd=decl, capture_output=True, text=True).stdout.strip()
    lines = ["# What made this layer (tools/pe2project.py extract): this machine's paths, so it stays in the layer.", "",
             f'declarations = "{decl.resolve().as_posix()}"', f'declarations_revision = "{revision}"']
    for release in wanted:
        lines += ["", f"[releases.{release}]", "images = ["] + [
            f'    {{ path = "{img.resolve().as_posix()}", sha1 = "{d["sha1"]}" }},'
            for img, d in zip(images[release], p.game.releases[release])] + ["]"]
    (layer / "extraction.toml").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"extracted {', '.join(wanted)} into {layer}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="command", required=True)
    for name in ("create", "init"):
        c = sub.add_parser(name)
        if name == "create":
            c.add_argument("directory", nargs="?", type=Path, default=ROOT / "project")
        c.add_argument("--release", action="append", help="a release the project is built for (default USA)")
        c.add_argument("--roms", type=Path, help="a folder of disc images kept elsewhere: `roms` links to it")
        c.add_argument("--no-extract", action="store_true")
    e = sub.add_parser("extract")
    e.add_argument("--project", type=Path, default=Path.cwd())
    e.add_argument("--release", action="append")
    e.add_argument("--roms", type=Path)
    e.add_argument("--declarations", type=Path)
    args = ap.parse_args()
    if args.command in ("create", "init"):
        directory = args.directory if args.command == "create" else Path.cwd()
        setup(directory, args.release or ["USA"], args.roms)
        if not args.no_extract:
            extract(directory, None, None, None)
    else:
        extract(args.project, args.release, args.roms, args.declarations)
    return 0


if __name__ == "__main__":
    sys.exit(main())
