#!/usr/bin/env python3
"""Coordinate vacuum sessions: function claims and a merge lock on main.

Coordination only. Vacuum creates worktrees, runs match/port agents, and
writes git. This process never merges.

State lives in ``$(git rev-parse --git-common-dir)/vacuum-orch.json`` so every
worktree shares it. Override with ``--state`` or ``VACUUM_ORCH_STATE``.

Exit codes:
  0  ok
  1  usage / internal error
  2  conflict (claim taken, merge lock busy)
  3  no unmatched unclaimed functions
"""

from __future__ import annotations

import argparse
import fcntl
import json
import os
import subprocess
import sys
import time
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
EXIT_OK = 0
EXIT_ERROR = 1
EXIT_CONFLICT = 2
EXIT_EMPTY = 3


def _now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def pid_alive(pid: Any) -> bool:
    try:
        pid = int(pid)
    except (TypeError, ValueError):
        return False
    if pid <= 0:
        return False
    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True
    except OSError:
        return False
    return True


def git_common_dir(root: Path) -> Path:
    out = subprocess.check_output(
        ["git", "rev-parse", "--git-common-dir"],
        cwd=root,
        text=True,
    ).strip()
    path = Path(out)
    if not path.is_absolute():
        path = (root / path).resolve()
    return path


def default_state_path(root: Path) -> Path:
    env = os.environ.get("VACUUM_ORCH_STATE")
    if env:
        return Path(env)
    return git_common_dir(root) / "vacuum-orch.json"


def empty_state() -> dict:
    return {"claims": {}, "matched": [], "difficult": [], "merge": None}


class Store:
    def __init__(self, path: Path):
        self.path = path
        self._fh: Optional[Any] = None

    def __enter__(self) -> "Store":
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._fh = open(self.path, "a+", encoding="utf-8")
        fcntl.flock(self._fh.fileno(), fcntl.LOCK_EX)
        self._fh.seek(0)
        raw = self._fh.read()
        if raw.strip():
            try:
                self.data = json.loads(raw)
            except json.JSONDecodeError:
                self.data = empty_state()
        else:
            self.data = empty_state()
        self.data.setdefault("claims", {})
        self.data.setdefault("matched", [])
        self.data.setdefault("difficult", [])
        self.data.setdefault("merge", None)
        self.sweep()
        return self

    def __exit__(self, exc_type, exc, tb) -> None:
        assert self._fh is not None
        if exc_type is None:
            self._fh.seek(0)
            self._fh.truncate()
            json.dump(self.data, self._fh, indent=2)
            self._fh.write("\n")
            self._fh.flush()
        fcntl.flock(self._fh.fileno(), fcntl.LOCK_UN)
        self._fh.close()
        self._fh = None

    def sweep(self) -> None:
        dead = [
            name
            for name, claim in self.data["claims"].items()
            if not pid_alive(claim.get("pid"))
        ]
        for name in dead:
            del self.data["claims"][name]
        merge = self.data.get("merge")
        if merge and not pid_alive(merge.get("pid")):
            self.data["merge"] = None

    def session_claim(self, session: str) -> Optional[str]:
        for name, claim in self.data["claims"].items():
            if claim.get("session") == session:
                return name
        return None


def list_nonmatching_dirs(root: Path, overlay: Optional[str] = None) -> list[str]:
    cmd = [sys.executable, str(root / "tools" / "decomp_overlay.py"), "list-nonmatchings"]
    if overlay:
        cmd.extend(["--overlay", overlay])
    proc = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    if proc.returncode != 0:
        err = (proc.stderr or proc.stdout or "").strip()
        raise RuntimeError(err or "decomp_overlay.py list-nonmatchings failed")
    return [line.strip() for line in proc.stdout.splitlines() if line.strip()]


