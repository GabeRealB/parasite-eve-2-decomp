"""Declared sound banks: a `sound_bank` record in place of a bank's `spk` record, and a `sound` record for each
sample of its pool (asset_schema.SCHEMA).

    decode_all(assets, root)        each declared bank's `<name>.sfz` and `<name>.sfx`, and each of its sounds once
                                    as `<name>.wav`, from the stored bank under raw/
    blob = build(name, assets, root)  the bank's bytes from those files

A bank's `samples` names the sound at each position of its pool, in pool order (None where the pool holds raw bytes,
which its .sfx keeps), and its `sounds` names its slots - the sound ids the game plays, `{name = slot}` - each a
slot the bank fills. A sample is a recording; a slot is a sound made from it, so one sample can stand under several
slots at different pitches (a footstep bank's walk and run steps). A sound states the SHA-1 of its stored body, and its `mode` where normal mode does not
reproduce that body (adpcm.decode); decoding checks both, so a declaration that no longer fits its bytes stops the
extraction.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path

import asset_schema
from parallel_util import run_jobs

from . import adpcm, bank, codec, pitch, wav


def declared(assets: dict) -> list[str]:
    return sorted(k for k, r in assets.items() if r.get("type") == "sound_bank")


def sound_path(name: str, assets: dict, root: Path) -> Path:
    return root / asset_schema.decoded_rel(name, assets[name])


def bank_paths(name: str, assets: dict, root: Path) -> tuple[Path, Path]:
    sfz = root / asset_schema.decoded_rel(name, assets[name])
    return sfz, sfz.with_suffix(".sfx")


def _sample_files(name: str, assets: dict, root: Path) -> list[str | None]:
    """How the bank's .sfz names each pool sample: the path from the bank's directory to its WAV."""
    sfz, _ = bank_paths(name, assets, root)
    return [None if s is None else os.path.relpath(sound_path(s, assets, root), sfz.parent).replace(os.sep, "/")
            for s in assets[name]["samples"]]


def _pool(name: str, assets: dict, root: Path) -> tuple[bytes, list[dict]]:
    """The stored bank and its pool, checked against the declaration."""
    rec = assets[name]
    blob = (root / asset_schema.raw_rel(name, rec)).read_bytes()
    pool = codec.decode(blob)["pool"]
    samples = rec.get("samples") or []
    if len(samples) != len(pool):
        raise ValueError(f"{name}: declares {len(samples)} pool samples, the bank holds {len(pool)}")
    for i, (item, sound) in enumerate(zip(pool, samples)):
        if "raw" in item:
            if sound is not None:
                raise ValueError(f"{name}: pool item {i} is raw bytes, declared as {sound}")
            continue
        srec = assets.get(sound) if sound else None
        if srec is None or srec.get("type") != "sound":
            raise ValueError(f"{name}: pool item {i} is declared as {sound!r}, which is not a sound")
        digest = hashlib.sha1(item["sample"]).hexdigest()
        if srec.get("sha1") != digest:
            raise ValueError(f"{name}: pool item {i} has sha1 {digest}; {sound} declares {srec.get('sha1')}")
    return blob, pool


def _decode_job(job: dict) -> dict:
    """Process-pool worker: one sound's body -> its WAV, in the mode it declares."""
    try:
        pcm, mode, loop = adpcm.decode(job["body"])
        if mode != job["mode"]:
            return {"ok": False, "name": job["name"], "error": f"declares mode {job['mode']}; its body needs {mode}"}
        dest = Path(job["dest"]); dest.parent.mkdir(parents=True, exist_ok=True)
        wav.write(dest, pcm, None if loop is None else 28 * loop, pitch.rate(job["octave"]))
        return {"ok": True, "name": job["name"]}
    except Exception as ex:  # noqa: BLE001 - reported per sound
        return {"ok": False, "name": job["name"], "error": str(ex)}


def all_octaves(assets: dict, root: Path) -> dict[str, int]:
    """Each sample body's octave (sound/pitch.py), from how every bank on the disc plays it."""
    return pitch.octaves_of((root / asset_schema.raw_rel(k, r)).read_bytes() for k, r in sorted(assets.items())
                            if r.get("type") in ("spk", "sound_bank") and "sha1" in r)


def decode_all(assets: dict, root: Path, jobs: int | None = None) -> None:
    work: dict[str, dict] = {}
    octaves = all_octaves(assets, root) if declared(assets) else {}
    for name in declared(assets):
        blob, pool = _pool(name, assets, root)
        mine = {}
        for i, (item, sound) in enumerate(zip(pool, assets[name]["samples"])):
            if sound is None:
                continue
            mine[i] = octaves.get(assets[sound]["sha1"], 0)
            if sound not in work:
                work[sound] = {"name": sound, "body": item["sample"], "mode": assets[sound].get("mode", "normal"),
                               "dest": str(sound_path(sound, assets, root)), "octave": mine[i]}
        sfz, sfx = bank_paths(name, assets, root)
        sfz.parent.mkdir(parents=True, exist_ok=True)
        bank.write(blob, sfz, sfx, _sample_files(name, assets, root), name, mine, assets[name].get("sounds"))
    failed = [r for r in run_jobs(_decode_job, sorted(work.values(), key=lambda j: -len(j["body"])), jobs=jobs,
                                  label_key="name") if not r.get("ok")]
    if failed:
        raise ValueError("sounds not decoded: " + "; ".join(f"{r['name']}: {r.get('error')}" for r in failed[:5]))


def build(name: str, assets: dict, root: Path) -> bytes:
    """The bank's bytes from its .sfz, its .sfx and its sounds' WAVs, each encoded in its mode."""
    samples = assets[name]["samples"]
    sounds, modes, loops = [], [], []
    for sound in samples:
        if sound is None:
            continue
        pcm, loop = wav.read(sound_path(sound, assets, root))
        if len(pcm) % 28:
            raise ValueError(f"{sound}: {len(pcm)} samples, not whole 28-sample frames")
        sounds.append(pcm); modes.append(assets[sound].get("mode", "normal"))
        loops.append(None if loop is None else loop // 28)
    bodies = iter(adpcm.encode(sounds, modes, loops))
    sfz, sfx = bank_paths(name, assets, root)
    octaves = {i: pitch.octave(wav.rate_of(sound_path(s, assets, root))) for i, s in enumerate(samples) if s is not None}
    return bank.build(sfz, sfx, _sample_files(name, assets, root), [None if s is None else next(bodies) for s in samples],
                      octaves)
