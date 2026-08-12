#!/usr/bin/env python3
"""Extract PE2 audio streams (MTS) into the peassets type-store layout.

Writes under the assets root (default ``assets/USA``)::

    raw/audio/{stem}.mts     on-disc sector payload
    audio/{stem}.wav         decoded stereo PCM (~22050 Hz)
    audio/{stem}.json        geometry + descriptor meta
    audio/streams.json       catalog of all streams

Usage::

    python3 tools/peassets/extract_streams.py --rom rom/USA --out assets/USA
"""

from __future__ import annotations

import argparse
import json
import logging
import sys
import traceback
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator

_SCRIPT_DIR = Path(__file__).resolve().parent
if str(_SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(_SCRIPT_DIR))

from format import (  # noqa: E402
    SECTOR_SIZE,
    STAGE0_STREAMING_COUNT,
    STAGE_N_FOLDER_LIST_COUNT,
    STAGE_N_STREAMING_COUNT,
    StreamingListAudioEntry,
    parse_folder_list_entry,
    parse_streaming_list_entry,
    streaming_entry_to_json,
)
from mts_codec import (  # noqa: E402
    extract_mts_asset,
    find_first_mts_sector,
    probe_mts_stream,
)
from parallel_util import default_jobs, run_jobs  # noqa: E402

RAW_AUDIO_DIR = "raw/audio"
AUDIO_DIR = "audio"


@dataclass
class AudioStreamRef:
    disk: str
    stage: int
    folder_id: int | None  # None for STAGE0
    slot: int
    entry: StreamingListAudioEntry
    # Byte offset of payload inside the stage CDF file
    cdf_offset: int
    cdf_path: Path


def _iter_stage0_audio(disk: str, disk_dir: Path) -> Iterator[AudioStreamRef]:
    hed = disk_dir / "STAGE0.HED"
    cdf = disk_dir / "STAGE0.CDF"
    if not hed.is_file() or not cdf.is_file():
        return
    h = hed.read_bytes()
    for i in range(STAGE0_STREAMING_COUNT):
        raw = h[i * 0x28 : (i + 1) * 0x28]
        if raw == b"\x00" * 0x28:
            continue
        if int.from_bytes(raw[0:2], "little") != 2:
            continue
        entry = parse_streaming_list_entry(raw)
        assert isinstance(entry, StreamingListAudioEntry)
        # STAGE0: offset is stage-absolute sectors
        yield AudioStreamRef(
            disk=disk,
            stage=0,
            folder_id=None,
            slot=i,
            entry=entry,
            cdf_offset=entry.offset_stage,
            cdf_path=cdf,
        )


def _iter_stage_n_audio(
    disk: str, disk_dir: Path, stage: int
) -> Iterator[AudioStreamRef]:
    cdf_path = disk_dir / f"STAGE{stage}.CDF"
    if not cdf_path.is_file():
        return
    cdf = cdf_path.read_bytes()
    folder_offset = 0x800
    for fi in range(STAGE_N_FOLDER_LIST_COUNT):
        fl = parse_folder_list_entry(cdf[fi * 8 : fi * 8 + 8])
        if fl.folder_id == 0 and fl.folder_size == 0:
            continue
        root = folder_offset
        stream_base = root + 0x514
        for i in range(STAGE_N_STREAMING_COUNT):
            raw = cdf[stream_base + i * 0x28 : stream_base + (i + 1) * 0x28]
            if raw == b"\x00" * 0x28:
                continue
            if int.from_bytes(raw[0:2], "little") != 2:
                continue
            entry = parse_streaming_list_entry(raw)
            assert isinstance(entry, StreamingListAudioEntry)
            # Stored sectors are folder-relative
            sec = int.from_bytes(raw[4:8], "little")
            abs_off = root + sec * SECTOR_SIZE
            yield AudioStreamRef(
                disk=disk,
                stage=stage,
                folder_id=fl.folder_id,
                slot=i,
                entry=entry,
                cdf_offset=abs_off,
                cdf_path=cdf_path,
            )
        folder_offset += fl.folder_size