def ranked_functions(
    root: Path,
    extra_exclude: Optional[Path] = None,
    overlay: Optional[str] = None,
    only_difficult: bool = False,
) -> list[str]:
    dirs = list_nonmatching_dirs(root, overlay=overlay)
    if not dirs:
        return []
    cmd = [
        sys.executable,
        str(root / "tools" / "score_functions.py"),
        "--ranked",
        *dirs,
    ]
    if only_difficult:
        cmd.append("--only-difficult")
    if extra_exclude is not None and extra_exclude.is_file():
        cmd.extend(["--exclude-file", str(extra_exclude)])
    proc = subprocess.run(cmd, cwd=root, capture_output=True, text=True)
    if proc.returncode != 0:
        err = (proc.stderr or proc.stdout or "").strip()
        if (
            "All functions are marked as difficult" in err
            or "No remaining difficult functions" in err
            or "difficult_functions is empty" in err
        ):
            return []
        raise RuntimeError(err or "score_functions.py failed")
    return [line.strip() for line in proc.stdout.splitlines() if line.strip()]


def blocked_names(state: dict, *, only_difficult: bool = False) -> set[str]:
    names = set(state["claims"])
    names.update(state.get("matched") or [])
    # Historical give-ups are the --difficult pool; only skip them in normal vacuum.
    if not only_difficult:
        names.update(state.get("difficult") or [])
    return names


def pick_func(
    root: Path,
    state: dict,
    *,
    overlay: Optional[str] = None,
    only_difficult: bool = False,
    extra_exclude: Optional[Path] = None,
) -> Optional[str]:
    blocked = blocked_names(state, only_difficult=only_difficult)
    for name in ranked_functions(
        root,
        extra_exclude=extra_exclude,
        overlay=overlay,
        only_difficult=only_difficult,
    ):
        if name not in blocked:
            return name
    return None


def empty_claim_error(
    *, overlay: Optional[str], only_difficult: bool
) -> str:
    if only_difficult and overlay:
        return f"no remaining difficult functions in overlay {overlay}"
    if only_difficult:
        return "no remaining difficult functions"
    if overlay:
        return f"no unmatched unclaimed functions in overlay {overlay}"
    return "no unmatched unclaimed functions"


def result(ok: bool, **kwargs) -> dict:
    payload = {"ok": ok, **kwargs}
    return payload


def cmd_claim(
    store: Store,
    *,
    session: str,
    pid: int,
    cli: str,
    func: Optional[str],
    root: Path,
    overlay: Optional[str] = None,
    only_difficult: bool = False,
    extra_exclude: Optional[Path] = None,
) -> tuple[int, dict]:
    existing = store.session_claim(session)
    if existing:
        if func and func != existing:
            return EXIT_CONFLICT, result(
                False,
                error=f"session already holds {existing}",
                code="conflict",
                func=existing,
                holder=session,
            )
        return EXIT_OK, result(True, func=existing, reused=True)

    if not func:
        try:
            func = pick_func(
                root,
                store.data,
                overlay=overlay,
                only_difficult=only_difficult,
                extra_exclude=extra_exclude,
            )
        except RuntimeError as exc:
            return EXIT_ERROR, result(False, error=str(exc), code="error")
        if not func:
            return EXIT_EMPTY, result(
                False,
                error=empty_claim_error(overlay=overlay, only_difficult=only_difficult),
                code="empty",
            )

    claim = store.data["claims"].get(func)
    if claim:
        return EXIT_CONFLICT, result(
            False,
            error=f"{func} held by {claim.get('session')}",
            code="conflict",
            func=func,
            holder=claim.get("session"),
        )
    if func in store.data.get("matched") or []:
        return EXIT_CONFLICT, result(
            False, error=f"{func} already matched", code="conflict", func=func
        )
    difficult = store.data.setdefault("difficult", [])
    if func in difficult:
        if only_difficult:
            # Retry a previous give-up. Drop it from the orch skip list so a
            # concurrent worker can also see the claim rather than a stale skip.
            difficult.remove(func)
        else:
            return EXIT_CONFLICT, result(
                False, error=f"{func} marked difficult", code="conflict", func=func
            )

    store.data["claims"][func] = {
        "session": session,
        "pid": int(pid),
        "cli": cli,
        "since": _now(),
    }
    return EXIT_OK, result(True, func=func, reused=False)


