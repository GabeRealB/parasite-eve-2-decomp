"""A bank's programs and layers (codec.py's form) as SFZ, and back.

    write(programs, sample_files, path, title, octaves)   sample_files[i]: the WAV of pool sample i, relative to
                                                          the .sfz; octaves[i]: the octave of 44100 Hz it is written at
    programs = read(path, sample_index, octaves)          sample_index: WAV path -> pool sample index

Each program is a `<group>`, each of its layers a `<region>` in order. A region states what a sampler needs to play
it - `sample`, `lokey`/`hikey`, `pitch_keycenter`, `tune`, `volume`, `pan`, `bend_up`/`bend_down` - and the exact
values the bank stores as `psx_*` opcodes, which samplers ignore:

    psx_root_key               the key that plays the sample at 44100 Hz; `pitch_keycenter` is moved from it by the
                               octaves its WAV's rate is below 44100 (sound/pitch.py), so a sampler plays it right
    psx_volume, psx_pan        0..127 (the layer's gain and pan; a group's are its program's)
    psx_fine_tune              1/128 semitone steps (0..127)
    psx_priority               voice priority
    psx_adsr1, psx_adsr2       the SPU envelope registers
    psx_reverb                 reverb send (1 on)

Reading takes a `psx_*` value where it is given, else derives it from the standard opcode (a region written by hand):
volume in dB, pan -100..100, tune in cents, bend in cents. The SPU envelope has no standard counterpart that maps
back exactly, so a region without `psx_adsr1`/`psx_adsr2` gets the default (attack 0x80FF, release 0x5FC0).
"""
from __future__ import annotations

import math
from pathlib import Path

DEFAULT_ADSR = (0x80FF, 0x5FC0)


def _db(v: int) -> str:
    return "-144" if v == 0 else f"{20 * math.log10(v / 127):.2f}"


def _pan(p: int) -> str:
    return f"{(p - 64) * 100 / 64:.1f}"


def write(programs: list[dict], sample_files: list[str], path: Path, title: str,
          octaves: dict[int, int] | None = None) -> None:
    lines = [f"// {title}", "// psx_* opcodes are the bank's exact values; the standard ones play it in a sampler.", ""]
    for n, prog in enumerate(programs):
        lines += [f"<group> // program {n}", f"psx_volume={prog['volume']} psx_pan={prog['pan']}"]
        for L in prog["layers"]:
            vol = L["volume"] * prog["volume"] // 128
            pan = max(0, min(127, L["pan"] + prog["pan"] - 64))
            lines += ["<region>",
                      f"sample={sample_files[L['sample']]}",
                      f"lokey={L['key_min']} hikey={L['key_max']}"
                      f" pitch_keycenter={L['root_key'] + 12 * (octaves or {}).get(L['sample'], 0)}"
                      f" tune={round(L['fine_tune'] * 100 / 128)}",
                      f"volume={_db(vol)} pan={_pan(pan)} bend_up={L['bend_up'] * 100} bend_down={-L['bend_down'] * 100}",
                      f"psx_root_key={L['root_key']} psx_volume={L['volume']} psx_pan={L['pan']}"
                      f" psx_fine_tune={L['fine_tune']}"
                      f" psx_priority={L['priority']} psx_adsr1=0x{L['adsr1']:04X} psx_adsr2=0x{L['adsr2']:04X}"
                      f" psx_reverb={L['reverb']}"]
        lines.append("")
    path.write_text("\n".join(lines))


def _opcodes(text: str):
    """(header, {opcode: value}) in order; `sample=` runs to the end of its line (paths may hold spaces)."""
    current = None
    for raw in text.splitlines():
        line = raw.split("//", 1)[0].strip()
        while line:
            if line.startswith("<"):
                end = line.index(">")
                if current is not None:
                    yield current
                current = (line[1:end], {})
                line = line[end + 1:].strip()
                continue
            key, _, rest = line.partition("=")
            key = key.strip()
            if key == "sample":
                current[1][key] = rest.strip()
                break
            value, _, line = rest.strip().partition(" ")
            current[1][key] = value
            line = line.strip()
    if current is not None:
        yield current


def _num(v: str) -> int:
    return int(v, 0)


def read(path: Path, sample_index: dict[str, int], octaves: dict[int, int] | None = None) -> list[dict]:
    programs = []
    for header, op in _opcodes(path.read_text()):
        if header == "group":
            programs.append({"volume": _num(op.get("psx_volume", "127")), "pan": _num(op.get("psx_pan", "64")),
                             "layers": []})
        elif header == "region":
            if not programs:
                raise ValueError(f"{path}: a region before any group")
            vol = _num(op["psx_volume"]) if "psx_volume" in op else (
                0 if float(op.get("volume", 0)) <= -144 else round(127 * 10 ** (float(op.get("volume", 0)) / 20)))
            pan = _num(op["psx_pan"]) if "psx_pan" in op else round(64 + float(op.get("pan", 0)) * 64 / 100)
            fine = _num(op["psx_fine_tune"]) if "psx_fine_tune" in op else round(float(op.get("tune", 0)) * 128 / 100)
            sample = sample_index[op["sample"]]
            root = _num(op["psx_root_key"]) if "psx_root_key" in op else (
                _num(op.get("pitch_keycenter", "60")) - 12 * (octaves or {}).get(sample, 0))
            programs[-1]["layers"].append({
                "sample": sample,
                "volume": max(0, min(127, vol)), "pan": max(0, min(127, pan)),
                "root_key": root, "fine_tune": fine,
                "priority": _num(op.get("psx_priority", "0")),
                "key_min": _num(op.get("lokey", "0")), "key_max": _num(op.get("hikey", "127")),
                "bend_down": -_num(op.get("bend_down", "0")) // 100, "bend_up": _num(op.get("bend_up", "0")) // 100,
                "adsr1": _num(op.get("psx_adsr1", str(DEFAULT_ADSR[0]))),
                "adsr2": _num(op.get("psx_adsr2", str(DEFAULT_ADSR[1]))),
                "reverb": _num(op.get("psx_reverb", "0")),
            })
    return programs
