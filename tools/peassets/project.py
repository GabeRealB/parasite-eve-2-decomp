"""How the tier 1 build reaches the data it reads, and where a tool given no data folder works.

The tier 1 build reaches the data it works on through one link, `build/<release>/project`, made at the start of a
build (`link`): the splat configs' targets, the overlay configs' packages and every generator's root are paths through
it. It is the extraction's native forms, `assets/<release>`, which `config.py splat extract` writes from the discs.

Nothing here reads an editor project: an editor project is the editor's, and the Python side does only what building
needs. `config.py editor` has the editor build one (`pe2-cli project build`) and holds what it deploys to the
checksums, never reading the project's layers.
"""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
ENV = "PE2_PROJECT"                      # the data root a tool falls back to when it is given none


def link_path(release: str = "USA") -> Path:
    """Where a build reaches its data: `build/<release>/project`, relative to the repository."""
    return Path("build") / release / "project"


def link(release: str, target: Path) -> Path:
    """Point `build/<release>/project` at the data a build reads - a directory of native forms - as a junction on
    Windows (no privileges needed), and make it the root tools fall back to in this process and its children. Returns
    the link's path."""
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
    """The data tools work on when given none: the extraction under `assets/<release>`."""
    return REPOSITORY / "assets" / release


def root() -> Path:
    """The data root a tool works on when it was given none: `PE2_PROJECT`, else the default."""
    return Path(os.environ[ENV]) if os.environ.get(ENV) else default_root()