def iter_audio_streams(rom_usa: Path) -> Iterator[AudioStreamRef]:
    """Yield unique streams across disk1/disk2 (dedup by stage/folder/id/slot)."""
    seen: set[tuple[int, int | None, int, int]] = set()
    for disk in ("disk1", "disk2"):
        d = rom_usa / disk
        if not d.is_dir():
            continue
        for ref in _iter_stage0_audio(disk, d):
            key = (0, None, ref.entry.stream_id, ref.entry.stage_number)
            if key in seen:
                continue
            seen.add(key)
            yield ref
        for stage in range(1, 6):
            for ref in _iter_stage_n_audio(disk, d, stage):
                key = (
                    stage,
                    ref.folder_id,
                    ref.entry.stream_id,
                    ref.slot,
                )
                if key in seen:
                    continue
                seen.add(key)
                yield ref


def _stem_for(ref: AudioStreamRef) -> str:
    if ref.folder_id is None:
        return f"stage{ref.stage}_id{ref.entry.stream_id}"
    return (
        f"stage{ref.stage}_folder{ref.folder_id}"
        f"_id{ref.entry.stream_id}_s{ref.slot}"
    )


def _span_bytes(
    cdf: bytes, *, cdf_offset: int, cdf_name: str, desc_sec: int | None
) -> tuple[bytes, int | None]:
    """Slice raw payload from CDF; return (bytes, descriptor_sectors or None)."""
    off = cdf_offset
    if off >= len(cdf):
        raise ValueError(f"offset 0x{off:X} past end of {cdf_name}")

    rest = cdf[off:]
    pre = find_first_mts_sector(rest)
    if pre is None:
        raise ValueError("no MTS header near stream start")

    info = probe_mts_stream(rest, descriptor_sectors=desc_sec)
    nsec = info.total_sectors
    nbytes = nsec * SECTOR_SIZE
    end = min(off + nbytes, len(cdf))
    return cdf[off:end], desc_sec


# Per-process CDF cache for parallel workers
_WORKER_CDF_CACHE: dict[str, bytes] = {}


def _worker_read_cdf(path: str) -> bytes:
    data = _WORKER_CDF_CACHE.get(path)
    if data is None:
        data = Path(path).read_bytes()
        _WORKER_CDF_CACHE[path] = data
    return data


def _extract_one_audio_job(job: dict[str, Any]) -> dict[str, Any]:
    """Process-pool worker: slice one MTS stream and write raw/wav/json."""
    stem = job["stem"]
    try:
        cdf = _worker_read_cdf(job["cdf_path"])
        raw, desc_sec = _span_bytes(
            cdf,
            cdf_offset=job["cdf_offset"],
            cdf_name=job["cdf_name"],
            desc_sec=job["desc_sec"],
        )
        raw_dir = Path(job["raw_dir"]) if job["raw_dir"] else None
        audio_dir = Path(job["audio_dir"]) if job["audio_dir"] else None
        meta = extract_mts_asset(
            raw,
            stem=stem,
            raw_dir=raw_dir,
            audio_dir=audio_dir,
            descriptor_sectors=desc_sec,
            write_raw=job["write_raw"],
            write_wav=job["write_wav"],
        )
        meta["descriptor"] = job["descriptor"]
        meta["disk"] = job["disk"]
        meta["stage"] = job["stage"]
        meta["folder_id"] = job["folder_id"]
        meta["slot"] = job["slot"]
        meta["cdf_offset"] = f"0x{job['cdf_offset']:X}"
        meta["cdf"] = job["cdf_name"]
        if job["write_wav"] and audio_dir is not None:
            (audio_dir / f"{stem}.json").write_text(
                json.dumps(meta, indent=2) + "\n", encoding="utf-8"
            )
        return {"ok": True, "meta": meta, "stem": stem}
    except Exception as ex:
        return {
            "ok": False,
            "stem": stem,
            "error": str(ex),
            "traceback": traceback.format_exc(),
            "disk": job.get("disk"),
            "stage": job.get("stage"),
            "folder_id": job.get("folder_id"),
            "stream_id": job.get("stream_id"),
            "cdf_offset": f"0x{job.get('cdf_offset', 0):X}",
        }


