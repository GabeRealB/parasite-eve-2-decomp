"""Projects: what a `project.toml` declares, and how the tier 1 build reaches a project's data.

A project is a directory holding `project.toml` (made by tools/pe2project.py, through the editor's commands):

    [[layer]]                            the layers, bottom to top: the game's nodes (shipped)
    name = "game"
    path = ".layers/game"

    [[layer]]                            their files, written by the plan beside the folder (`.layers/game-files.plan.jsonl`)
    name = "game-files"                  from the user's discs (gitignored)
    path = ".layers/game-files"
    kind = "generated"
    fills = "game"

    [[mount]]                            external trees placed in the project (its code tree)
    at = "src"
    path = "../src"                      relative to project.toml, or absolute
    read_only = false

    [[mount]]                            where the disc images are: read by path alone, never indexed; on each machine
    at = "roms"                          where `project.local.toml` says (`[mounts] roms = "..."`), else `roms/`
    unmanaged = true

    [config.usa]                         configurations: flat key-values
    release = "USA"

`res://` paths are absolute from the project root, the directory holding `project.toml`.

The tier 1 build reaches the data it works on through one link, `build/<release>/project`, made at the start of a
build (`link`): the splat configs' targets, the overlay configs' packages and every generator's root are paths through
it. Without `--project` it is the extraction's native forms, `assets/<release>`; with it, a project's generated layer
(`data_root`) - until a resolver joins a project's layers, the files the plan wrote are the data a build reads.
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
LOCAL = "project.local.toml"             # what of a project is this machine's: where its unmanaged mounts are
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
    kind: str = "assets"
    fills: str | None = None             # a generated layer's: the layer whose nodes it gives their files

    @property
    def plan(self) -> Path:
        """A generated layer's plan: beside its folder."""
        return self.path.with_name(self.path.name + ".plan.jsonl")


@dataclass
class Project:
    root: Path
    layers: list[Layer] = field(default_factory=list)
    mounts: list[Mount] = field(default_factory=list)
    configs: dict[str, dict] = field(default_factory=dict)
    unmanaged: list[Mount] = field(default_factory=list)

    def resolve(self, uri: str) -> Path:
        """A `res://` path's place on disk, through the mounts, unmanaged ones too (the longest mount point first)."""
        if not uri.startswith("res://"):
            raise ValueError(f"{uri}: not a res:// path")
        rel = uri[len("res://"):].strip("/")
        for m in sorted(self.mounts + self.unmanaged, key=lambda m: -len(m.at)):
            if rel == m.at or rel.startswith(m.at + "/"):
                return m.path / rel[len(m.at):].lstrip("/")
        return self.root / rel

    def layer(self, name: str) -> Layer:
        return next(layer for layer in self.layers if layer.name == name)

    def generated(self) -> list[Layer]:
        return [layer for layer in self.layers if layer.kind == "generated"]


def _path(directory: Path, value: str) -> Path:
    p = Path(value)
    return p if p.is_absolute() else (directory / p).absolute()


def load(directory: Path) -> Project:
    """The project in `directory`."""
    directory = Path(directory).absolute()
    data = tomllib.loads((directory / FILE).read_text(encoding="utf-8"))
    local = directory / LOCAL
    here = tomllib.loads(local.read_text(encoding="utf-8")).get("mounts", {}) if local.is_file() else {}
    layers = [Layer(layer["name"], _path(directory, layer.get("path", f".layers/{layer['name']}")),
                    layer.get("kind", "assets"), layer.get("fills"))
              for layer in data.get("layer", ())]
    mounts, unmanaged = [], []
    for m in data.get("mount", ()):
        at = m["at"].strip("/")
        if m.get("unmanaged"):
            unmanaged.append(Mount(at, _path(directory, here.get(at, m.get("path", at))), True))
        else:
            mounts.append(Mount(at, _path(directory, m["path"]), m.get("read_only", False), m.get("revision")))
    return Project(directory, layers, mounts, data.get("config", {}), unmanaged)


def is_project(directory: Path) -> bool:
    return (Path(directory) / FILE).is_file()


def data_root(directory: Path) -> Path:
    """The data a build reads in a project: the files its generated layer's plan wrote (until a resolver joins the
    layers)."""
    generated = load(directory).generated()
    if not generated:
        raise SystemExit(f"{directory}: no layer of the project is generated by a plan")
    return generated[0].path


def link_path(release: str = "USA") -> Path:
    """Where a build reaches its data: `build/<release>/project`, relative to the repository."""
    return Path("build") / release / "project"


def link(release: str, target: Path) -> Path:
    """Point `build/<release>/project` at the data a build reads - a directory of native forms, or a project, whose
    generated layer it then points at - as a junction on Windows (no privileges needed), and make it the root tools fall
    back to in this process and its children. Returns the link's path."""
    if is_project(target):
        target = data_root(target)
    link_directory(REPOSITORY / link_path(release), Path(target))
    os.environ[ENV] = str(link_path(release))
    return link_path(release)


def link_directory(path: Path, target: Path) -> None:
    """Point `path` at a directory, as a junction on Windows, where a link needs privileges."""
    target = target.absolute()                      # as given: an alias stays an alias
    if path.is_symlink() or path.exists():
        if path.is_symlink() and Path(os.readlink(path)) == target:
            return
        if path.is_symlink() or not path.is_dir():
            path.unlink()
        elif os.path.isjunction(path):
            os.rmdir(path)                  # removes the junction only, never what it points at
        else:
            raise SystemExit(f"{path} is a directory, not a link: move it away")
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        path.symlink_to(target, target_is_directory=True)
    except OSError:
        if sys.platform != "win32":
            raise
        subprocess.run(["cmd", "/c", "mklink", "/J", str(path), str(target)], check=True, capture_output=True)


def default_root(release: str = "USA") -> Path:
    """The data tools work on when given none: the checkout's project, `project/` (its generated layer), where there is
    one - the full extraction, which the asset tools work on -, else the extraction under `assets/<release>`."""
    if is_project(REPOSITORY / "project"):
        return data_root(REPOSITORY / "project")
    return REPOSITORY / "assets" / release


def root() -> Path:
    """The data root a tool works on when it was given none: `PE2_PROJECT`, else the default."""
    return Path(os.environ[ENV]) if os.environ.get(ENV) else default_root()
