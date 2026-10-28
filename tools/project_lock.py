#!/usr/bin/env python3
"""The project lock, as the editor takes it (editor/crates/project/src/lock.rs): one process at a time works on a
project - the editor, an extraction, a build. Each holds an exclusive OS lock on `<project>/.cache/lock` for as long as
it runs (`flock` on Unix, `LockFile` on Windows), so it is released however the process ends; `.cache/lock.owner` names
the holder for whoever finds the project locked. A tool that finds it held waits, saying who holds it.

    python3 tools/project_lock.py DIR WHO -- COMMAND [ARG ...]    run a command holding DIR's lock

A process holding the lock passes it to the processes it starts (`PE2_PROJECT_LOCKED` names the project): they are
part of its work, and do not wait for their own parent.
"""
from __future__ import annotations

import os
import subprocess
import sys
import time
from pathlib import Path

HELD = "PE2_PROJECT_LOCKED"


def _try(handle) -> bool:
    if os.name == "nt":
        import msvcrt
        handle.seek(0)                  # it locks from the position: the first byte, which every holder's range holds
        try:
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
            return True
        except OSError:
            return False
    import fcntl
    try:
        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return True
    except BlockingIOError:
        return False


def _wait(handle) -> None:
    if os.name == "nt":
        while not _try(handle):
            time.sleep(0.5)
        return
    import fcntl
    fcntl.flock(handle, fcntl.LOCK_EX)


def hold(project: Path, who: str):
    """Lock the project for this process, waiting while another holds it. Returns the lock's handle, held until it is
    closed or the process ends; None when a parent process already holds it for this one."""
    project = project.resolve()
    if os.environ.get(HELD) == str(project):
        return None
    cache = project / ".cache"
    cache.mkdir(parents=True, exist_ok=True)
    ignore = cache / ".gitignore"       # the cache ignores itself, whoever makes it first (cache.rs, `make`)
    if not ignore.exists():
        ignore.write_text("*\n", encoding="utf-8")
    handle = open(cache / "lock", "a+b")
    if not _try(handle):
        owner = cache / "lock.owner"
        holder = owner.read_text(encoding="utf-8").strip() if owner.exists() else "another process"
        print(f"waiting: the project is in use by {holder}", file=sys.stderr)
        _wait(handle)
    (cache / "lock.owner").write_text(f"{who} (process {os.getpid()})", encoding="utf-8")
    os.environ[HELD] = str(project)
    return handle


def main() -> int:
    args = sys.argv[1:]
    if len(args) < 4 or args[2] != "--":
        print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
        return 2
    _handle = hold(Path(args[0]), args[1])
    return subprocess.call(args[3:])


if __name__ == "__main__":
    sys.exit(main())
