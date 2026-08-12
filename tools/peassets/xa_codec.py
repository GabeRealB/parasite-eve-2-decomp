"""PlayStation CD-ROM XA-ADPCM codec for STR movie audio.

PE2 ``INTER*.STR`` stores Mode 2 sectors of 2336 bytes::

    0x000  8 B    XA subheader (file, channel, submode, codinginfo × 2)
    0x008  2324 B Form 2 payload (2304 B = 18×128 sound groups + pad/EDC)
    ──────── 2336 B total

Video: submode ``0x48`` + STR magic ``0x80010160``.
Audio: submode ``0x64``, codinginfo ``0x01`` (stereo, 37800 Hz, 4-bit) on USA
title FMV. Typical interleave: **7 video + 1 audio**.

Sound-group decode matches FFmpeg ``xa_decode`` / BERO CD-ROM XA layout
(not sequential 16-packet packing).
"""

from __future__ import annotations

import struct
import wave
from array import array
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterator

INTER_RAW_SECTOR = 2336
INTER_USER_OFF = 8
XA_DATA_SIZE = 2304  # 18 × 128-byte sound groups
XA_SOUND_GROUP = 128
STR_MAGIC = 0x80010160

# FFmpeg xa_adpcm_table[5][2]
_XA_TABLE: tuple[tuple[int, int], ...] = (
    (0, 0),
    (60, 0),
    (115, -52),
    (98, -55),
    (122, -60),
)


@dataclass
class XaCoding:
    stereo: bool
    rate: int  # 37800 or 18900
    bits4: bool


@dataclass
class XaSectorInfo:
    file: int
    channel: int
    submode: int
    coding: int
    coding_info: XaCoding


def parse_xa_subheader(sub: bytes) -> tuple[int, int, int, int] | None:
    if len(sub) < 8:
        return None
    return sub[0], sub[1], sub[2], sub[3]


def parse_codinginfo(coding: int) -> XaCoding:
    """jPSXdec / Green Book style codinginfo."""
    stereo = (coding & 0x3) == 1
    rate = 18900 if ((coding >> 2) & 0x3) == 1 else 37800
    bits4 = ((coding >> 4) & 0x3) != 1
    return XaCoding(stereo=stereo, rate=rate, bits4=bits4)


def is_str_video_sector(raw2336: bytes) -> bool:
    if len(raw2336) < INTER_USER_OFF + 4:
        return False
    return struct.unpack_from("<I", raw2336, INTER_USER_OFF)[0] == STR_MAGIC


def is_xa_audio_sector(raw2336: bytes) -> bool:
    if len(raw2336) < INTER_RAW_SECTOR:
        return False
    if is_str_video_sector(raw2336):
        return False
    info = parse_xa_subheader(raw2336[0:8])
    if info is None:
        return False
    _f, _c, submode, _coding = info
    return submode == 0x64 or (submode & 0x04) != 0


def _clamp16(x: int) -> int:
    if x > 32767:
        return 32767
    if x < -32768:
        return -32768
    return x


def _sign4(n: int) -> int:
    n &= 0x0F
    return n - 16 if n >= 8 else n


def decode_xa_sound_group(
    group: bytes,
    *,
    stereo: bool,
    prev: list[list[int]],
) -> list[list[int]]:
    """Decode one 128-byte XA sound group (FFmpeg ``xa_decode`` layout).

    *prev* is ``[[sample1, sample2], ...]`` per channel (mutated).
    Returns list of channels; each gets **112** new samples (4×28).
    """
    nch = 2 if stereo else 1
    # Per channel: accumulate 4 blocks × 28 samples
    out: list[list[int]] = [[] for _ in range(nch)]

    # State mirrors FFmpeg: left=prev[0], right=prev[1]
    left_s1, left_s2 = prev[0][0], prev[0][1]
    right_s1, right_s2 = (
        (prev[1][0], prev[1][1]) if nch > 1 else (0, 0)
    )

    for i in range(4):
        # --- left (or mono primary) ---
        hdr = group[4 + i * 2]
        shift = 12 - (hdr & 0x0F)
        filt = hdr >> 4
        if filt >= len(_XA_TABLE) or shift < 0:
            filt = 0
            if shift < 0:
                shift = 0
        f0, f1 = _XA_TABLE[filt]
        s_1, s_2 = left_s1, left_s2
        blk_l: list[int] = []
        for j in range(28):
            d = group[16 + i + j * 4]
            t = _sign4(d)
            s = t * (1 << shift) + ((s_1 * f0 + s_2 * f1 + 32) >> 6)
            s = _clamp16(s)
            s_2, s_1 = s_1, s
            blk_l.append(s)
        left_s1, left_s2 = s_1, s_2

        # --- right (stereo) or second mono stream in same group ---
        hdr = group[5 + i * 2]
        shift = 12 - (hdr & 0x0F)
        filt = hdr >> 4
        if filt >= len(_XA_TABLE) or shift < 0:
            filt = 0
            if shift < 0:
                shift = 0
        f0, f1 = _XA_TABLE[filt]
        if stereo:
            s_1, s_2 = right_s1, right_s2
        else:
            # mono: FFmpeg continues into out0+28 using left state after first half
            s_1, s_2 = left_s1, left_s2
        blk_r: list[int] = []
        for j in range(28):
            d = group[16 + i + j * 4]
            t = _sign4(d >> 4)
            s = t * (1 << shift) + ((s_1 * f0 + s_2 * f1 + 32) >> 6)
            s = _clamp16(s)
            s_2, s_1 = s_1, s
            blk_r.append(s)

        if stereo:
            right_s1, right_s2 = s_1, s_2
            out[0].extend(blk_l)
            out[1].extend(blk_r)
        else:
            left_s1, left_s2 = s_1, s_2
            out[0].extend(blk_l)
            out[0].extend(blk_r)

    prev[0][0], prev[0][1] = left_s1, left_s2
    if nch > 1:
        prev[1][0], prev[1][1] = right_s1, right_s2
    return out


