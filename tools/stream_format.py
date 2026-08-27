#!/usr/bin/env python3
"""Render `claude -p --output-format stream-json` NDJSON as readable log lines.

Headless `claude -p` prints only the final result, so a vacuum iteration shows
nothing until the agent is done. Piping the stream-json events through this
gives the same running commentary the grok CLI writes, plus the tool calls:

    [17:20:51] · Reading BRIEF.md before touching base.c.
    [17:20:53] → Bash  ./build.sh base_1.c
    [17:20:59]   ← base_1.c 92.632%
    [17:21:10] ✓ done in 4m12s · 37 turns · $1.83

Unrecognised lines are passed through untouched so nothing is ever swallowed.
"""

from __future__ import annotations

import argparse
import json
import sys
import time

MAX_INPUT = 200
MAX_RESULT = 200

# tool name -> input keys to summarise, in order of preference
TOOL_KEYS = {
    "Bash": ("command",),
    "Read": ("file_path",),
    "Write": ("file_path",),
    "Edit": ("file_path",),
    "NotebookEdit": ("notebook_path",),
    "Glob": ("pattern",),
    "Grep": ("pattern",),
    "Task": ("description",),
    "Skill": ("skill",),
    "WebFetch": ("url",),
    "WebSearch": ("query",),
}


def stamp() -> str:
    return time.strftime("[%H:%M:%S]")


def emit(line: str) -> None:
    print(line, flush=True)


def squash(text: str, limit: int) -> str:
    text = " ".join(str(text).split())
    if len(text) > limit:
        text = text[: limit - 1] + "…"
    return text


def summarise_input(name: str, tool_input: dict) -> str:
    if not isinstance(tool_input, dict):
        return squash(tool_input, MAX_INPUT)
    for key in TOOL_KEYS.get(name, ()):
        if tool_input.get(key):
            return squash(tool_input[key], MAX_INPUT)
    if not tool_input:
        return ""
    return squash(json.dumps(tool_input), MAX_INPUT)


def content_text(content) -> str:
    if isinstance(content, str):
        return content
    if isinstance(content, list):
        parts = []
        for block in content:
            if isinstance(block, dict) and block.get("type") == "text":
                parts.append(block.get("text", ""))
            elif isinstance(block, str):
                parts.append(block)
        return "\n".join(parts)
    return ""


def duration(ms) -> str:
    try:
        secs = int(ms) // 1000
    except (TypeError, ValueError):
        return "?"
    if secs < 60:
        return f"{secs}s"
    return f"{secs // 60}m{secs % 60:02d}s"


LAST_TEXT = ""


def handle(event: dict, show_text: bool) -> None:
    global LAST_TEXT
    etype = event.get("type")

    if etype == "system" and event.get("subtype") == "init":
        emit(f"{stamp()} ▶ claude session {event.get('session_id', '?')}")
        return

    if etype in ("assistant", "user"):
        message = event.get("message") or {}
        blocks = message.get("content")
        if isinstance(blocks, str):
            blocks = [{"type": "text", "text": blocks}]
        for block in blocks or []:
            if not isinstance(block, dict):
                continue
            btype = block.get("type")
            if btype == "text":
                text = block.get("text", "").strip()
                if text:
                    LAST_TEXT = text
                if text and show_text:
                    for line in text.splitlines():
                        emit(f"{stamp()} · {line}" if line.strip() else "")
            elif btype == "thinking" and show_text:
                text = squash(block.get("thinking", ""), MAX_INPUT)
                if text:
                    emit(f"{stamp()} ~ {text}")
            elif btype == "tool_use":
                name = block.get("name", "tool")
                emit(f"{stamp()} → {name}  {summarise_input(name, block.get('input'))}".rstrip())
            elif btype == "tool_result":
                body = squash(content_text(block.get("content")), MAX_RESULT)
                mark = "✗" if block.get("is_error") else "←"
                emit(f"{stamp()}   {mark} {body}".rstrip())
        return

    if etype == "result":
        # The result carries the final assistant text again, plus the reason a
        # run ended early (max turns, API error). Print it unless it is a
        # verbatim repeat of text already logged, so callers that grep the
        # captured output (e.g. vacuum's stop phrase) always see it.
        text = (event.get("result") or "").strip()
        if text and (not show_text or text != LAST_TEXT):
            emit(text)
        cost = event.get("total_cost_usd")
        bits = [duration(event.get("duration_ms"))]
        if event.get("num_turns") is not None:
            bits.append(f"{event['num_turns']} turns")
        if isinstance(cost, (int, float)):
            bits.append(f"${cost:.2f}")
        mark = "✗" if event.get("is_error") else "✓"
        emit(f"{stamp()} {mark} {event.get('subtype', 'done')} · {' · '.join(bits)}")
        return


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--quiet-text",
        action="store_true",
        help="Log only tool calls and the final result, not the running commentary",
    )
    args = parser.parse_args(argv)

    for line in sys.stdin:
        line = line.strip()
        if not line:
            continue
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            emit(line)
            continue
        if not isinstance(event, dict):
            emit(line)
            continue
        try:
            handle(event, show_text=not args.quiet_text)
        except Exception as exc:  # never lose the stream to a format bug
            emit(f"{stamp()} ! stream_format: {exc}: {squash(line, MAX_INPUT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
