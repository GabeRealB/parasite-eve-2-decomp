"""A sample as a WAV file: 16-bit mono PCM at the rate the game plays it at (sound/pitch.py; 44100 Hz, the SPU's
unity pitch, unless stated), its loop in a standard `smpl` chunk.

    write(path, pcm, loop_start=None, rate=44100)   loop_start: the sample index the loop returns to; the loop runs
                                                    to the end
    pcm, loop_start = read(path)
    rate_of(path)

A loop is one `smpl` loop (forward, from `loop_start` to the last sample, played forever). Any other chunk is
ignored on reading; a file that is not 16-bit mono PCM is refused.
"""
from __future__ import annotations

import struct
from pathlib import Path

import numpy as np

RATE = 44100


def write(path: Path, pcm, loop_start: int | None = None, rate: int = RATE) -> None:
    pcm = np.asarray(pcm, dtype="<i2")
    data = pcm.tobytes()
    chunks = struct.pack("<4sIHHIIHH", b"fmt ", 16, 1, 1, rate, rate * 2, 2, 16)
    chunks += b"data" + struct.pack("<I", len(data)) + data + (b"\0" if len(data) & 1 else b"")
    if loop_start is not None:
        period = round(1e9 / rate)
        smpl = struct.pack("<IIIIIIIII", 0, 0, period, 60, 0, 0, 0, 1, 0)
        smpl += struct.pack("<IIIIII", 0, 0, loop_start, len(pcm) - 1, 0, 0)
        chunks += b"smpl" + struct.pack("<I", len(smpl)) + smpl
    Path(path).write_bytes(b"RIFF" + struct.pack("<I", 4 + len(chunks)) + b"WAVE" + chunks)


def read(path: Path):
    b = Path(path).read_bytes()
    if b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise ValueError(f"{path}: not a WAV file")
    at, fmt, pcm, loop = 12, None, None, None
    while at + 8 <= len(b):
        tag, size = b[at:at + 4], struct.unpack_from("<I", b, at + 4)[0]
        body = b[at + 8:at + 8 + size]
        if tag == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", body)
        elif tag == b"data":
            pcm = body
        elif tag == b"smpl" and struct.unpack_from("<I", body, 28)[0]:
            loop = struct.unpack_from("<I", body, 36 + 8)[0]
        at += 8 + size + (size & 1)
    if fmt is None or pcm is None:
        raise ValueError(f"{path}: no fmt or data chunk")
    tag, channels, _rate, _bps, _align, bits = fmt
    if tag != 1 or channels != 1 or bits != 16:
        raise ValueError(f"{path}: not 16-bit mono PCM (format {tag}, {channels} channels, {bits} bits)")
    return np.frombuffer(pcm, dtype="<i2").astype(np.int64), loop


def rate_of(path: Path) -> int:
    b = Path(path).read_bytes()
    at = 12
    while at + 8 <= len(b):
        tag, size = b[at:at + 4], struct.unpack_from("<I", b, at + 4)[0]
        if tag == b"fmt ":
            return struct.unpack_from("<I", b, at + 12)[0]
        at += 8 + size + (size & 1)
    raise ValueError(f"{path}: no fmt chunk")
