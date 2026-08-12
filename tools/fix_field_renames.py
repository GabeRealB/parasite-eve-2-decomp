#!/usr/bin/env python3
"""Per-function proven field renames. Does not leak arg0 types across functions."""
from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

MAPS: dict[str, dict[str, str]] = {
    "Task": {
        "field_8": "parent",
        "field_c": "firstChild",
        "field_C": "firstChild",
        "field_10": "nextSibling",
        "field_14": "callback",
        "field_18": "exitCallback",
        "field_1C": "idMap",
        "field_20": "spawnArg2",
        "field_28": "spawnType",
        "field_29": "priority",
        "field_2a": "killCountdown",
        "field_2A": "killCountdown",
        "field_2c": "extra",
        "field_2C": "extra",
        "field_30": "state",
        "field_34": "spawnArg1",
        "field_38": "flags",
        "field_3c": "extraState",
        "field_3C": "extraState",
    },
    "TaskDesc": {"field_2": "priority", "field_8": "setupArg"},
    "TaskIdMap": {"field_0": "index", "field_4": "table"},
    "TaskIdPair": {"field_0": "id", "field_1": "type"},
    "DisplayState": {
        "field_18": "width",
        "field_1a": "height",
        "field_1A": "height",
        "field_1c": "interlace",
        "field_1C": "interlace",
        "field_20": "dispEnv",
        "field_48": "drawEnv",
        "field_108": "vsyncFlag",
        "field_109": "vramYOffset",
        "field_118": "frameMode",
        "field_123": "skipTeardown",
        "field_124": "region",
    },
    "GpuOtBuf": {"field_0": "depth", "field_4": "ot", "field_10": "lastTag"},
    "PadState": {
        "field_0": "status",
        "field_2": "eventIdx",
        "field_3": "initialized",
        "field_4": "buttons",
        "field_6": "prevButtons",
        "field_8": "triggered",
        "field_A": "cooldown",
        "field_B": "autoRepeat",
        "field_10": "events",
    },
    "TextDrawReq": {
        "field_0": "x",
        "field_2": "y",
        "field_4": "otIndex",
        "field_C": "glyphTable",
        "field_D": "centerMode",
        "field_F": "vBias",
    },
    "PrimDrawParams": {
        "field_0": "x",
        "field_2": "y",
        "field_4": "u",
        "field_6": "v",
        "field_8": "w",
        "field_A": "h",
        "field_C": "r",
        "field_D": "g",
        "field_E": "b",
        "field_10": "shadeMode",
    },
    "TextStream": {
        "field_0": "x",
        "field_2": "y",
        "field_4": "tpageX",
        "field_6": "tpageY",
        "field_8": "clutX",
        "field_A": "clutY",
        "field_C": "charDelay",
        "field_E": "cursor",
        "field_10": "chars",
        "field_14": "glyphs",
        "field_18": "lineHeight",
        "field_1A": "delayReload",
        "field_1a": "delayReload",
    },
    "GfxImageSlot": {"field_0": "pixels", "field_4": "size"},
    "AudioTickNode": {
        "field_0": "poll",
        "field_4": "onRemove",
        "field_8": "id",
        "field_c": "arg",
        "field_C": "arg",
        "field_10": "prev",
        "field_14": "next",
    },
    "SpuVoiceRange": {"field_0": "first", "field_2": "count"},
    "SndEvt": {
        "field_0": "allocated",
        "field_2": "handlerIdx",
        "field_14": "prev",
        "field_18": "next",
    },
    "UiObject": {
        "field_0": "status",
        "field_8": "mode",
        "field_14": "drawOrder",
        "field_16": "timer",
        "field_20": "baseX",
        "field_22": "baseY",
        "field_24": "callback",
        "field_28": "owner",
    },
    "DialogOption": {"field_0": "text", "field_4": "next"},
}