def extract_all(
    rom_usa: Path,
    assets_root: Path,
    *,
    write_raw: bool = True,
    write_wav: bool = True,
    limit: int | None = None,
    jobs: int | None = None,
) -> list[dict[str, Any]]:
    """Extract into ``assets_root/raw/audio`` and ``assets_root/audio``."""
    assets_root = Path(assets_root)
    raw_dir = assets_root / "raw" / "audio"
    audio_dir = assets_root / "audio"
    if write_raw:
        raw_dir.mkdir(parents=True, exist_ok=True)
    if write_wav:
        audio_dir.mkdir(parents=True, exist_ok=True)

    refs = list(iter_audio_streams(rom_usa))
    if limit is not None:
        refs = refs[:limit]

    n_jobs = default_jobs() if jobs is None else max(1, jobs)
    logging.info(
        "Extracting %d audio stream(s) with %d worker(s)", len(refs), n_jobs
    )

    work: list[dict[str, Any]] = []
    for ref in refs:
        e = ref.entry
        work.append(
            {
                "stem": _stem_for(ref),
                "cdf_path": str(ref.cdf_path.resolve()),
                "cdf_name": ref.cdf_path.name,
                "cdf_offset": ref.cdf_offset,
                "desc_sec": e.unknown3 if e.unknown3 else None,
                "raw_dir": str(raw_dir) if write_raw else None,
                "audio_dir": str(audio_dir) if write_wav else None,
                "write_raw": write_raw,
                "write_wav": write_wav,
                "descriptor": streaming_entry_to_json(e),
                "disk": ref.disk,
                "stage": ref.stage,
                "folder_id": ref.folder_id,
                "slot": ref.slot,
                "stream_id": e.stream_id,
            }
        )

    results = run_jobs(_extract_one_audio_job, work, jobs=n_jobs, label_key="stem")
    by_stem = {r.get("stem"): r for r in results}

    catalog: list[dict[str, Any]] = []
    for job in work:
        stem = job["stem"]
        r = by_stem.get(stem)
        if r is None:
            catalog.append({"stem": stem, "error": "missing result"})
            continue
        if r.get("ok"):
            meta = r["meta"]
            catalog.append(meta)
            logging.info(
                "OK %s  %.1fs  → %s + %s",
                stem,
                meta.get("duration_sec", 0),
                f"{RAW_AUDIO_DIR}/{stem}.mts" if write_raw else "-",
                f"{AUDIO_DIR}/{stem}.wav" if write_wav else "-",
            )
        else:
            if r.get("traceback"):
                logging.error("FAIL %s:\n%s", stem, r["traceback"])
            else:
                logging.error("FAIL %s: %s", stem, r.get("error"))
            catalog.append(
                {
                    "stem": stem,
                    "error": r.get("error", "unknown"),
                    "disk": r.get("disk", job["disk"]),
                    "stage": r.get("stage", job["stage"]),
                    "folder_id": r.get("folder_id", job["folder_id"]),
                    "stream_id": r.get("stream_id", job["stream_id"]),
                    "cdf_offset": r.get("cdf_offset", f"0x{job['cdf_offset']:X}"),
                }
            )

    audio_dir.mkdir(parents=True, exist_ok=True)
    (audio_dir / "streams.json").write_text(
        json.dumps(catalog, indent=2) + "\n", encoding="utf-8"
    )
    ok = sum(1 for c in catalog if "error" not in c)
    logging.info(
        "Done: %d ok / %d total → %s/ and %s/",
        ok,
        len(catalog),
        RAW_AUDIO_DIR,
        AUDIO_DIR,
    )
    return catalog


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--rom",
        type=Path,
        default=Path("rom/USA"),
        help="Path to rom/USA (disk1, disk2)",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=Path("assets/USA"),
        help="Assets root (writes raw/audio/ and audio/ underneath)",
    )
    ap.add_argument("--no-raw", action="store_true", help="Skip writing raw/audio/*.mts")
    ap.add_argument("--no-wav", action="store_true", help="Skip WAV decode")
    ap.add_argument("--limit", type=int, default=None, help="Max streams (debug)")
    ap.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=None,
        help="Parallel workers (default: min(cpu_count, 16))",
    )
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args(argv)

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )

    rom = args.rom
    if not rom.is_dir():
        logging.error("rom path not found: %s", rom)
        return 1

    extract_all(
        rom,
        args.out,
        write_raw=not args.no_raw,
        write_wav=not args.no_wav,
        limit=args.limit,
        jobs=args.jobs,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