def cmd_relinquish(store: Store, *, session: str, func: str) -> tuple[int, dict]:
    claim = store.data["claims"].get(func)
    if not claim:
        return EXIT_OK, result(True, func=func, missing=True)
    if claim.get("session") != session:
        return EXIT_CONFLICT, result(
            False,
            error=f"{func} held by {claim.get('session')}",
            code="conflict",
            func=func,
            holder=claim.get("session"),
        )
    del store.data["claims"][func]
    return EXIT_OK, result(True, func=func)


def cmd_finish(
    store: Store, *, session: str, func: str, status: str
) -> tuple[int, dict]:
    if status not in ("matched", "difficult"):
        return EXIT_ERROR, result(False, error=f"bad status {status}")
    claim = store.data["claims"].get(func)
    if claim and claim.get("session") != session:
        return EXIT_CONFLICT, result(
            False,
            error=f"{func} held by {claim.get('session')}",
            code="conflict",
            func=func,
            holder=claim.get("session"),
        )
    store.data["claims"].pop(func, None)
    other = "difficult" if status == "matched" else "matched"
    if func in store.data[other]:
        store.data[other] = [n for n in store.data[other] if n != func]
    if func not in store.data[status]:
        store.data[status].append(func)
    return EXIT_OK, result(True, func=func, status=status)


def cmd_merge_acquire(
    store: Store, *, session: str, pid: int
) -> tuple[int, dict]:
    merge = store.data.get("merge")
    if merge:
        if merge.get("session") == session:
            merge["pid"] = int(pid)
            return EXIT_OK, result(True, reused=True)
        return EXIT_CONFLICT, result(
            False,
            error=f"merge held by {merge.get('session')}",
            code="conflict",
            holder=merge.get("session"),
        )
    store.data["merge"] = {
        "session": session,
        "pid": int(pid),
        "since": _now(),
    }
    return EXIT_OK, result(True, reused=False)


def cmd_merge_release(store: Store, *, session: str) -> tuple[int, dict]:
    merge = store.data.get("merge")
    if not merge:
        return EXIT_OK, result(True, missing=True)
    if merge.get("session") != session:
        return EXIT_CONFLICT, result(
            False,
            error=f"merge held by {merge.get('session')}",
            code="conflict",
            holder=merge.get("session"),
        )
    store.data["merge"] = None
    return EXIT_OK, result(True)


def cmd_status(store: Store) -> tuple[int, dict]:
    return EXIT_OK, result(True, **store.data)


def cmd_log_flush(
    *,
    local: Path,
    main: Path,
    offset: int,
    session: str,
    label: str,
) -> tuple[int, dict]:
    """Append new bytes from the session log onto the shared log under flock."""
    if offset < 0:
        return EXIT_ERROR, result(False, error="offset must be >= 0")
    if not local.is_file():
        return EXIT_OK, result(True, offset=offset, wrote=0)
    chunk = local.read_bytes()[offset:]
    if not chunk:
        return EXIT_OK, result(True, offset=offset, wrote=0)
    stamp = datetime.now().strftime("%H:%M:%S")
    who = label.strip() or "session"
    header = f"\n======== [{stamp}] {session} {who} ========\n".encode()
    payload = chunk if chunk.endswith(b"\n") else chunk + b"\n"
    main.parent.mkdir(parents=True, exist_ok=True)
    lock_path = Path(str(main) + ".lock")
    with open(lock_path, "a+", encoding="utf-8") as lf:
        fcntl.flock(lf.fileno(), fcntl.LOCK_EX)
        with open(main, "ab") as mf:
            mf.write(header)
            mf.write(payload)
            mf.flush()
            os.fsync(mf.fileno())
    return EXIT_OK, result(True, offset=offset + len(chunk), wrote=len(payload))


