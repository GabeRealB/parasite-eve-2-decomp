"""PlayStation STR (MDEC video) demux + frame decode for PE2 movies.

Retail streaming movies are classic **STR v2** MDEC (same demuxed frame
format as room ``.bs`` backgrounds). On disc they appear as:

* **STAGE folder / CDF** — 2048-byte ISO sectors, each starting with
  ``0x80010160`` (video) or non-magic (audio/pad; skipped).
* **INTER0.STR / INTER1.STR** — raw Mode 2 Form 1 sectors of **2336** bytes
  (8-byte XA subheader + 2048 user + 280 ECC). Video user data starts at
  ``+8`` with the same classic header.

:: STR sector user data (2048 B) ::

    0x00  u32  magic ``0x80010160``
    0x04  u16  chunk index (0 .. total-1)
    0x06  u16  chunks in this frame
    0x08  u32  frame number
    0x0C  u32  demuxed frame size (bytes, multiple of 4)
    0x10  u16  width
    0x12  u16  height
    0x14  u16  MDEC code count (div2, rounded)
    0x16  u16  ``0x3800``
    0x18  u16  quant scale
    0x1A  u16  version (2)
    0x1C  u32  0
    0x20  …    2016 bytes chunk payload

Demux: concatenate chunk payloads 0..total-1, truncate to frame size, then
decode with :func:`bs_codec.decode_bs_v2`.
"""

from __future__ import annotations

import json
import logging
import shutil
import struct
import subprocess
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterator, Sequence

from bs_codec import decode_bs_v2

STR_MAGIC = 0x80010160
STR_SECTOR_USER = 0x800
STR_HEADER_SIZE = 0x20
STR_CHUNK_PAYLOAD = 0x7E0  # 2016
INTER_RAW_SECTOR = 2336
INTER_USER_OFF = 8  # after XA subheader


@dataclass
class StrSectorHeader:
    chunk: int
    total: int
    frame: int
    frame_size: int
    width: int
    height: int
    mdec_codes: int
    quant: int
    version: int


@dataclass
class StrFrame:
    frame: int
    width: int
    height: int
    frame_size: int
    quant: int
    version: int
    demux: bytes


def parse_str_sector_header(sec: bytes) -> StrSectorHeader | None:
    if len(sec) < STR_HEADER_SIZE:
        return None
    magic = struct.unpack_from("<I", sec, 0)[0]
    if magic != STR_MAGIC:
        return None
    chunk, total = struct.unpack_from("<HH", sec, 4)
    frame, fsize = struct.unpack_from("<II", sec, 8)
    w, h = struct.unpack_from("<HH", sec, 16)
    codes, magic3800 = struct.unpack_from("<HH", sec, 20)
    q, ver = struct.unpack_from("<HH", sec, 24)
    if magic3800 != 0x3800:
        # still accept; some variants differ
        pass
    return StrSectorHeader(
        chunk=chunk,
        total=total,
        frame=frame,
        frame_size=fsize,
        width=w,
        height=h,
        mdec_codes=codes,
        quant=q,
        version=ver,
    )


def iter_str_user_sectors_iso(data: bytes) -> Iterator[bytes]:
    """Yield 2048-byte user sectors from an ISO-style blob."""
    n = len(data) // STR_SECTOR_USER
    for i in range(n):
        yield data[i * STR_SECTOR_USER : (i + 1) * STR_SECTOR_USER]


def inter_raw_to_iso_user(raw: bytes, start_sector: int, nsectors: int) -> bytes:
    """Extract *nsectors* of 2048-byte user data from INTER raw (2336 B/sec)."""
    out = bytearray()
    for s in range(nsectors):
        base = (start_sector + s) * INTER_RAW_SECTOR
        if base + INTER_RAW_SECTOR > len(raw):
            break
        out += raw[base + INTER_USER_OFF : base + INTER_USER_OFF + STR_SECTOR_USER]
    return bytes(out)


def _user_sector_inter(raw: bytes, sector: int) -> bytes | None:
    base = sector * INTER_RAW_SECTOR
    if base + INTER_RAW_SECTOR > len(raw):
        return None
    return raw[base + INTER_USER_OFF : base + INTER_USER_OFF + STR_SECTOR_USER]


def _user_sector_iso(data: bytes, sector: int, base_off: int = 0) -> bytes | None:
    off = base_off + sector * STR_SECTOR_USER
    if off + STR_SECTOR_USER > len(data):
        return None
    return data[off : off + STR_SECTOR_USER]


