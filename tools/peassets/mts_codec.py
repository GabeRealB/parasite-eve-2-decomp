"""Parasite Eve 2 MTS CD→SPU audio stream codec.

Retail audio streams (streaming-list type ``2``) are sector streams of
**MTS** ("MTS" + channel count) packets fed to ``CdStream_*`` /
``SpuWrite``.

## On-disc layout

Descriptor (``FsCdfStream`` audio arm, ``0x28`` bytes) points at a start
sector. Payload is a run of CD sectors (``0x800`` bytes each):

Every *period* sectors an **MTS header sector** appears::

    0x00  s32   field_0      chunk index (increments every stereo pair)
    0x04  s32   field_4      total chunk count
    0x08  u32   magic        LE ``0x4D5453cc`` → bytes ``cc 'S' 'T' 'M'``
                             (``cc`` = channel count, almost always 2)
    0x0C  s8    field_C      channel index within the period group (0/1)
    0x0D  u8    period       sectors between successive MTS headers
    0x0E  u8    field_E
    0x0F  s8    field_F
    0x10  …     SPU-ADPCM (game ``SpuWrite(sec+0x10, …)``; see sizes below)

Intervening sectors are raw SPU-ADPCM continuation.

``CdStream_ReadyMts`` write sizes per period window (``remaining % period``)::

    header (rem % P == 0):  ``SpuWrite(sec+0x10, 0x800)`` but SPU ring
                            advances **0x7F0** → take ``sec[0x10:0x800]``
    middle:                 full sector ``0x800``
    last   (rem % P == 1):  **0x780** only (``ringHalf`` math; trailing
                            ``0x80`` is pad/zeros — including it causes a
                            periodic click)

Stereo pattern (2ch, period *P*)::

    sec 0:     MTS ch0 chunk0  + ADPCM
    sec 1..P-1: ADPCM ch0
    sec P:     MTS ch1 chunk0  + ADPCM
    sec P+1..: ADPCM ch1
    sec 2P:    MTS ch0 chunk1
    …

Total MTS body length in sectors ≈ ``chunks × channels × period``.
Some streams have a short preamble (TOC) before the first MTS header;
descriptor ``field_14`` (peassets ``unknown3``) is often the total span
from the listed offset when non-zero.

Sample rate: SPU native ≈ **22050 Hz** (same as SPK).
"""

from __future__ import annotations

import json
import struct
import wave
from array import array
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from spk_codec import _ADPCM_COEFFS

SECTOR_SIZE = 0x800
MTS_HEADER_SIZE = 0x10
# Usable ADPCM after MtsSector on a header sector (SPU ring advance).
MTS_HEADER_ADPCM = SECTOR_SIZE - MTS_HEADER_SIZE  # 0x7F0
# Last sector of each period window (ReadyMts rem%period==1 path).
MTS_LAST_SECTOR_ADPCM = 0x780
# Bytes ``\\x02STM`` as stored (LE magic 0x4D545302)
MTS_MAGIC_BYTES = b"\x02\x53\x54\x4d"
SPU_RATE = 22050


def decode_spu_adpcm_stream(data: bytes) -> list[int]:
    """Decode SPU-ADPCM for a continuous MTS stream.

    Unlike one-shot SPK samples, stream frames often set the ADPCM "end"
    flag at chunk boundaries; ignore it and decode the full buffer.
    """
    s_1 = 0
    s_2 = 0
    out: list[int] = []
    for i in range(0, len(data) - 15, 16):
        block = data[i : i + 16]
        shift = block[0] & 0x0F
        filt = block[0] >> 4
        if filt > 4:
            filt = 0
        if shift > 12:
            shift = 9
        f0, f1 = _ADPCM_COEFFS[filt]
        for b in block[2:]:
            for nibble in (b & 0x0F, b >> 4):
                if nibble >= 8:
                    nibble -= 16
                sample = nibble << (12 - shift)
                sample += (s_1 * f0 + s_2 * f1 + 32) >> 6
                if sample > 0x7FFF:
                    sample = 0x7FFF
                elif sample < -0x8000:
                    sample = -0x8000
                out.append(sample)
                s_2 = s_1
                s_1 = sample
    return out


@dataclass
class MtsHeader:
    sector_index: int
    field_0: int
    chunk_count: int
    channels: int
    channel_index: int
    period: int
    field_e: int
    field_f: int