def dispatch(cmd: str, store: Store, args: argparse.Namespace) -> tuple[int, dict]:
    if cmd == "claim":
        exclude = getattr(args, "exclude_file", None)
        return cmd_claim(
            store,
            session=args.session,
            pid=args.pid,
            cli=args.cli,
            func=args.func,
            root=Path(args.root),
            overlay=getattr(args, "overlay", None) or None,
            only_difficult=bool(getattr(args, "only_difficult", False)),
            extra_exclude=Path(exclude) if exclude else None,
        )
    if cmd == "relinquish":
        if not args.func:
            return EXIT_ERROR, result(False, error="--func required")
        return cmd_relinquish(store, session=args.session, func=args.func)
    if cmd == "finish":
        if not args.func:
            return EXIT_ERROR, result(False, error="--func required")
        return cmd_finish(
            store, session=args.session, func=args.func, status=args.status
        )
    if cmd == "merge-acquire":
        return cmd_merge_acquire(store, session=args.session, pid=args.pid)
    if cmd == "merge-release":
        return cmd_merge_release(store, session=args.session)
    if cmd == "status":
        return cmd_status(store)
    return EXIT_ERROR, result(False, error=f"unknown command {cmd}")


def run_cmd(args: argparse.Namespace) -> tuple[int, dict]:
    root = Path(args.root).resolve()
    state_path = Path(args.state) if args.state else default_state_path(root)
    wait_until = None
    if args.cmd == "merge-acquire" and args.wait > 0:
        wait_until = time.time() + args.wait

    while True:
        with Store(state_path) as store:
            code, payload = dispatch(args.cmd, store, args)
        if (
            args.cmd == "merge-acquire"
            and code == EXIT_CONFLICT
            and wait_until is not None
            and time.time() < wait_until
        ):
            time.sleep(args.poll)
            continue
        return code, payload


