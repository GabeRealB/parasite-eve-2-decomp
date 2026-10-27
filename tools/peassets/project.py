"""The project a build works on: its root, and what its `project.toml` declares (local/project_design.md).

A project is a directory holding `project.toml` and the project's data: the extraction writes both. Everything that
reads the data takes it from the project's root, which a tool is given (`--project`, or a generator's `--assets`) and
otherwise is the repository's default, `assets/<release>`:

    [project]
    release = "USA"

    [[mount]]                     external trees mounted into the project (its code tree)
    at = "src"
    path = "../../src"            relative to project.toml, or absolute

    [config.usa]                  configurations: flat key-values
    release = "USA"

The tier 1 build reaches the project through one link, `build/<release>/project`, made at the start of a build (`link`):
the splat configs' targets, the overlay configs' packages and every generator's root are paths through it, so which
project a build works on is that link's target and nothing else. Mounts and configurations are declared, for the build
to use as it learns them (step 2 of the design).
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
ENV = "PE2_PROJECT"                      # the root a tool falls back to when it is given none


@dataclass
class Mount:
    at: str
    path: Path                           # resolved
    read_only: bool = False
    revision: str | None = None


@dataclass
class Project:
    root: Path
    release: str
    mounts: list[Mount] = field(default_factory=list)
    configs: dict[str, dict] = field(default_factory=dict)


def link_path(release: str = "USA") -> Path:
    """Where a build reaches its project: `build/<release>/project`, relative to the repository."""
    return Path("build") / release / "project"


def link(release: str, target: Path) -> Path:
    """Point `build/<release>/project` at a project root (a junction on Windows, which needs no privileges), and make
    it the root tools fall back to in this process and its children. Returns the link's path."""
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
    """The repository's own project: the extraction under `assets/<release>`."""
    return REPOSITORY / "assets" / release


def root() -> Path:
    """The root of the project a tool works on when it was given none: `PE2_PROJECT`, else the default."""
    return Path(os.environ[ENV]) if os.environ.get(ENV) else default_root()


def load(directory: Path) -> Project:
    """The project at `directory`; without a `project.toml` (an extraction older than it), the release it is named
    for and nothing declared."""
    directory = Path(directory)
    path = directory / FILE
    if not path.exists():
        return Project(directory, directory.name)
    data = tomllib.loads(path.read_text(encoding="utf-8"))
    mounts = [Mount(m["at"], (directory / m["path"]).resolve() if not Path(m["path"]).is_absolute() else Path(m["path"]),
                    m.get("read_only", False), m.get("revision")) for m in data.get("mount", ())]
    return Project(directory, data["project"]["release"], mounts, data.get("config", {}))


def write(directory: Path, release: str, code_tree: Path) -> Path:
    """A new project's `project.toml`: its release, one configuration for it, and the code tree it was made with
    (src/ and include/), mounted by a path relative to the project."""
    directory = Path(directory)
    rel = Path(os.path.relpath(code_tree.resolve(), directory.resolve())).as_posix()
    text = (f"# The project: its data is this directory (local/project_design.md). Written by the extraction.\n\n"
            f"[project]\nrelease = \"{release}\"\n\n"
            + "".join(f"[[mount]]\nat = \"{tree}\"\npath = \"{rel}/{tree}\"\n\n" for tree in ("src", "include"))
            + f"[config.{release.lower()}]\nrelease = \"{release}\"\n")
    path = directory / FILE
    path.write_text(text, encoding="utf-8")
    return path