def align_str_window(
    get_user: Any,
    start: int,
    nsectors: int,
    *,
    hard_end: int | None = None,
    max_back: int = 48,
    max_fwd: int = 128,
) -> tuple[int, int]:
    """Expand ``[start, start+nsectors)`` so first/last frames are complete.

    Retail descriptors often begin mid-frame (chunk≠0) and/or end before the
    last frame's final chunks — we drop those incomplete frames and the clip
    looks short. This walks a little backward/forward (bounded) to recover them.

    *get_user(sector_index) → 2048-byte user data or None*
    *hard_end*: exclusive sector limit (e.g. next movie start).
    Returns ``(new_start, new_nsectors)``.
    """
    # Keep absolute end fixed when backing up the start so we don't shrink.
    abs_end = start + max(nsectors, 1)
    if hard_end is not None:
        abs_end = min(abs_end, hard_end)
    end = abs_end

    def hdr_at(s: int) -> StrSectorHeader | None:
        u = get_user(s)
        if not u:
            return None
        return parse_str_sector_header(u)

    # --- start: if first video isn't chunk 0, back up to that frame's chunk 0
    first_s = None
    first_h = None
    for s in range(start, min(end, start + 32)):
        h = hdr_at(s)
        if h is not None:
            first_s, first_h = s, h
            break
    if first_h is not None and first_h.chunk != 0:
        target = first_h.frame
        for s in range(first_s - 1, max(0, start - max_back) - 1, -1):
            h = hdr_at(s)
            if h is None:
                continue
            if h.frame == target and h.chunk == 0:
                start = s
                break
            if h.frame != target and h.chunk == 0:
                break

    # --- end: complete the last frame that intersects the window
    from collections import defaultdict

    acc: dict[int, dict[str, Any]] = {}
    for s in range(start, end):
        h = hdr_at(s)
        if h is None or h.total <= 0:
            continue
        fr = acc.setdefault(h.frame, {"total": h.total, "chunks": set()})
        fr["total"] = h.total
        fr["chunks"].add(h.chunk)

    if acc:
        last_f = max(acc)
        need = acc[last_f]["total"]
        have = acc[last_f]["chunks"]
        if len(have) < need:
            limit = end + max_fwd
            if hard_end is not None:
                limit = min(limit, hard_end)
            s = end
            while s < limit and len(have) < need:
                h = hdr_at(s)
                s += 1
                end = s
                if h is None:
                    continue
                if h.frame == last_f:
                    have.add(h.chunk)
                    need = h.total
                elif h.chunk == 0 and h.frame != last_f:
                    if len(have) < need:
                        end = s - 1
                    break

    if end < start:
        end = start
    return start, end - start


def demux_str_frames(iso_data: bytes) -> list[StrFrame]:
    """Demux complete frames from a 2048-byte-sector STR blob."""
    acc: dict[int, dict[str, Any]] = {}
    for sec in iter_str_user_sectors_iso(iso_data):
        hdr = parse_str_sector_header(sec)
        if hdr is None:
            continue
        if hdr.total <= 0 or hdr.chunk >= hdr.total:
            continue
        fr = acc.setdefault(
            hdr.frame,
            {
                "total": hdr.total,
                "fsize": hdr.frame_size,
                "w": hdr.width,
                "h": hdr.height,
                "q": hdr.quant,
                "ver": hdr.version,
                "chunks": {},
            },
        )
        # Prefer first-seen geometry
        fr["chunks"][hdr.chunk] = sec[STR_HEADER_SIZE : STR_HEADER_SIZE + STR_CHUNK_PAYLOAD]

    frames: list[StrFrame] = []
    for frame_num in sorted(acc):
        fr = acc[frame_num]
        total = fr["total"]
        chunks = fr["chunks"]
        if any(i not in chunks for i in range(total)):
            continue  # incomplete
        demux = b"".join(chunks[i] for i in range(total))
        fsize = fr["fsize"]
        if fsize > 0:
            demux = demux[:fsize]
        if len(demux) < 8:
            continue
        if struct.unpack_from("<H", demux, 2)[0] != 0x3800:
            continue
        frames.append(
            StrFrame(
                frame=frame_num,
                width=fr["w"],
                height=fr["h"],
                frame_size=fsize,
                quant=fr["q"],
                version=fr["ver"],
                demux=demux,
            )
        )
    return frames


def decode_str_frame(frame: StrFrame):
    """Return ``(PIL.Image, BsInfo)`` for one demuxed frame."""
    return decode_bs_v2(frame.demux, width=frame.width, height=frame.height)