TYPES = sorted(MAPS, key=len, reverse=True)
TYPE_ALT = "|".join(re.escape(t) for t in TYPES)
TYPE_DECL = re.compile(
    rf"(?:register\s+|volatile\s+|const\s+)*({TYPE_ALT})\s*\*+\s*([A-Za-z_]\w*)"
)
CAST_MEM = re.compile(
    rf"\(\s*({TYPE_ALT})\s*\*\s*\)\s*(?:[A-Za-z_]\w*|\([^)]+\))\s*->\s*(field_[0-9A-Fa-f]+)\b"
)
MEMBER = re.compile(r"\b([A-Za-z_]\w*)\s*(->|\.)\s*(field_[0-9A-Fa-f]+)\b")
FUNC_START = re.compile(r"^[A-Za-z_].*\([^;]*\)\s*(\{|$)")


def apply_env(text: str, env: dict[str, str]) -> tuple[str, int]:
    n = 0

    def repl(m: re.Match[str]) -> str:
        nonlocal n
        var, op, field = m.group(1), m.group(2), m.group(3)
        ty = env.get(var)
        if not ty:
            return m.group(0)
        mapping = MAPS[ty]
        if field not in mapping:
            return m.group(0)
        n += 1
        return f"{var}{op}{mapping[field]}"

    text = MEMBER.sub(repl, text)

    def repl_cast(m: re.Match[str]) -> str:
        nonlocal n
        ty, field = m.group(1), m.group(2)
        mapping = MAPS.get(ty)
        if not mapping or field not in mapping:
            return m.group(0)
        n += 1
        return m.group(0)[: -len(field)] + mapping[field]

    text = CAST_MEM.sub(repl_cast, text)
    return text, n


def rewrite_file(text: str) -> tuple[str, int]:
    total = 0
    # always-on globals
    gtext, n = apply_env(
        text,
        {"Display_State": "DisplayState"},
    )
    # only replace Display_State.* from that pass; don't keep leaking.
    # Re-do properly by splitting functions.
    text = text  # noqa

    lines = text.splitlines(keepends=True)
    out: list[str] = []
    i = 0
    global_env = {"Display_State": "DisplayState"}
    total = 0

    def flush_chunk(chunk: str, env: dict[str, str]) -> None:
        nonlocal total
        # seed env from decls in this chunk
        local = dict(env)
        for m in TYPE_DECL.finditer(chunk):
            local[m.group(2)] = m.group(1)
        new, n = apply_env(chunk, local)
        total += n
        out.append(new)

    # preamble until first function
    preamble: list[str] = []
    while i < len(lines):
        if FUNC_START.match(lines[i]) and not lines[i].lstrip().startswith("if "):
            break
        preamble.append(lines[i])
        i += 1
    flush_chunk("".join(preamble), global_env)

    while i < len(lines):
        # gather function header + body until next top-level func or EOF
        chunk = [lines[i]]
        i += 1
        while i < len(lines) and not FUNC_START.match(lines[i]):
            chunk.append(lines[i])
            i += 1
        flush_chunk("".join(chunk), global_env)

    # Gpu_OtBuffers[i].field
    result = "".join(out)

    def repl_ot(m: re.Match[str]) -> str:
        nonlocal total
        field = m.group(1)
        mapping = MAPS["GpuOtBuf"]
        if field not in mapping:
            return m.group(0)
        total += 1
        return m.group(0)[: -len(field)] + mapping[field]

    result = re.sub(
        r"\bGpu_OtBuffers\[[^\]]+\]\s*\.\s*(field_[0-9A-Fa-f]+)\b",
        repl_ot,
        result,
    )
    return result, total


def main() -> int:
    files = list((ROOT / "src").rglob("*.c")) + list((ROOT / "include" / "main").rglob("*.h"))
    changed = 0
    subs = 0
    for path in files:
        raw = path.read_text(encoding="utf-8")
        new, n = rewrite_file(raw)
        if n:
            path.write_text(new, encoding="utf-8")
            changed += 1
            subs += n
            print(f"  {n:4d}  {path.relative_to(ROOT)}")
    print(f"\nUpdated {changed} files ({subs} substitutions)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