def serve(args: argparse.Namespace) -> int:
    root = Path(args.root).resolve()
    state_path = Path(args.state) if args.state else default_state_path(root)

    class Handler(BaseHTTPRequestHandler):
        def _read_json(self) -> dict:
            length = int(self.headers.get("Content-Length") or 0)
            if length <= 0:
                return {}
            raw = self.rfile.read(length)
            return json.loads(raw.decode("utf-8") or "{}")

        def _send(self, http_status: int, payload: dict) -> None:
            body = (json.dumps(payload) + "\n").encode("utf-8")
            self.send_response(http_status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, fmt: str, *rest) -> None:
            sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % rest))

        def do_GET(self) -> None:  # noqa: N802
            if self.path.rstrip("/") != "/status":
                self._send(404, result(False, error="not found"))
                return
            ns = argparse.Namespace(
                cmd="status",
                session="",
                pid=0,
                cli="",
                func=None,
                status="matched",
                root=str(root),
                state=str(state_path),
                wait=0,
                poll=1.0,
            )
            code, payload = run_cmd(ns)
            self._send(200 if code == EXIT_OK else 500, payload)

        def do_POST(self) -> None:  # noqa: N802
            path = self.path.rstrip("/")
            mapping = {
                "/claim": "claim",
                "/relinquish": "relinquish",
                "/finish": "finish",
                "/merge/acquire": "merge-acquire",
                "/merge/release": "merge-release",
            }
            cmd = mapping.get(path)
            if not cmd:
                self._send(404, result(False, error="not found"))
                return
            body = self._read_json()
            ns = argparse.Namespace(
                cmd=cmd,
                session=body.get("session") or "",
                pid=int(body.get("pid") or 0),
                cli=body.get("cli") or "unknown",
                func=body.get("func"),
                status=body.get("status") or "matched",
                root=str(root),
                state=str(state_path),
                wait=int(body.get("wait") or 0),
                poll=float(body.get("poll") or 1.0),
                overlay=body.get("overlay"),
                only_difficult=bool(
                    body.get("only_difficult") or body.get("difficult")
                ),
                exclude_file=body.get("exclude_file"),
            )
            if not ns.session and cmd != "status":
                self._send(400, result(False, error="session required"))
                return
            code, payload = run_cmd(ns)
            http = {
                EXIT_OK: 200,
                EXIT_CONFLICT: 409,
                EXIT_EMPTY: 503,
            }.get(code, 500)
            self._send(http, payload)

    httpd = ThreadingHTTPServer((args.bind, args.port), Handler)
    print(f"vacuum orchestrator on http://{args.bind}:{args.port}", flush=True)
    print(f"state {state_path}", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstopping", flush=True)
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        default=str(REPO_ROOT),
        help="Canonical repo (scoring + git-common-dir). Default: this checkout.",
    )
    parser.add_argument(
        "--state",
        default="",
        help="Override state JSON path (default: git-common-dir/vacuum-orch.json)",
    )
    sub = parser.add_subparsers(dest="cmd", required=True)

    def add_session(p: argparse.ArgumentParser, pid: bool = True) -> None:
        p.add_argument("--session", required=True)
        if pid:
            p.add_argument("--pid", type=int, required=True)

    p_claim = sub.add_parser("claim", help="Lease a function (or a named one)")
    add_session(p_claim)
    p_claim.add_argument("--cli", default="unknown")
    p_claim.add_argument("--func", default=None)
    p_claim.add_argument(
        "--overlay",
        default=None,
        help="Restrict auto-pick to one overlay (name, version/name, or asm path)",
    )
    p_claim.add_argument(
        "--only-difficult",
        action="store_true",
        help="Auto-pick from tools/difficult_functions instead of skipping it",
    )
    p_claim.add_argument(
        "--exclude-file",
        default=None,
        help="Extra names to skip (same format as tools/difficult_functions)",
    )

    p_rel = sub.add_parser("relinquish", help="Drop a claim without finishing")
    add_session(p_rel, pid=False)
    p_rel.add_argument("--func", required=True)

    p_fin = sub.add_parser("finish", help="Release a claim as matched or difficult")
    add_session(p_fin, pid=False)
    p_fin.add_argument("--func", required=True)
    p_fin.add_argument("--status", choices=("matched", "difficult"), required=True)

    p_acq = sub.add_parser("merge-acquire", help="Take the main-tree merge lock")
    add_session(p_acq)
    p_acq.add_argument(
        "--wait",
        type=int,
        default=0,
        help="Seconds to wait for a busy lock (0 = fail immediately)",
    )
    p_acq.add_argument("--poll", type=float, default=2.0)

    p_relm = sub.add_parser("merge-release", help="Drop the merge lock")
    add_session(p_relm, pid=False)

    sub.add_parser("status", help="Print current claims / lock / finished")

    p_log = sub.add_parser(
        "log-flush",
        help="Atomically append new bytes from a session log onto tools/vacuum.log",
    )
    p_log.add_argument("--local", required=True, help="Session log path")
    p_log.add_argument("--main", required=True, help="Shared vacuum.log path")
    p_log.add_argument("--offset", type=int, default=0)
    p_log.add_argument("--session", default="")
    p_log.add_argument("--label", default="")

    p_serve = sub.add_parser("serve", help="HTTP API on 127.0.0.1 (optional)")
    p_serve.add_argument("--bind", default="127.0.0.1")
    p_serve.add_argument("--port", type=int, default=8765)

    return parser


def main(argv: Optional[list[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if args.cmd == "serve":
        return serve(args)
    if args.cmd == "log-flush":
        code, payload = cmd_log_flush(
            local=Path(args.local),
            main=Path(args.main),
            offset=args.offset,
            session=args.session,
            label=args.label,
        )
        print(json.dumps(payload))
        return code
    if args.cmd == "status":
        args.session = ""
        args.pid = 0
        args.cli = ""
        args.func = None
        args.status = "matched"
        args.wait = 0
        args.poll = 1.0
    if args.cmd == "merge-release":
        args.pid = 0
        args.wait = 0
        args.poll = 1.0
    if args.cmd == "relinquish":
        args.pid = 0
        args.wait = 0
        args.poll = 1.0
    if args.cmd == "finish":
        args.pid = 0
        args.wait = 0
        args.poll = 1.0
    if args.cmd == "claim":
        args.wait = 0
        args.poll = 1.0

    code, payload = run_cmd(args)
    print(json.dumps(payload))
    return code


if __name__ == "__main__":
    sys.exit(main())