def _ffmpeg_bin() -> str:
    exe = shutil.which("ffmpeg")
    if not exe:
        raise RuntimeError("ffmpeg not found on PATH (required for movie MP4)")
    return exe


def encode_lossless_mp4(
    *,
    width: int,
    height: int,
    fps: float,
    rgb_frames: Iterator[bytes],
    n_frames: int,
    out_mp4: Path,
    pcm_s16le: bytes | None = None,
    audio_rate: int = 37800,
    audio_channels: int = 2,
) -> dict[str, Any]:
    """Mux RGB24 frames (+ optional s16le PCM) into a **true lossless** MP4.

    * Video: ``libx264 -crf 0 -pix_fmt yuv444p`` (High 4:4:4 Predictive —
      bit-exact vs the RGB we feed after MDEC; no chroma subsampling).
    * Audio: **ALAC** at the native XA rate (typically 37800 Hz stereo).

    **Windows Media Player cannot play this** (no H.264 4:4:4, weak ALAC).
    Use **VLC**, **mpv**, **MPC-HC**, or **ffplay**.

    Container is still ``.mp4`` (isom) for simplicity; for maximum archival
    interop prefer remuxing to MKV with FFV1+FLAC outside this tool.
    """
    out_mp4 = Path(out_mp4)
    out_mp4.parent.mkdir(parents=True, exist_ok=True)
    ff = _ffmpeg_bin()

    tmp_wav: Path | None = None
    cmd: list[str] = [
        ff,
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-f",
        "rawvideo",
        "-pix_fmt",
        "rgb24",
        "-s",
        f"{width}x{height}",
        "-r",
        str(fps),
        "-i",
        "pipe:0",
    ]
    if pcm_s16le:
        import wave

        # Pad/trim PCM to video duration so we never drop frames.
        nch = max(1, audio_channels)
        bytes_per_frame = 2 * nch
        target_samples = (
            int(round(n_frames * float(audio_rate) / float(fps))) if fps > 0 else 0
        )
        target_bytes = target_samples * bytes_per_frame
        if target_bytes > 0:
            if len(pcm_s16le) < target_bytes:
                pcm_s16le = pcm_s16le + b"\x00" * (target_bytes - len(pcm_s16le))
            elif len(pcm_s16le) > target_bytes + bytes_per_frame * audio_rate:
                pcm_s16le = pcm_s16le[:target_bytes]

        tmp = tempfile.NamedTemporaryFile(suffix=".wav", delete=False)
        tmp_wav = Path(tmp.name)
        tmp.close()
        with wave.open(str(tmp_wav), "wb") as w:
            w.setnchannels(nch)
            w.setsampwidth(2)
            w.setframerate(audio_rate)
            w.writeframes(pcm_s16le)
        cmd += ["-i", str(tmp_wav)]

    # True lossless: 4:4:4 H.264 + ALAC (native sample rate, no resampler).
    cmd += [
        "-c:v",
        "libx264",
        "-crf",
        "0",
        "-preset",
        "ultrafast",
        "-pix_fmt",
        "yuv444p",
    ]
    if pcm_s16le:
        cmd += ["-c:a", "alac"]
    else:
        cmd += ["-an"]
    cmd += ["-movflags", "+faststart", str(out_mp4)]

    try:
        proc = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        assert proc.stdin is not None
        written = 0
        try:
            for raw in rgb_frames:
                proc.stdin.write(raw)
                written += 1
            proc.stdin.close()
        except BrokenPipeError:
            pass
        _stdout, stderr = proc.communicate()
        if proc.returncode != 0:
            err = (stderr or b"").decode("utf-8", errors="replace")
            raise RuntimeError(
                f"ffmpeg failed ({proc.returncode}) after {written}/{n_frames} frames: {err}"
            )
        if written != n_frames:
            logging.warning(
                "mp4 encode wrote %s frames, expected %s", written, n_frames
            )
    finally:
        if tmp_wav is not None:
            try:
                tmp_wav.unlink(missing_ok=True)
            except OSError:
                pass

    return {
        "video_codec": "libx264",
        "video_lossless": True,
        "crf": 0,
        "pix_fmt": "yuv444p",
        "audio_codec": "alac" if pcm_s16le else None,
        "audio_rate": audio_rate if pcm_s16le else None,
        "audio_lossless": bool(pcm_s16le),
        "wmp_compatible": False,
        "player_hint": "VLC / mpv / MPC-HC / ffplay (not Windows Media Player)",
        "frames_muxed": n_frames,
    }


