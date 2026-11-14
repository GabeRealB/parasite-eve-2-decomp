"""The editor's command line as `cargo xtask dist` lays it out: how the Python side runs the editor's commands, never
reading an editor project itself."""
from __future__ import annotations

import os
from pathlib import Path

EDITOR = Path(__file__).resolve().parents[3] / "editor"


def dist_cli() -> Path | None:
    """The newest `pe2-cli` `cargo xtask dist` laid out, as it is."""
    cli = "pe2-cli.exe" if os.name == "nt" else "pe2-cli"
    found = [t / cli for t in (EDITOR / "target" / "dist").glob("pe2-*") if (t / cli).is_file()]
    return max(found, key=lambda c: c.stat().st_mtime, default=None)
