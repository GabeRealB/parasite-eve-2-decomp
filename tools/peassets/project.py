"""Projects: what a `project.toml` declares, and how the tier 1 build reaches a project's data (local/project_design.md).

A project is a directory holding `project.toml` (made by tools/pe2project.py):

    [game]
    layer = "game"                       the layer the user's discs fill (gitignored)
    roms = "roms"                        where a user puts the disc images (gitignored; shipped empty)
    declarations = "res://declarations"  what the extraction goes by

    [game.releases.USA]                  the releases the project is built for, and the discs each needs
    discs = [ { name = "SLUS-01042.bin", sha1 = "..." }, ... ]

    [[layer]]                            the asset layers below the project's own directory, bottom to top
    name = "game"
    path = ".game"

    [[mount]]                            external trees placed in the project (its code tree)
    at = "src"
    path = "../src"                      relative to project.toml, or absolute
    read_only = false

    [config.usa]                         configurations: flat key-values
    release = "USA"

`res://` paths are absolute from the project root, the directory holding `project.toml`.

The tier 1 build reaches the data it works on through one link, `build/<release>/project`, made at the start of a
build (`link`): the splat configs' targets, the overlay configs' packages and every generator's root are paths through
it. Without `--project` it is the extraction's native forms, `assets/<release>`; with it, a project's game layer
(`data_root`) - until a resolver joins a project's layers, the game layer is the data a build reads.
"""
from __future__ import annotations

import os
import subprocess
import sys
import tomllib
from dataclasses import dataclass, field
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
FILE = "project.toml"
ENV = "PE2_PROJECT"                      # the data root a tool falls back to when it is given none


@dataclass
class Mount:
    at: str
    path: Path                           # resolved
    read_only: bool = False
    revision: str | None = None


@dataclass
class Layer:
    name: str
    path: Path                           # resolved


@dataclass
class Game:
    layer: str
    roms: Path                           # resolved
    declarations: str                    # a res:// path
    releases: dict[str, list[dict]]      # release -> its discs, { name, sha1 }


@dataclass
class Project:
    root: Path
    game: Game | None
    layers: list[Layer] = field(default_factory=list)
    mounts: list[Mount] = field(default_factory=list)
    configs: dict[str, dict] = field(default_factory=dict)

    def resolve(self, uri: str) -> Path:
        """A `res://` path's place on disk, through the mounts (the longest mount point first)."""
        if not uri.startswith("res://"):
            raise ValueError(f"{uri}: not a res:// path")
        rel = uri[len("res://"):].strip("/")
        for m in sorted(self.mounts, key=lambda m: -len(m.at)):
            if rel == m.at or rel.startswith(m.at + "/"):
                return m.path / rel[len(m.at):].lstrip("/")
        return self.root / rel

    def layer(self, name: str) -> Layer:
        return next(layer for layer in self.layers if layer.name == name)


def _path(directory: Path, value: str) -> Path:
    p = Path(value)
    return p if p.is_absolute() else (directory / p).absolute()


def load(directory: Path) -> Project:
    """The project in `directory`."""
    directory = Path(directory).absolute()
    data = tomllib.loads((directory / FILE).read_text(encoding="utf-8"))
    g = data.get("game")
    game = Game(g["layer"], _path(directory, g.get("roms", "roms")), g["declarations"],
                {r: spec.get("discs", []) for r, spec in g.get("releases", {}).items()}) if g else None
    layers = [Layer(layer["name"], _path(directory, layer["path"])) for layer in data.get("layer", ())]
    mounts = [Mount(m["at"], _path(directory, m["path"]), m.get("read_only", False), m.get("revision"))
              for m in data.get("mount", ())]
    return Project(directory, game, layers, mounts, data.get("config", {}))


def is_project(directory: Path) -> bool:
    return (Path(directory) / FILE).is_file()


def data_root(directory: Path) -> Path:
    """The data a build reads in a project: its game layer (until a resolver joins the layers)."""
    p = load(directory)
    if p.game is None:
        raise SystemExit(f"{directory}: the project names no game layer ([game] layer)")
    return p.layer(p.game.layer).path


def _toml(value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return str(value)
    if isinstance(value, str):
        return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'
    if isinstance(value, list):
        return "[" + ", ".join(_toml(v) for v in value) + "]"
    return "{ " + ", ".join(f"{k} = {_toml(v)}" for k, v in value.items()) + " }"


def render(game: dict, layers: list[dict], mounts: list[dict], configs: dict[str, dict]) -> str:
    """A `project.toml`'s text."""
    lines = ["# A project (local/project_design.md). `res://` paths are absolute from this directory.", "",
             "[game]"] + [f"{k} = {_toml(v)}" for k, v in game.items() if k != "releases"]
    for release, spec in game.get("releases", {}).items():
        lines += ["", f"[game.releases.{release}]", "discs = ["] + [f"    {_toml(d)}," for d in spec["discs"]] + ["]"]
    for layer in layers:
        lines += ["", "[[layer]]"] + [f"{k} = {_toml(v)}" for k, v in layer.items()]
    for mount in mounts:
        lines += ["", "[[mount]]"] + [f"{k} = {_toml(v)}" for k, v in mount.items()]
    for name, keys in configs.items():
        lines += ["", f"[config.{name}]"] + [f"{k} = {_toml(v)}" for k, v in keys.items()]
    return "\n".join(lines) + "\n"


def link_path(release: str = "USA") -> Path:
    """Where a build reaches its data: `build/<release>/project`, relative to the repository."""
    return Path("build") / release / "project"


def link(release: str, target: Path) -> Path:
    """Point `build/<release>/project` at the data a build reads - a directory of native forms, or a project, whose
    game layer it then points at - as a junction on Windows (no privileges needed), and make it the root tools fall
    back to in this process and its children. Returns the link's path."""
    if is_project(target):
        target = data_root(target)
    path = REPOSITORY / link_path(release)
    target = Path(target).absolute()                # as given: an alias stays an alias
    if path.is_symlink() or path.exists():
        if path.is_symlink() and Path(os.readlink(path)) == target:
            os.environ[ENV] = str(link_path(release))
            return link_path(release)
        if path.is_symlink() or not path.is_dir():
            path.unlink()
        elif os.path.isjunction(path):
            os.rmdir(path)                  # removes the junction only, never what it points at
        else:
            raise SystemExit(f"{path} is a directory, not a link to a project: move it away")
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        path.symlink_to(target, target_is_directory=True)
    except OSError:
        if sys.platform != "win32":
            raise
        subprocess.run(["cmd", "/c", "mklink", "/J", str(path), str(target)], check=True, capture_output=True)
    os.environ[ENV] = str(link_path(release))
    return link_path(release)


def default_root(release: str = "USA") -> Path:
    """The data tools work on when given none: the checkout's project, `project/` (its game layer), where there is
    one - the full extraction, which the asset tools work on -, else the extraction under `assets/<release>`."""
    if is_project(REPOSITORY / "project"):
        return data_root(REPOSITORY / "project")
    return REPOSITORY / "assets" / release


def root() -> Path:
    """The data root a tool works on when it was given none: `PE2_PROJECT`, else the default."""
    return Path(os.environ[ENV]) if os.environ.get(ENV) else default_root()
