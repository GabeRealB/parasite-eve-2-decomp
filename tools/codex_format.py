#!/usr/bin/env python3
"""Render `codex exec --json` JSONL as readable log lines.

Plain `codex exec` echoes every command it runs *and that command's entire
stdout*. For a matching agent that is ruinous: one `func_replay_bonus_80118B6C`
iteration wrote a 355KB log, 91% of which was GCC RTL dumps and register
allocation traces the agent pulled to reason about scheduling. Those dumps are
the agent working correctly - it should keep reading them - but the log only
needs to say that a dump happened and how big it was.

This renders the same shape tools/stream_format.py gives claude, so a codex
lane and a claude lane produce comparable logs:

    [17:20:51] · Reading BRIEF.md before touching base.c.
    [17:20:53] → Bash  ./build.sh base_1.c
    [17:20:59]   ← base_1.c 92.632%
    [17:21:10] ✓ done in 4m12s · 34 cmds · 31.6k in / 62 out

Unrecognised lines are passed through untouched so nothing is ever swallowed -
codex writes a few plain-text notices ("Reading additional input from
stdin...") alongside the JSONL.
"""

from __future__ import annotations

import argparse
import json
import sys
import time

MAX_INPUT = 200
MAX_RESULT = 200

# codex runs every tool call through a login shell; the wrapper is noise.
SHELL_PREFIXES = ("/usr/bin/zsh -lc ", "/bin/bash -lc ", "/usr/bin/bash -lc ",
                  "/bin/sh -lc ", "bash -lc ", "zsh -lc ")


def stamp() -> str:
    return time.strftime("[%H:%M:%S]")


def emit(line: str) -> None:
    print(line, flush=True)


def squash(text: str, limit: int) -> str:
    text = " ".join(str(text).split())
    if len(text) > limit:
        text = text[: limit - 1] + "…"
    return text


def strip_shell(command: str) -> str:
    """Drop the `zsh -lc` wrapper and one layer of quoting around the payload."""
    cmd = str(command)
    for prefix in SHELL_PREFIXES:
        if cmd.startswith(prefix):
            cmd = cmd[len(prefix):]
            break
    if len(cmd) >= 2 and cmd[0] == cmd[-1] and cmd[0] in "'\"":
        cmd = cmd[1:-1]
    return cmd


def summarise_output(text: str) -> str:
    """One line for the result, with the volume named rather than printed.

    A short result is shown as-is. A long one is reported by size, because the
    whole point of this formatter is that a 400-line RTL dump must not reach
    the log.
    """
    raw = str(text or "")
    lines = raw.splitlines()
    if not lines:
        return ""
    if len(lines) <= 3 and len(raw) <= MAX_RESULT:
        return squash(raw, MAX_RESULT)
    head = squash(next((l for l in lines if l.strip()), ""), MAX_RESULT - 24)
    return f"{head}  (+{len(lines) - 1} lines, {len(raw)}B)"


def tokens(usage: dict) -> str:
    def k(n):
        try:
            n = int(n)
        except (TypeError, ValueError):
            return "?"
        return f"{n / 1000:.1f}k" if n >= 1000 else str(n)
    if not isinstance(usage, dict):
        return ""
    parts = [f"{k(usage.get('input_tokens'))} in", f"{k(usage.get('output_tokens'))} out"]
    reasoning = usage.get("reasoning_output_tokens")
    if reasoning:
        parts.append(f"{k(reasoning)} reasoning")
    return " / ".join(parts)


def duration(started: float) -> str:
    secs = int(time.time() - started)
    if secs < 60:
        return f"{secs}s"
    return f"{secs // 60}m{secs % 60:02d}s"


class State:
    def __init__(self) -> None:
        self.started = time.time()
        self.commands = 0
        self.last_text = ""


def handle(event: dict, state: State, show_text: bool) -> None:
    etype = event.get("type")

    if etype == "thread.started":
        emit(f"{stamp()} ▶ codex thread {event.get('thread_id', '?')}")
        return

    if etype in ("item.started", "item.completed"):
        item = event.get("item") or {}
        itype = item.get("type")

        if itype == "command_execution":
            if etype == "item.started":
                state.commands += 1
                emit(f"{stamp()} → Bash  {squash(strip_shell(item.get('command', '')), MAX_INPUT)}")
            else:
                out = summarise_output(item.get("aggregated_output"))
                code = item.get("exit_code")
                mark = "←" if code in (0, None) else f"← exit {code}:"
                if out:
                    emit(f"{stamp()}   {mark} {out}")
                elif code not in (0, None):
                    emit(f"{stamp()}   {mark}")
            return

        if itype == "agent_message" and etype == "item.completed":
            text = squash(item.get("text", ""), MAX_INPUT)
            if show_text and text and text != state.last_text:
                emit(f"{stamp()} · {text}")
                state.last_text = text
            return

        if itype == "reasoning" and etype == "item.completed":
            if show_text:
                text = squash(item.get("text") or item.get("summary") or "", MAX_INPUT)
                if text:
                    emit(f"{stamp()} · {text}")
            return

        if itype == "file_change" and etype == "item.completed":
            changes = item.get("changes") or item.get("files") or []
            names = ", ".join(str(c.get("path", c)) if isinstance(c, dict) else str(c)
                              for c in changes) if isinstance(changes, list) else str(changes)
            emit(f"{stamp()} ✎ {squash(names, MAX_INPUT)}")
            return

        if etype == "item.completed" and itype:
            # An item kind this formatter has not been taught: name it rather
            # than drop it, so a codex update shows up instead of vanishing.
            emit(f"{stamp()} · [{itype}] {squash(json.dumps(item), MAX_INPUT)}")
        return

    if etype == "turn.completed":
        tok = tokens(event.get("usage") or {})
        emit(f"{stamp()} ✓ done in {duration(state.started)} · "
             f"{state.commands} cmds{' · ' + tok if tok else ''}")
        return

    if etype in ("turn.failed", "error"):
        err = event.get("error") or event.get("message") or event
        emit(f"{stamp()} ✗ {squash(json.dumps(err) if isinstance(err, dict) else err, MAX_RESULT)}")
        return


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--quiet-text",
        action="store_true",
        help="Log commands and results only, no agent commentary.",
    )
    args = parser.parse_args(argv)

    state = State()
    for line in sys.stdin:
        line = line.rstrip("\n")
        stripped = line.strip()
        if not stripped.startswith("{"):
            if stripped:
                emit(line)
            continue
        try:
            event = json.loads(stripped)
        except json.JSONDecodeError:
            emit(line)
            continue
        try:
            handle(event, state, show_text=not args.quiet_text)
        except Exception as exc:  # never let formatting kill a lane
            emit(f"{stamp()} ! codex_format: {exc}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
