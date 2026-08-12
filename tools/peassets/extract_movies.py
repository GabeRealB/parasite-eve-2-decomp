#!/usr/bin/env python3
"""Extract PE2 STR movies from STAGE*.CDF / INTER*.STR.

Writes under the assets root (default ``assets/USA``)::

    raw/movie/{stem}.str   2048-byte STR video sector blob
    movie/{stem}.mp4       lossless H.264 (yuv444p crf0) + ALAC when XA present
    movie/{stem}.json      meta / descriptor
    movie/movies.json      catalog

Play with VLC/mpv/MPC-HC — not Windows Media Player (no H.264 4:4:4 / ALAC).

Usage::

    python3 tools/peassets/extract_movies.py --rom rom/USA --out assets/USA -j 16
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
    StreamingListMovieEntry,
    parse_folder_list_entry,
    parse_streaming_list_entry,
    streaming_entry_to_json,
)
from parallel_util import default_jobs, run_jobs  # noqa: E402
from str_codec import (  # noqa: E402
    STR_SECTOR_USER,
    align_str_window,
    extract_movie_asset,
    inter_raw_to_iso_user,
    parse_str_sector_header,
    _user_sector_inter,
    _user_sector_iso,
)
from xa_codec import demux_xa_from_inter  # noqa: E402

RAW_MOVIE_DIR = "raw/movie"
MOVIE_DIR = "movie"

# STR frame number at a true clip head is 0 or 1. Higher values mean this
# descriptor's interOffset lands mid-stream on *this* INTER file (usually the
# other language layout, which lives on the other disc).
_INTER_CLIP_START_MAX_FRAME = 1


@dataclass
class MovieStreamRef:
    disk: str
    stage: int
    folder_id: int | None
    slot: int
    entry: StreamingListMovieEntry
    # Where the payload lives
    source: str  # "cdf" | "inter"
    # Absolute path to container file
    container: Path
    # Byte offset into container for ISO (2048) or sector index for INTER
    start_offset: int  # ISO: byte offset; INTER: start sector index (2336)
    nsectors: int
    folder_root: int | None = None  # CDF folder root when applicable


def _iter_stage0_movies(disk: str, disk_dir: Path) -> Iterator[MovieStreamRef]:
    hed = disk_dir / "STAGE0.HED"
    if not hed.is_file():
        return
    inter = disk_dir / ("INTER0.STR" if disk == "disk1" else "INTER1.STR")
    if not inter.is_file():
        # fallback names
        cands = list(disk_dir.glob("INTER*.STR"))
        inter = cands[0] if cands else inter
    h = hed.read_bytes()
    for i in range(STAGE0_STREAMING_COUNT):
        raw = h[i * 0x28 : (i + 1) * 0x28]
        if raw == b"\x00" * 0x28:
            continue
        if int.from_bytes(raw[0:2], "little") != 1:
            continue
        entry = parse_streaming_list_entry(raw)
        assert isinstance(entry, StreamingListMovieEntry)
        # INTER seek = interOffset + INTER_LBA (func_8001F180). interOffset may
        # be 0 (STAGE0 title starts at file sector 0); do not fall back to offset.
        # unknown5 ≈ frame count, not sector count — span filled later.
        start_sec = entry.offset_inter // SECTOR_SIZE
        if not inter.is_file():
            logging.warning("missing %s for stage0 movie id=%s", inter, entry.stream_id)
            continue
        yield MovieStreamRef(
            disk=disk,
            stage=0,
            folder_id=None,
            slot=i,
            entry=entry,
            source="inter",
            container=inter,
            start_offset=start_sec,
            nsectors=entry.unknown5 or 1,  # provisional; resolved in extract_all
        )


def _iter_stage_n_movies(
    disk: str, disk_dir: Path, stage: int
) -> Iterator[MovieStreamRef]:
    cdf_path = disk_dir / f"STAGE{stage}.CDF"
    if not cdf_path.is_file():
        return
    inter = disk_dir / ("INTER0.STR" if disk == "disk1" else "INTER1.STR")
    if not inter.is_file():
        cands = list(disk_dir.glob("INTER*.STR"))
        inter = cands[0] if cands else inter

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
            if int.from_bytes(raw[0:2], "little") != 1:
                continue
            entry = parse_streaming_list_entry(raw)
            assert isinstance(entry, StreamingListMovieEntry)
            # unknown5 ≈ frame count (not sectors). Real span resolved later.
            nsec_hint = entry.unknown5 if entry.unknown5 else 1

            if entry.movie_number != 0:
                # INTER*: seek LBA = interOffset + INTER file LBA (func_8001F180).
                # interOffset==0 is valid (file start); never fall back to offset.
                start_sec = entry.offset_inter // SECTOR_SIZE
                if not inter.is_file():
                    logging.warning(
                        "missing INTER for stage%d folder%d movie",
                        stage,
                        fl.folder_id,
                    )
                    continue
                yield MovieStreamRef(
                    disk=disk,
                    stage=stage,
                    folder_id=fl.folder_id,
                    slot=i,
                    entry=entry,
                    source="inter",
                    container=inter,
                    start_offset=start_sec,
                    nsectors=nsec_hint,
                    folder_root=root,
                )
            else:
                # Folder-relative offset in CDF (bytes already)
                abs_off = root + entry.offset_folder
                yield MovieStreamRef(
                    disk=disk,
                    stage=stage,
                    folder_id=fl.folder_id,
                    slot=i,
                    entry=entry,
                    source="cdf",
                    container=cdf_path,
                    start_offset=abs_off,
                    nsectors=nsec_hint,
                    folder_root=root,
                )
        folder_offset += fl.folder_size


def iter_movie_streams(rom_usa: Path) -> Iterator[MovieStreamRef]:
    """Yield raw movie descriptors (both discs). Prefer :func:`collect_movie_streams`."""
    seen: set[tuple] = set()
    for disk in ("disk1", "disk2"):
        d = rom_usa / disk
        if not d.is_dir():
            continue
        for ref in _iter_stage0_movies(disk, d):
            key2 = (0, None, ref.entry.stream_id, ref.slot, ref.disk)
            if key2 in seen:
                continue
            seen.add(key2)
            yield ref
        for stage in range(1, 6):
            for ref in _iter_stage_n_movies(disk, d, stage):
                key = (
                    stage,
                    ref.folder_id,
                    ref.entry.stream_id,
                    ref.slot,
                    ref.disk,
                )
                if key in seen:
                    continue
                seen.add(key)
                yield ref


def _stem_for(ref: MovieStreamRef) -> str:
    e = ref.entry
    if ref.folder_id is None:
        return f"stage{ref.stage}_id{e.stream_id}"
    return (
        f"stage{ref.stage}_folder{ref.folder_id}"
        f"_id{e.stream_id}_s{ref.slot}"
    )


def _inter_start_frame(ref: MovieStreamRef, data: bytes) -> int | None:
    """Frame number at ``interOffset``, or None if not an STR video sector."""
    ud = _user_sector_inter(data, ref.start_offset)
    if not ud:
        return None
    hdr = parse_str_sector_header(ud)
    return hdr.frame if hdr is not None else None


def _ref_rank(ref: MovieStreamRef) -> tuple:
    """Lower is better when two descriptors claim the same INTER start."""
    return (
        ref.stage,
        ref.folder_id if ref.folder_id is not None else -1,
        ref.entry.stream_id,
        ref.slot,
        0 if ref.disk == "disk1" else 1,
    )


def collect_movie_streams(rom_usa: Path) -> list[MovieStreamRef]:
    """Collect movie refs with INTER validation and dedupe.

    Retail puts **both language layouts' sector indices** in every stage CDF,
    but each ``INTER*.STR`` only holds one layout:

    * Starts mid-stream (frame > 1) or non-STR → wrong disc for that offset.
    * Two descriptors can share a sector (stage0 id 100/101; stage1 reusing a
      later stage's language-pair offset) → keep the earliest stage owner.
    * Same stem on disk1 and disk2 after filtering → keep one (usually same
      video).
    """
    raw = list(iter_movie_streams(rom_usa))
    inter_cache: dict[Path, bytes] = {}

    # 1) Drop INTER starts that are not clip heads on this disc's INTER file.
    valid: list[MovieStreamRef] = []
    for ref in raw:
        if ref.source != "inter":
            valid.append(ref)
            continue
        data = inter_cache.setdefault(ref.container, ref.container.read_bytes())
        frame = _inter_start_frame(ref, data)
        if frame is None:
            logging.info(
                "skip %s (%s): no STR at INTER sector %s",
                _stem_for(ref),
                ref.disk,
                ref.start_offset,
            )
            continue
        if frame > _INTER_CLIP_START_MAX_FRAME:
            logging.info(
                "skip %s (%s): INTER sector %s is mid-stream (frame %s)",
                _stem_for(ref),
                ref.disk,
                ref.start_offset,
                frame,
            )
            continue
        valid.append(ref)

    # 2) One owner per (disk, container, start sector) for INTER.
    owner: dict[tuple[str, str, int], MovieStreamRef] = {}
    for ref in valid:
        if ref.source != "inter":
            continue
        key = (ref.disk, str(ref.container.resolve()), ref.start_offset)
        prev = owner.get(key)
        if prev is None or _ref_rank(ref) < _ref_rank(prev):
            if prev is not None:
                logging.info(
                    "INTER sector %s on %s: prefer %s over %s",
                    ref.start_offset,
                    ref.disk,
                    _stem_for(ref),
                    _stem_for(prev),
                )
            owner[key] = ref

    owned_ids = {id(r) for r in owner.values()}
    after_start: list[MovieStreamRef] = []
    for ref in valid:
        if ref.source != "inter":
            after_start.append(ref)
        elif id(ref) in owned_ids:
            after_start.append(ref)
        else:
            logging.info(
                "skip %s (%s): INTER sector %s owned by another stream",
                _stem_for(ref),
                ref.disk,
                ref.start_offset,
            )

    # 3) One extract per stem (disk1/disk2 duplicates after filtering).
    by_stem: dict[str, list[MovieStreamRef]] = {}
    for ref in after_start:
        by_stem.setdefault(_stem_for(ref), []).append(ref)

    out: list[MovieStreamRef] = []
    for stem, group in by_stem.items():
        if len(group) == 1:
            out.append(group[0])
            continue
        group_sorted = sorted(group, key=_ref_rank)
        keep = group_sorted[0]
        for extra in group_sorted[1:]:
            logging.info(
                "skip %s (%s): duplicate stem, keeping %s",
                stem,
                extra.disk,
                keep.disk,
            )
        out.append(keep)

    out.sort(key=_ref_rank)
    return out


def _container_starts(
    refs: list[MovieStreamRef],
) -> dict[tuple[str, Path], list[int]]:
    """Per container file, sorted start sector indices (for next-clip bounds)."""
    by: dict[tuple[str, Path], list[int]] = {}
    for r in refs:
        key = (r.disk, r.container)
        if r.source == "inter":
            sec = r.start_offset
        else:
            sec = r.start_offset // STR_SECTOR_USER
        by.setdefault(key, []).append(sec)
    for k in by:
        by[k] = sorted(set(by[k]))
    return by


def _hard_end_for(
    ref: MovieStreamRef, starts: dict[tuple[str, Path], list[int]]
) -> int | None:
    key = (ref.disk, ref.container)
    if ref.source == "inter":
        here = ref.start_offset
    else:
        here = ref.start_offset // STR_SECTOR_USER
    for s in starts.get(key, []):
        if s > here:
            return s
    return None


def _estimate_nsectors(frame_hint: int) -> int:
    # ~10 user-sectors/frame (video+XA) + pad for last frame / align
    return frame_hint * 11 + 32 if frame_hint > 0 else 1


def _resolve_sector_spans(refs: list[MovieStreamRef]) -> None:
    """Set ``nsectors`` from real on-disc span.

    Streaming-list ``unknown5`` is approximately the **frame count**, not the
    CD sector count. Prefer the gap to the next **validated** movie start on
    the same container; if that gap is far shorter than ``unknown5`` implies,
    fall back to a frame-based estimate (false boundaries from wrong-language
    offsets used to truncate stage4/5 clips).
    """
    starts = _container_starts(refs)
    for ref in refs:
        frame_hint = ref.entry.unknown5 if ref.entry.unknown5 else 0
        hard = _hard_end_for(ref, starts)
        if ref.source == "inter":
            here = ref.start_offset
        else:
            here = ref.start_offset // STR_SECTOR_USER
        est = _estimate_nsectors(frame_hint)
        if hard is not None and hard > here:
            gap = hard - here
            # Genuine short clips have gap ≈ 10*unknown5; false next-starts
            # leave a much smaller gap than the frame hint.
            if frame_hint > 0 and gap < frame_hint * 9:
                ref.nsectors = est
            else:
                ref.nsectors = gap
        else:
            ref.nsectors = est if frame_hint > 0 else max(ref.nsectors, 1)


def _load_iso_blob(
    ref: MovieStreamRef,
    *,
    hard_end: int | None = None,
    data: bytes | None = None,
) -> tuple[bytes, int, int]:
    """Load STR user-data blob with frame-aligned start/end.

    Returns ``(iso_bytes, aligned_start_sector, aligned_nsectors)``.
    """
    if data is None:
        data = ref.container.read_bytes()
    if ref.source == "inter":
        start, nsec = align_str_window(
            lambda s: _user_sector_inter(data, s),
            ref.start_offset,
            ref.nsectors,
            hard_end=hard_end,
        )
        return inter_raw_to_iso_user(data, start, nsec), start, nsec

    # CDF: absolute file sector indices
    start_sec = ref.start_offset // STR_SECTOR_USER
    start, nsec = align_str_window(
        lambda s: _user_sector_iso(data, s, base_off=0),
        start_sec,
        ref.nsectors,
        hard_end=hard_end,
    )
    byte0 = start * STR_SECTOR_USER
    return data[byte0 : byte0 + nsec * STR_SECTOR_USER], start, nsec


# Per-process cache so parallel workers reuse INTER/CDF bytes across jobs.
_WORKER_FILE_CACHE: dict[str, bytes] = {}


def _worker_read_container(path: str) -> bytes:
    data = _WORKER_FILE_CACHE.get(path)
    if data is None:
        data = Path(path).read_bytes()
        _WORKER_FILE_CACHE[path] = data
    return data


def _extract_one_job(job: dict[str, Any]) -> dict[str, Any]:
    """Process-pool worker: load one movie, demux video (+ XA audio on INTER)."""
    stem = job["stem"]
    try:
        data = _worker_read_container(job["container"])
        source = job["source"]
        start_offset = job["start_offset"]
        nsectors = job["nsectors"]
        hard_end = job["hard_end"]

        if source == "inter":
            start, nsec = align_str_window(
                lambda s: _user_sector_inter(data, s),
                start_offset,
                nsectors,
                hard_end=hard_end,
            )
            iso = inter_raw_to_iso_user(data, start, nsec)
            align_start = start
            align_nsec = nsec
        else:
            start_sec = start_offset // STR_SECTOR_USER
            start, nsec = align_str_window(
                lambda s: _user_sector_iso(data, s, base_off=0),
                start_sec,
                nsectors,
                hard_end=hard_end,
            )
            byte0 = start * STR_SECTOR_USER
            iso = data[byte0 : byte0 + nsec * STR_SECTOR_USER]
            align_start = start
            align_nsec = nsec

        if len(iso) < STR_SECTOR_USER:
            raise ValueError(f"empty/short payload ({len(iso)} B)")

        movie_dir = Path(job["movie_dir"])
        raw_dir = Path(job["raw_dir"]) if job["raw_dir"] else None

        # XA audio (INTER only) — demux before mux so extract_movie_asset can embed it.
        pcm_chs: list[list[int]] | None = None
        xa_meta: dict[str, Any] | None = None
        audio_rate = 37800
        if job.get("write_audio") and source == "inter":
            try:
                pcm_chs, xa_meta = demux_xa_from_inter(data, align_start, align_nsec)
                if not pcm_chs or not pcm_chs[0]:
                    pcm_chs = None
                else:
                    audio_rate = int(xa_meta.get("sample_rate") or 37800)
            except Exception as ax:
                xa_meta = {"error": str(ax)}
                pcm_chs = None
        elif source != "inter":
            xa_meta = {
                "format": "none",
                "note": "CDF/ISO movies have no XA sectors in 2048-byte dumps",
            }

        meta = extract_movie_asset(
            iso,
            movie_dir,
            stem=stem,
            raw_dir=raw_dir,
            write_raw=job["write_raw"],
            write_mp4=job.get("write_mp4", True),
            pcm_channels=pcm_chs,
            audio_rate=audio_rate,
            max_frames=job["max_frames"],
        )
        meta["descriptor"] = job["descriptor"]
        meta["disk"] = job["disk"]
        meta["stage"] = job["stage"]
        meta["folder_id"] = job["folder_id"]
        meta["slot"] = job["slot"]
        meta["source"] = source
        meta["container"] = job["container_name"]
        meta["descriptor_start"] = (
            f"sector {start_offset}" if source == "inter" else f"0x{start_offset:X}"
        )
        meta["descriptor_frame_hint"] = job["frame_hint"]
        meta["resolved_nsectors"] = nsectors
        meta["aligned_start"] = (
            f"sector {align_start}"
            if source == "inter"
            else f"0x{align_start * STR_SECTOR_USER:X}"
        )
        meta["aligned_nsectors"] = align_nsec
        if xa_meta is not None:
            meta["xa"] = xa_meta

        # Drop legacy loose wav/webp next to mp4
        for legacy in (
            movie_dir / f"{stem}.wav",
            movie_dir / f"{stem}.webp",
        ):
            try:
                if legacy.is_file():
                    legacy.unlink()
            except OSError:
                pass

        (movie_dir / f"{stem}.json").write_text(
            json.dumps(meta, indent=2) + "\n", encoding="utf-8"
        )
        meta["meta"] = f"{MOVIE_DIR}/{stem}.json"
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
        }


def extract_all(
    rom_usa: Path,
    assets_root: Path,
    *,
    write_raw: bool = True,
    write_mp4: bool = True,
    write_audio: bool = True,
    limit: int | None = None,
    max_frames: int | None = None,
    jobs: int | None = None,
) -> list[dict[str, Any]]:
    assets_root = Path(assets_root)
    raw_dir = assets_root / "raw" / "movie"
    movie_dir = assets_root / "movie"
    raw_dir.mkdir(parents=True, exist_ok=True)
    movie_dir.mkdir(parents=True, exist_ok=True)

    refs = collect_movie_streams(rom_usa)
    if limit is not None:
        refs = refs[:limit]
    n_jobs = default_jobs() if jobs is None else max(1, jobs)
    logging.info("Extracting %d movie stream(s) with %d worker(s)", len(refs), n_jobs)
    _resolve_sector_spans(refs)
    container_starts = _container_starts(refs)

    work: list[dict[str, Any]] = []
    for ref in refs:
        hard = _hard_end_for(ref, container_starts)
        work.append(
            {
                "stem": _stem_for(ref),
                "container": str(ref.container.resolve()),
                "container_name": ref.container.name,
                "source": ref.source,
                "start_offset": ref.start_offset,
                "nsectors": ref.nsectors,
                "hard_end": hard,
                "movie_dir": str(movie_dir),
                "raw_dir": str(raw_dir) if write_raw else None,
                "write_raw": write_raw,
                "write_mp4": write_mp4,
                "write_audio": write_audio,
                "max_frames": max_frames,
                "descriptor": streaming_entry_to_json(ref.entry),
                "disk": ref.disk,
                "stage": ref.stage,
                "folder_id": ref.folder_id,
                "slot": ref.slot,
                "frame_hint": ref.entry.unknown5,
                "stream_id": ref.entry.stream_id,
            }
        )

    catalog: list[dict[str, Any]] = []
    results = run_jobs(_extract_one_job, work, jobs=n_jobs, label_key="stem")

    # Stable catalog order: original work order
    by_stem = {r.get("stem"): r for r in results}
    for job in work:
        r = by_stem.get(job["stem"])
        if r is None:
            catalog.append({"stem": job["stem"], "error": "missing result"})
            continue
        if r.get("ok"):
            meta = r["meta"]
            catalog.append(meta)
            xa = meta.get("xa") or {}
            extra = ""
            if meta.get("mp4"):
                extra = "  mp4"
                if meta.get("audio_duration_sec") is not None:
                    extra += f"+alac {meta['audio_duration_sec']}s"
                elif job["source"] == "inter" and xa.get("audio_sectors") == 0:
                    extra += " (silent)"
            if meta.get("mp4_error"):
                extra = f"  mp4_error={meta['mp4_error']}"
            logging.info(
                "OK %s  %s frames  %dx%d  (sectors %s→%s)%s  → %s",
                job["stem"],
                meta.get("frame_count", 0),
                meta.get("width", 0),
                meta.get("height", 0),
                job["nsectors"],
                meta.get("aligned_nsectors"),
                extra,
                meta.get("mp4") or f"{MOVIE_DIR}/{job['stem']}.json",
            )
        else:
            if r.get("traceback"):
                logging.error("FAIL %s:\n%s", job["stem"], r["traceback"])
            else:
                logging.error("FAIL %s: %s", job["stem"], r.get("error"))
            catalog.append(
                {
                    "stem": job["stem"],
                    "error": r.get("error", "unknown"),
                    "disk": r.get("disk", job["disk"]),
                    "stage": r.get("stage", job["stage"]),
                    "folder_id": r.get("folder_id", job["folder_id"]),
                    "stream_id": r.get("stream_id", job["stream_id"]),
                }
            )

    (movie_dir / "movies.json").write_text(
        json.dumps(catalog, indent=2) + "\n", encoding="utf-8"
    )
    ok = sum(1 for c in catalog if "error" not in c)
    logging.info(
        "Done: %d ok / %d total → %s/ and %s/",
        ok,
        len(catalog),
        RAW_MOVIE_DIR,
        MOVIE_DIR,
    )
    return catalog


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--rom", type=Path, default=Path("rom/USA"))
    ap.add_argument("--out", type=Path, default=Path("assets/USA"))
    ap.add_argument("--no-raw", action="store_true")
    ap.add_argument(
        "--no-mp4",
        action="store_true",
        help="Skip lossless MP4 encode (meta/json only)",
    )
    ap.add_argument(
        "--no-audio",
        action="store_true",
        help="Skip XA audio demux (video-only MP4 for INTER movies)",
    )
    ap.add_argument("--limit", type=int, default=None)
    ap.add_argument(
        "--max-frames",
        type=int,
        default=None,
        help="Cap frames per movie (debug)",
    )
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
    if not args.rom.is_dir():
        logging.error("rom path not found: %s", args.rom)
        return 1

    extract_all(
        args.rom,
        args.out,
        write_raw=not args.no_raw,
        write_mp4=not args.no_mp4,
        write_audio=not args.no_audio,
        limit=args.limit,
        max_frames=args.max_frames,
        jobs=args.jobs,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