def _pcm_channels_to_s16le(channels: Sequence[Sequence[int]]) -> tuple[bytes, int]:
    """Interleave channel PCM lists to little-endian s16 bytes. Returns (data, nch)."""
    from array import array

    if not channels or not channels[0]:
        return b"", 0
    if len(channels) == 1:
        return array("h", channels[0]).tobytes(), 1
    left, right = channels[0], channels[1]
    n = max(len(left), len(right))
    interleaved: list[int] = []
    for i in range(n):
        interleaved.append(left[i] if i < len(left) else 0)
        interleaved.append(right[i] if i < len(right) else 0)
    return array("h", interleaved).tobytes(), 2


def extract_movie_asset(
    iso_str: bytes,
    out_dir: Path | str,
    *,
    stem: str,
    raw_dir: Path | str | None = None,
    write_raw: bool = True,
    write_mp4: bool = True,
    pcm_channels: Sequence[Sequence[int]] | None = None,
    audio_rate: int = 37800,
    max_frames: int | None = None,
    fps: float = 15.0,
    # legacy no-ops kept for call-site compatibility
    write_frames: bool = False,
    write_webp: bool = False,
) -> dict[str, Any]:
    """Demux STR video, optionally mux with PCM, write a lossless MP4.

    Outputs under *out_dir* (the ``movie/`` type dir)::

        {stem}.mp4     lossless H.264 (yuv444p crf0) + ALAC when audio present
        {stem}.json    meta / descriptor fields (caller may rewrite)

    Optional ``raw_dir/{stem}.str`` — 2048-byte sector video blob.
    """
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if write_raw and raw_dir is not None:
        raw_dir = Path(raw_dir)
        raw_dir.mkdir(parents=True, exist_ok=True)
        (raw_dir / f"{stem}.str").write_bytes(iso_str)

    frames = demux_str_frames(iso_str)
    if max_frames is not None:
        frames = frames[:max_frames]

    meta: dict[str, Any] = {
        "format": "str",
        "stem": stem,
        "frame_count": len(frames),
        "fps": fps,
        "raw_bytes": f"0x{len(iso_str):X}",
        "raw_sectors": len(iso_str) // STR_SECTOR_USER,
    }
    if frames:
        meta["width"] = frames[0].width
        meta["height"] = frames[0].height
        meta["version"] = frames[0].version
        meta["frame_range"] = [frames[0].frame, frames[-1].frame]

    if write_mp4 and frames:
        width = frames[0].width
        height = frames[0].height

        def _rgb_iter() -> Iterator[bytes]:
            for fr in frames:
                img, _info = decode_str_frame(fr)
                rgb = img.convert("RGB")
                if rgb.size != (width, height):
                    rgb = rgb.resize((width, height))
                yield rgb.tobytes()

        pcm_bytes: bytes | None = None
        nch = 0
        if pcm_channels and pcm_channels[0]:
            pcm_bytes, nch = _pcm_channels_to_s16le(pcm_channels)
            meta["audio_sample_rate"] = audio_rate
            meta["audio_channels"] = nch
            meta["audio_samples"] = len(pcm_channels[0])
            meta["audio_duration_sec"] = round(
                len(pcm_channels[0]) / float(audio_rate), 3
            )

        mp4_path = out_dir / f"{stem}.mp4"
        try:
            enc = encode_lossless_mp4(
                width=width,
                height=height,
                fps=fps,
                rgb_frames=_rgb_iter(),
                n_frames=len(frames),
                out_mp4=mp4_path,
                pcm_s16le=pcm_bytes,
                audio_rate=audio_rate,
                audio_channels=nch or 2,
            )
            meta["mp4"] = f"movie/{stem}.mp4"
            meta["mp4_encode"] = enc
            meta["mp4_bytes"] = mp4_path.stat().st_size
        except Exception as e:
            meta["mp4_error"] = str(e)
    elif write_mp4 and not frames:
        meta["mp4_error"] = "no frames to encode"

    # Remove legacy per-frame directory if empty leftovers exist
    legacy_dir = out_dir / stem
    if legacy_dir.is_dir() and not write_frames:
        # only remove if it looks like old frame dump (has frame_*.png or meta only)
        try:
            for p in legacy_dir.glob("frame_*.png"):
                p.unlink()
            meta_legacy = legacy_dir / "meta.json"
            if meta_legacy.is_file():
                meta_legacy.unlink()
            if not any(legacy_dir.iterdir()):
                legacy_dir.rmdir()
        except OSError:
            pass

    meta_path = out_dir / f"{stem}.json"
    meta_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    meta["meta"] = f"movie/{stem}.json"
    return meta
