"""A sound bank as files: `<name>.sfz` (its programs and layers), `<name>.sfx` (its id, effect scripts and any pool
bytes that are not a sample), and its samples, each a WAV of its own (sound/wav.py) listed in pool order by the bank's
record (`samples`; None where the pool holds raw bytes).

    write(blob, sfz_path, sfx_path, sample_files, title, octaves, names)
                                    sample_files[i]: pool sample i's WAV, from the .sfz; octaves[i]: its rate's
                                    (sound/pitch.py); names: {sound name: slot} (the record's `sounds`)
    blob = build(sfz_path, sfx_path, sample_files, bodies, octaves) bodies[i]: pool sample i's ADPCM (sound/adpcm.py)
"""
from __future__ import annotations

from pathlib import Path

from . import codec, script, sfz


def write(blob: bytes, sfz_path: Path, sfx_path: Path, sample_files: list[str | None], title: str,
          octaves: dict[int, int] | None = None, names: dict[str, int] | None = None) -> dict:
    """The bank's two files from its bytes; returns the decoded parts (with each pool sample's body). `names` are
    its sounds' names by slot (the record's `sounds`); each must name a slot the bank fills."""
    bank = codec.decode(blob)
    for name, slot in (names or {}).items():
        if not (0 <= slot < len(bank["slots"])) or bank["slots"][slot] is None:
            raise ValueError(f"{title}: sound {name} names slot {slot:#x}, which the bank leaves empty")
    raw = {i: item["raw"] for i, item in enumerate(bank["pool"]) if "raw" in item}
    files = [f if f is not None else f"raw:{i}" for i, f in enumerate(sample_files)]
    sfz.write(bank["programs"], files, sfz_path, title, octaves)
    sfx_path.write_text(script.write(bank["chunks"], bank["slots"], title, bank["bank_id"], raw,
                                     {slot: name for name, slot in (names or {}).items()}))
    return bank


def build(sfz_path: Path, sfx_path: Path, sample_files: list[str | None], bodies: list[bytes | None],
          octaves: dict[int, int] | None = None) -> bytes:
    """The bank's bytes from its two files and its pool samples' bodies (None where the pool holds raw bytes)."""
    chunks, slots, bank_id, raw = script.read(sfx_path.read_text())
    index = {f: i for i, f in enumerate(sample_files) if f is not None}
    programs = sfz.read(sfz_path, index, octaves)
    pool = []
    for i, body in enumerate(bodies):
        if body is None:
            if i not in raw:
                raise ValueError(f"{sfx_path}: pool item {i} has neither a sample nor raw bytes")
            pool.append({"raw": raw[i]})
        else:
            pool.append({"sample": body})
    return codec.encode({"bank_id": bank_id, "programs": programs, "chunks": chunks, "slots": slots, "pool": pool})