@dataclass
class MtsStreamInfo:
    """Parsed geometry of one MTS stream body (from first header)."""

    preamble_sectors: int
    chunk_count: int
    channels: int
    period: int
    body_sectors: int  # chunks * channels * period
    total_sectors: int  # preamble + body (or descriptor override)
    headers: list[MtsHeader] = field(default_factory=list)

    @property
    def body_bytes(self) -> int:
        return self.body_sectors * SECTOR_SIZE


def is_mts_header_sector(sec: bytes) -> bool:
    return len(sec) >= 0x0C and sec[0x08:0x0C] == MTS_MAGIC_BYTES


def parse_mts_header(sec: bytes, sector_index: int = 0) -> MtsHeader:
    if not is_mts_header_sector(sec):
        raise ValueError("not an MTS header sector")
    field_0, chunk_count, magic = struct.unpack_from("<3I", sec, 0)
    channels = sec[0x08]
    return MtsHeader(
        sector_index=sector_index,
        field_0=field_0,
        chunk_count=chunk_count,
        channels=channels,
        channel_index=struct.unpack_from("<b", sec, 0x0C)[0],
        period=sec[0x0D],
        field_e=sec[0x0E],
        field_f=struct.unpack_from("<b", sec, 0x0F)[0],
    )


def find_first_mts_sector(data: bytes, max_sectors: int = 32) -> int | None:
    """Return sector index of first MTS header, or None."""
    n = min(max_sectors, len(data) // SECTOR_SIZE)
    for i in range(n):
        off = i * SECTOR_SIZE
        if is_mts_header_sector(data[off : off + SECTOR_SIZE]):
            return i
    return None


def probe_mts_stream(
    data: bytes,
    *,
    descriptor_sectors: int | None = None,
) -> MtsStreamInfo:
    """Probe stream geometry from raw sector bytes starting at the descriptor offset.

    ``descriptor_sectors`` is list-entry ``field_14`` when non-zero; used as
    total span when provided. Body length always comes from the first MTS header.
    """
    if len(data) < SECTOR_SIZE:
        raise ValueError("stream shorter than one sector")

    pre = find_first_mts_sector(data)
    if pre is None:
        raise ValueError("no MTS header found in first sectors")

    hdr = parse_mts_header(data[pre * SECTOR_SIZE : (pre + 1) * SECTOR_SIZE], pre)
    if hdr.period <= 0 or hdr.channels <= 0 or hdr.chunk_count <= 0:
        raise ValueError(
            f"invalid MTS geometry: chunks={hdr.chunk_count} "
            f"ch={hdr.channels} period={hdr.period}"
        )

    # Nominal body assumes tight packing (no gap sectors). Real streams often
    # insert padding between chunk pairs — scan for headers to measure.
    nominal_body = hdr.chunk_count * hdr.channels * hdr.period
    max_scan = len(data) // SECTOR_SIZE
    expected = hdr.chunk_count * hdr.channels

    if descriptor_sectors and descriptor_sectors > 0:
        scan_end = min(pre + descriptor_sectors, max_scan)
    else:
        # Allow large gaps: up to ~2× nominal or rest of file.
        scan_end = min(max_scan, pre + max(nominal_body * 2, nominal_body + 0x400))

    headers: list[MtsHeader] = []
    for i in range(pre, scan_end):
        sec = data[i * SECTOR_SIZE : (i + 1) * SECTOR_SIZE]
        if not is_mts_header_sector(sec):
            continue
        h = parse_mts_header(sec, i - pre)
        headers.append(h)
        if len(headers) >= expected and h.field_0 >= hdr.chunk_count - 1:
            # Include this header's period window, then stop.
            # (Last expected header may be ch1 of final chunk.)
            if len(headers) >= expected:
                break

    if len(headers) >= expected:
        headers = headers[:expected]
        last = headers[-1]
        body = last.sector_index + (last.period if last.period > 0 else hdr.period)
    elif headers:
        last = headers[-1]
        body = max(
            nominal_body,
            last.sector_index + (last.period if last.period > 0 else hdr.period),
        )
    else:
        body = nominal_body

    if descriptor_sectors and descriptor_sectors > 0:
        total = descriptor_sectors
        # Ensure body covers at least the descriptor span after preamble
        body = max(body, descriptor_sectors - pre)
    else:
        total = pre + body

    return MtsStreamInfo(
        preamble_sectors=pre,
        chunk_count=hdr.chunk_count,
        channels=hdr.channels,
        period=hdr.period,
        body_sectors=body,
        total_sectors=total,
        headers=headers,
    )


def _trim_zero_adpcm_frames(data: bytes, *, leading: bool, trailing: bool) -> bytes:
    """Drop whole 16-byte all-zero ADPCM frames at the ends of a window.

    Retail streams pad each period window with silence frames after the last
    real block (often past an end-flag frame ``flags & 1``). The SPU never
    plays those: end/loop flags stop the voice before the pad. Including the
    pad in offline decode puts ~10–14 ms of digital zero every period → click.
    """
    start = 0
    end = len(data) - (len(data) % 16)
    if leading:
        while start + 16 <= end and data[start : start + 16] == bytes(16):
            start += 16
    if trailing:
        while end - 16 >= start and data[end - 16 : end] == bytes(16):
            end -= 16
    return data[start:end]


def _trim_after_end_flag(data: bytes) -> bytes:
    """Keep samples through the first frame with end flag (bit0); drop the rest.

    Odd windows often end ``… [flags=0x03] [zero] [zero] …``. Hardware hits
    the end flag and loops/stops — zeros are never heard.
    """
    end = len(data) - (len(data) % 16)
    for i in range(0, end, 16):
        flags = data[i + 1]
        if flags & 1:
            return data[: i + 16]
    return data[:end]


def demux_adpcm_channels(body: bytes, info: MtsStreamInfo) -> list[bytes]:
    """Demux MTS *body* (no preamble) into per-channel SPU-ADPCM byte streams.

    Scan for every MTS header. Each header owns the following ``period``
    sectors. Write sizes match ``CdStream_ReadyMts``:

    * header: ``sec[0x10:0x800]`` (0x7F0)
    * middle: full 0x800
    * last of window: **0x780** only (sector pad ``0x80``)

    Each window is then trimmed: stop after ADPCM end-flag, drop leading
    zero frames (except the very first window of a channel) and trailing
    zero frames. Gaps between header windows (TOC) are skipped.
    """
    channels = max(info.channels, 1)
    nsec = len(body) // SECTOR_SIZE
    out = [bytearray() for _ in range(channels)]
    windows_seen = [0] * channels

    sec_i = 0
    while sec_i < nsec:
        hdr_off = sec_i * SECTOR_SIZE
        sec = body[hdr_off : hdr_off + SECTOR_SIZE]
        if not is_mts_header_sector(sec):
            sec_i += 1
            continue

        hdr = parse_mts_header(sec, sec_i)
        ch = hdr.channel_index
        if ch < 0 or ch >= channels:
            ch = 0
        period = hdr.period if hdr.period > 0 else info.period
        if period <= 0:
            period = 1

        window = bytearray()
        # Header sector: skip MtsSector, take 0x7F0 ADPCM bytes.
        window += sec[MTS_HEADER_SIZE : MTS_HEADER_SIZE + MTS_HEADER_ADPCM]

        if period > 1:
            for s in range(1, period):
                si = sec_i + s
                if si >= nsec:
                    break
                nxt = body[si * SECTOR_SIZE : (si + 1) * SECTOR_SIZE]
                if is_mts_header_sector(nxt):
                    break
                if s == period - 1:
                    window += nxt[:MTS_LAST_SECTOR_ADPCM]
                else:
                    window += nxt

        # Playable audio only: honor end-flag, drop sector silence padding.
        first = windows_seen[ch] == 0
        trimmed = _trim_after_end_flag(bytes(window))
        trimmed = _trim_zero_adpcm_frames(
            trimmed,
            leading=not first,  # keep song-start lead-in on first window
            trailing=True,
        )

        out[ch] += trimmed
        windows_seen[ch] += 1
        sec_i += 1

    return [bytes(b) for b in out]


def decode_mts_to_pcm(
    data: bytes,
    *,
    descriptor_sectors: int | None = None,
) -> tuple[MtsStreamInfo, list[list[int]]]:
    """Return ``(info, [pcm_ch0, pcm_ch1, …])`` from raw stream bytes."""
    info = probe_mts_stream(data, descriptor_sectors=descriptor_sectors)
    body_off = info.preamble_sectors * SECTOR_SIZE
    body_len = info.body_sectors * SECTOR_SIZE
    body = data[body_off : body_off + body_len]
    if len(body) < body_len:
        # Truncated — use what we have; recompute usable chunks.
        usable = len(body) // SECTOR_SIZE
        info = MtsStreamInfo(
            preamble_sectors=info.preamble_sectors,
            chunk_count=info.chunk_count,
            channels=info.channels,
            period=info.period,
            body_sectors=usable,
            total_sectors=info.preamble_sectors + usable,
            headers=info.headers,
        )
        body = data[body_off : body_off + usable * SECTOR_SIZE]

    adpcm_chs = demux_adpcm_channels(body, info)
    pcm_chs = [decode_spu_adpcm_stream(a) for a in adpcm_chs]
    return info, pcm_chs


def interleave_stereo(left: list[int], right: list[int]) -> list[int]:
    n = max(len(left), len(right))
    out: list[int] = []
    for i in range(n):
        out.append(left[i] if i < len(left) else 0)
        out.append(right[i] if i < len(right) else 0)
    return out


def write_wav_pcm(
    path: Path | str,
    channels_pcm: list[list[int]],
    *,
    rate: int = SPU_RATE,
) -> None:
    """Write mono or stereo s16le WAV."""
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if not channels_pcm or not any(channels_pcm):
        raise ValueError("no PCM data")

    nch = len(channels_pcm)
    if nch == 1:
        frames = array("h", channels_pcm[0])
    elif nch >= 2:
        frames = array("h", interleave_stereo(channels_pcm[0], channels_pcm[1]))
        nch = 2
    else:
        raise ValueError(nch)

    with wave.open(str(path), "wb") as w:
        w.setnchannels(nch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(frames.tobytes())


def info_to_json(info: MtsStreamInfo, **extra: Any) -> dict[str, Any]:
    d: dict[str, Any] = {
        "format": "mts",
        "preamble_sectors": info.preamble_sectors,
        "chunk_count": info.chunk_count,
        "channels": info.channels,
        "period": info.period,
        "body_sectors": info.body_sectors,
        "total_sectors": info.total_sectors,
        "body_bytes": f"0x{info.body_bytes:X}",
        "sample_rate": SPU_RATE,
        "header_count": len(info.headers),
    }
    d.update(extra)
    return d


def extract_mts_asset(
    data: bytes,
    *,
    stem: str = "stream",
    raw_dir: Path | str | None = None,
    audio_dir: Path | str | None = None,
    descriptor_sectors: int | None = None,
    write_raw: bool = True,
    write_wav: bool = True,
) -> dict[str, Any]:
    """Write type-store style outputs:

    * ``raw_dir/{stem}.mts`` — on-disc sector payload
    * ``audio_dir/{stem}.wav`` + ``audio_dir/{stem}.json`` — decoded edit form

    Either directory may be omitted to skip that half.
    """
    info, pcm_chs = decode_mts_to_pcm(data, descriptor_sectors=descriptor_sectors)
    # Prefer longest channel for duration (stereo may pad the shorter).
    pcm_frames = max((len(c) for c in pcm_chs), default=0)
    meta = info_to_json(
        info,
        stem=stem,
        pcm_frames=pcm_frames,
        duration_sec=round(pcm_frames / SPU_RATE, 3),
    )

    span = info.total_sectors * SECTOR_SIZE
    raw = data[: min(span, len(data))]
    meta["raw_bytes"] = f"0x{len(raw):X}"

    if write_raw and raw_dir is not None:
        raw_dir = Path(raw_dir)
        raw_dir.mkdir(parents=True, exist_ok=True)
        raw_path = raw_dir / f"{stem}.mts"
        raw_path.write_bytes(raw)
        meta["raw"] = f"raw/audio/{stem}.mts"

    if write_wav and audio_dir is not None and pcm_chs:
        audio_dir = Path(audio_dir)
        audio_dir.mkdir(parents=True, exist_ok=True)
        write_wav_pcm(audio_dir / f"{stem}.wav", pcm_chs)
        meta["wav"] = f"audio/{stem}.wav"
        (audio_dir / f"{stem}.json").write_text(
            json.dumps(meta, indent=2) + "\n", encoding="utf-8"
        )
        meta["meta"] = f"audio/{stem}.json"
    elif audio_dir is not None:
        audio_dir = Path(audio_dir)
        audio_dir.mkdir(parents=True, exist_ok=True)
        (audio_dir / f"{stem}.json").write_text(
            json.dumps(meta, indent=2) + "\n", encoding="utf-8"
        )
        meta["meta"] = f"audio/{stem}.json"

    return meta