def decode_xa_sector_pcm(
    raw2336: bytes,
    *,
    prev: list[list[int]] | None = None,
) -> tuple[list[list[int]], XaSectorInfo | None, list[list[int]]]:
    """Decode one INTER XA audio sector. Returns (channels, info, new_prev)."""
    if not is_xa_audio_sector(raw2336):
        return [], None, prev or [[0, 0], [0, 0]]

    parsed = parse_xa_subheader(raw2336[0:8])
    assert parsed is not None
    file_n, channel, submode, coding = parsed
    ci = parse_codinginfo(coding)
    if not ci.bits4:
        return [], None, prev or [[0, 0], [0, 0]]

    info = XaSectorInfo(
        file=file_n,
        channel=channel,
        submode=submode,
        coding=coding,
        coding_info=ci,
    )
    data = raw2336[INTER_USER_OFF : INTER_USER_OFF + XA_DATA_SIZE]
    nch = 2 if ci.stereo else 1
    if prev is None or len(prev) < nch:
        prev = [[0, 0] for _ in range(nch)]
    else:
        prev = [list(p) for p in prev[:nch]]

    channels: list[list[int]] = [[] for _ in range(nch)]
    for g in range(18):
        group = data[g * XA_SOUND_GROUP : (g + 1) * XA_SOUND_GROUP]
        part = decode_xa_sound_group(group, stereo=ci.stereo, prev=prev)
        for ch in range(nch):
            channels[ch].extend(part[ch])
    return channels, info, prev


def iter_inter_raw_sectors(
    raw: bytes, start: int, nsectors: int
) -> Iterator[tuple[int, bytes]]:
    for s in range(start, start + nsectors):
        base = s * INTER_RAW_SECTOR
        if base + INTER_RAW_SECTOR > len(raw):
            break
        yield s, raw[base : base + INTER_RAW_SECTOR]


def demux_xa_from_inter(
    raw: bytes,
    start: int,
    nsectors: int,
) -> tuple[list[list[int]], dict[str, Any]]:
    """Decode all XA audio sectors in an INTER window to PCM channels."""
    prev: list[list[int]] | None = None
    acc: list[list[int]] | None = None
    n_audio = n_video = n_other = 0
    rate = 37800
    stereo = True
    coding_byte = 0
    file_ch: tuple[int, int] | None = None

    for _s, sec in iter_inter_raw_sectors(raw, start, nsectors):
        if is_str_video_sector(sec):
            n_video += 1
            continue
        if not is_xa_audio_sector(sec):
            n_other += 1
            continue
        chs, info, prev = decode_xa_sector_pcm(sec, prev=prev)
        if not chs or info is None:
            n_other += 1
            continue
        n_audio += 1
        rate = info.coding_info.rate
        stereo = info.coding_info.stereo
        coding_byte = info.coding
        file_ch = (info.file, info.channel)
        if acc is None:
            acc = [[] for _ in chs]
        for i, c in enumerate(chs):
            if i < len(acc):
                acc[i].extend(c)

    meta: dict[str, Any] = {
        "format": "xa-adpcm",
        "audio_sectors": n_audio,
        "video_sectors": n_video,
        "other_sectors": n_other,
        "sample_rate": rate,
        "channels": 2 if stereo else 1,
        "codinginfo": f"0x{coding_byte:02X}",
    }
    if file_ch:
        meta["xa_file"] = file_ch[0]
        meta["xa_channel"] = file_ch[1]
    if acc and acc[0]:
        meta["samples"] = len(acc[0])
        meta["duration_sec"] = round(len(acc[0]) / float(rate), 3) if rate else 0
    else:
        meta["samples"] = 0
        meta["duration_sec"] = 0
    return acc or [], meta


def write_wav_s16(
    path: Path | str,
    channels_pcm: list[list[int]],
    *,
    rate: int,
) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    if not channels_pcm or not channels_pcm[0]:
        raise ValueError("no PCM")
    nch = 1 if len(channels_pcm) == 1 else 2
    if nch == 1:
        frames = array("h", channels_pcm[0])
    else:
        left, right = channels_pcm[0], channels_pcm[1]
        n = max(len(left), len(right))
        interleaved: list[int] = []
        for i in range(n):
            interleaved.append(left[i] if i < len(left) else 0)
            interleaved.append(right[i] if i < len(right) else 0)
        frames = array("h", interleaved)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(nch)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(frames.tobytes())
