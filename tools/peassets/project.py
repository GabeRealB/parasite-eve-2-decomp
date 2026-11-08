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
(`data_root`): its merged layers where this machine serves them as a filesystem, else the files its generated layer's
plan wrote.
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
    served: Path | None = None           # where its merged layers are served: its `assets` folder (SERVED)

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
    machine = tomllib.loads(local.read_text(encoding="utf-8")) if local.is_file() else {}
    here = machine.get("mounts", {})
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
    return Project(directory, layers, mounts, data.get("config", {}), unmanaged,
                   directory / SERVED)


def is_project(directory: Path) -> bool:
    return (Path(directory) / FILE).is_file()


def data_root(directory: Path) -> Path:
    """The data a build reads in a project: its merged layers where they are served (by its host, at its `assets`
    folder) - every layer's files at their resolved paths, the generated layer's among them - kept served for this
    process (`serve`), so the host does not stop between its reads. Where nothing serves them and the layers hold large
    files as chunks (a store, `.layers/.store`), which no program reads as files, they are served for this process too;
    else the files its generated layer's plan wrote."""
    project = load(directory)
    if project.served is not None and os.path.ismount(project.served):
        return serve(project.root) if dist_cli() is not None else project.served
    generated = project.generated()
    if not generated:
        raise SystemExit(f"{directory}: no layer of the project is generated by a plan")
    if (project.root / ".layers" / ".store").is_dir():
        return serve(project.root)
    return generated[0].path


_SERVING: dict[Path, tuple[subprocess.Popen, Path]] = {}

SERVED = "assets"                         # the folder a project is served at (`project::SERVED`)


def dist_cli() -> Path | None:
    """The newest `pe2-cli` `cargo xtask dist` laid out, as it is."""
    cli = "pe2-cli.exe" if os.name == "nt" else "pe2-cli"
    found = [t / cli for t in (REPOSITORY / "editor" / "target" / "dist").glob("pe2-*") if (t / cli).is_file()]
    return max(found, key=lambda c: c.stat().st_mtime, default=None)


def serve(root: Path) -> Path:
    """Keep a project's merged layers served as a filesystem for as long as this process runs (`pe2-cli project serve`:
    by the host serving it, or one started, mounted at its `assets` folder), and give where. A host started under a
    project lock this process holds (`PE2_PROJECT_LOCKED`) serves it for this process alone."""
    import atexit
    import signal
    import time
    root = Path(root).absolute()
    if root in _SERVING:
        return _SERVING[root][1]
    cli = dist_cli()
    if cli is None:
        raise SystemExit(f"{root}: its large files are chunks, read through a mount: build the editor (cargo xtask dist)")
    at = root / SERVED
    at.mkdir(parents=True, exist_ok=True)
    process = subprocess.Popen([str(cli), "project", "serve", "--project", str(root), "--backend", "fuse"],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    # Its first line says it is served, and kept so for this process: by the host it started, or one serving already.
    said = process.stdout.readline() if process.stdout else ""
    waited = time.monotonic() + 120
    while "served at" not in said or not os.path.ismount(at):
        if process.poll() is not None or time.monotonic() > waited:
            process.kill()
            why = (process.stderr.read() if process.stderr else "").strip()
            raise SystemExit(f"{root}: its layers could not be served at {at}"
                             f"{': ' + why if why else ' (is its host serving it otherwise than with FUSE?)'}")
        time.sleep(0.1)

    def stop() -> None:
        process.send_signal(signal.SIGINT)
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            process.kill()
    atexit.register(stop)
    _SERVING[root] = (process, at)
    return at


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
