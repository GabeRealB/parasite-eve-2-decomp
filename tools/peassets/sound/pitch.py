"""The rate a sample is written at: the octave of 44100 Hz its banks play it at.

The SPU plays a sample at 44100 Hz when a voice sounds the layer's root key, so a bank stores no recording rate of
its own; a WAV labelled 44100 Hz would play most of the game's samples an octave or two too high, since nearly all are
played well below unity (22050 Hz for 987 of the 1244 samples the scripts play, 11025 Hz for 192). The decoder
chooses a rate instead, the same way for every sample: each voice of every bank's scripts that plays it sounds the
layer's lowest key plus the voice's pitch offset, so many semitones from the root key (and its fine tune); the
sample's rate is the octave of 44100 Hz nearest the median of those. A sample no script plays is placed by its
layers' lowest keys. The rate is a label: the root key is stored exactly in the .sfz (`psx_root_key`), and its
`pitch_keycenter` is moved by the same octaves so that a sampler plays the sample as the game does.

    octaves = octaves_of(blobs)    {sample body's SHA-1: octave}, 0 = 44100 Hz, -1 = 22050 Hz, ...
    rate(octave) / octave(rate)
"""
from __future__ import annotations

import hashlib
import math
import statistics
from collections import defaultdict

from . import codec

UNITY = 44100


def rate(octave: int) -> int:
    return round(UNITY * 2.0 ** octave)


def octave(rate_hz: int) -> int:
    return round(math.log2(rate_hz / UNITY))


def _offsets(bank: dict) -> tuple[dict[str, list[float]], dict[str, list[float]]]:
    """(semitones from unity each script voice plays a sample at, those of its layers' lowest keys), by body SHA-1.
    A voice names a layer by its program and an index counted from the program's first layer (the interpreter does
    not bound it by the program; 30 voices on the disc address past the layer table and are not counted); a voice of
    another bank (`bank` set) is that bank's, whose id several files share, and is not counted."""
    layers = [L for program in bank["programs"] for L in program["layers"]]
    sha = {i: hashlib.sha1(item["sample"]).hexdigest() for i, item in enumerate(bank["pool"]) if "sample" in item}
    played, keyed = defaultdict(list), defaultdict(list)
    for L in layers:
        if L["sample"] in sha:
            keyed[sha[L["sample"]]].append(L["key_min"] - L["root_key"] - L["fine_tune"] / 128)
    for chunk in bank["chunks"]:
        for c in chunk.get("commands", ()):
            if c["op"] != "voice" or c.get("bank"):
                continue
            at = sum(len(p["layers"]) for p in bank["programs"][:c["program"]]) + c["layer"]
            if at >= len(layers) or layers[at]["sample"] not in sha:
                continue
            L = layers[at]
            pitch = ((c.get("pitch", 0) + 0x8000) & 0xFFFF) - 0x8000      # a signed Q7 offset from the lowest key
            played[sha[L["sample"]]].append(L["key_min"] + pitch / 128 - L["root_key"] - L["fine_tune"] / 128)
    return played, keyed


def octaves_of(blobs) -> dict[str, int]:
    played, keyed = defaultdict(list), defaultdict(list)
    for blob in blobs:
        p, k = _offsets(codec.decode(blob))
        for h, v in p.items():
            played[h] += v
        for h, v in k.items():
            keyed[h] += v
    return {h: round(statistics.median(played.get(h) or keyed[h]) / 12) for h in set(played) | set(keyed)}
