#!/usr/bin/env python3
"""The project lock, as the editor takes it (editor/crates/project/src/lock.rs): one process at a time works on a
project - the editor, an extraction, a build. Each holds an exclusive OS lock on `<project>/.cache/lock` for as long as
it runs (`flock` on Unix, `LockFile` on Windows), so it is released however the process ends; `.cache/lock.owner` names
the holder for whoever finds the project locked. A tool that finds it held waits, saying who holds it.

    python3 tools/project_lock.py DIR WHO -- COMMAND [ARG ...]    run a command holding DIR's lock

A process holding the lock passes it to the processes it starts (`PE2_PROJECT_LOCKED` names the project): they are
part of its work, and do not wait for their own parent.

A project the host serving it holds (`pe2-cli project host`, editor/crates/project/src/host.rs) is borrowed from it, as
the editor's tools borrow it: asked on its socket, the host lets go of the lock, and takes it back once the connection
borrowing it closes - with the lock, here.
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


def _host_address(project: Path) -> tuple[str, str]:
    """Where the host serving a project listens, as `project::host::local_name` names it: a Unix socket in its cache -
    or in the temporary folder, named by the project's path hashed, where that is too long for a socket - or a pipe."""
    import hashlib
    root = str(project)
    if os.name == "nt":
        # Rust's canonical path is verbatim (`\\?\D:\...`), and it is what is hashed.
        if not root.startswith("\\\\?\\"):
            root = "\\\\?\\UNC\\" + root[2:] if root.startswith("\\\\") else "\\\\?\\" + root
    digest = hashlib.sha1(root.lower().encode()).hexdigest()[:16]
    if os.name == "nt":
        return "pipe", rf"\\.\pipe\pe2-host-{digest}"
    path = project / ".cache" / "host.sock"
    if len(os.fsencode(str(path))) >= 100:
        path = Path(os.environ.get("TMPDIR") or "/tmp") / f"pe2-host-{digest}.sock"
    return "unix", str(path)


def _head(major: int, n: int) -> bytes:
    """A CBOR item's head: its major type and its length or value."""
    if n < 24:
        return bytes([major << 5 | n])
    for extra, size in ((24, 1), (25, 2), (26, 4), (27, 8)):
        if n < 1 << (8 * size):
            return bytes([major << 5 | extra]) + n.to_bytes(size, "big")
    raise ValueError(n)


def _frame(message: dict[str, str]) -> bytes:
    """A message to the host as it reads one (`project::wire`): its CBOR - a map of text - after its length."""
    text = lambda s: _head(3, len(s.encode())) + s.encode()  # noqa: E731
    body = _head(5, len(message)) + b"".join(text(k) + text(v) for k, v in message.items())
    return len(body).to_bytes(4, "little") + body


def _item(data: bytes, at: int):
    """The CBOR item at `at` - what the host's answers hold: numbers, text, bytes, arrays, maps, simple values - and
    where the next begins."""
    major, info = data[at] >> 5, data[at] & 31
    at += 1
    if info < 24:
        n = info
    else:
        size = 1 << (info - 24)
        n = int.from_bytes(data[at:at + size], "big")
        at += size
    if major == 0:
        return n, at
    if major == 1:
        return -1 - n, at
    if major in (2, 3):
        raw = data[at:at + n]
        return (raw if major == 2 else raw.decode()), at + n
    if major == 4:
        items = []
        for _ in range(n):
            value, at = _item(data, at)
            items.append(value)
        return items, at
    if major == 5:
        found = {}
        for _ in range(n):
            key, at = _item(data, at)
            found[key], at = _item(data, at)
        return found, at
    return {20: False, 21: True}.get(info), at


def _borrow(project: Path, who: str):
    """Borrow the project from the host holding it: the connection, held for as long as it is borrowed; None where no
    host holds it. Said and answered as CBOR frames (`project::wire`)."""
    import socket
    kind, address = _host_address(project)
    said = _frame({"say": "lend", "who": who})
    try:
        if kind == "pipe":
            pipe = open(address, "r+b", buffering=0)
            pipe.write(said)
            read = pipe.read
            connection = pipe
        else:
            connection = socket.socket(socket.AF_UNIX)
            connection.settimeout(30)
            connection.connect(address)
            connection.sendall(said)
            read = connection.makefile("rb").read
        length = int.from_bytes(read(4) or b"\0\0\0\0", "little")
        answer, _ = _item(read(length), 0) if length else ({}, 0)
    except (OSError, IndexError, UnicodeDecodeError):
        return None
    if isinstance(answer, dict) and answer.get("say") == "lent":
        return connection
    connection.close()
    return None


def _wait(handle, project: Path, who: str):
    """Wait for the lock, asking for it again and again - borrowing it from the host, where it holds it, each time -
    rather than waiting on the lock, which a host never lets go of unasked. The borrowed connection, or None."""
    while True:
        lease = _borrow(project, who)
        if _try(handle):
            return lease
        if lease is not None:
            lease.close()
        time.sleep(0.1)


class Held:
    """The project held: its lock, and the host's lease where it was borrowed - let go of after the lock."""

    def __init__(self, handle, lease) -> None:
        self.handle, self.lease = handle, lease

    def close(self) -> None:
        self.handle.close()
        if self.lease is not None:
            self.lease.close()


def hold(project: Path, who: str):
    """Lock the project for this process - borrowed from the host serving it, where it holds it - waiting while another
    holds it. Returns what holds it, until it is closed or the process ends; None when a parent process already holds
    it for this one."""
    project = project.resolve()
    if os.environ.get(HELD) == str(project):
        return None
    cache = project / ".cache"
    cache.mkdir(parents=True, exist_ok=True)
    ignore = cache / ".gitignore"       # the cache ignores itself, whoever makes it first (cache.rs, `make`)
    if not ignore.exists():
        ignore.write_text("*\n", encoding="utf-8")
    handle = open(cache / "lock", "a+b")
    lease = None
    if not _try(handle):
        lease = _borrow(project, who)
        if not _try(handle):
            if lease is not None:
                lease.close()
            owner = cache / "lock.owner"
            holder = owner.read_text(encoding="utf-8").strip() if owner.exists() else "another process"
            print(f"waiting: the project is in use by {holder}", file=sys.stderr)
            lease = _wait(handle, project, who)
    (cache / "lock.owner").write_text(f"{who} (process {os.getpid()})", encoding="utf-8")
    os.environ[HELD] = str(project)
    return Held(handle, lease)


def main() -> int:
    args = sys.argv[1:]
    if len(args) < 4 or args[2] != "--":
        print(__doc__.strip().split("\n\n")[1], file=sys.stderr)
        return 2
    _handle = hold(Path(args[0]), args[1])
    return subprocess.call(args[3:])


if __name__ == "__main__":
    sys.exit(main())
