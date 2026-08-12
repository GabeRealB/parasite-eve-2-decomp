"""Parasite Eve 2 SPK (``hSPK``) sound-bank codec.

Retail music chunks (CDF type ``0x6``) are **SPK** programs fed to
``SndLoad_*`` / ``SpuWritePartly``. Layout of the clean payload::

    0x00  char[4]  magic ``hSPK``
    0x04  u16      bank_id  (high nibble = bank type for Snd_AllocBank)
    0x06  u8       field_22
    0x07  u8       group_count
    0x08  u8       note_count
    0x09  u8       field_25
    0x0A  u16      field_26  (SPU transfer start page; usually 0)
    0x0C  u8       field_28
    0x0D  u8       field_29
    0x0E  u16      prog_size   (MIDI / hONE program bytes)
    0x10  s32      spu_size    (bytes of SPU-ADPCM sample pool)
    0x14  …        group table (4 B × group_count)
                   note table  (0x14 B × note_count)
    0x800 …        program (``hONE`` / ``oneV`` / ``oneC`` / ``endC`` …)
    spu_base …     SPU-ADPCM pool (note.waveAddr is offset into this pool)

``spu_base = align16(0x800 + align4(prog_size))``.

Each note's ``waveAddr`` is a byte offset into the SPU pool (16-byte ADPCM
frames). Samples run until an ADPCM frame with the end flag, or the next
note's wave offset / pool end.

Encode is not implemented: pack prefers ``raw/spk/`` for bit-identity.
"""

from __future__ import annotations

import json
import struct
import wave
from array import array
from dataclasses import dataclass, field
from pathlib import Path

SPK_MAGIC = b"hSPK"
SPK_HEADER_SIZE = 0x14
PROG_BASE = 0x800  # program / hONE starts on a 2 KiB boundary

# PSX SPU-ADPCM filter coefficients (standard)
_ADPCM_COEFFS = (
    (0, 0),
    (60, 0),
    (115, -52),
    (98, -55),
    (122, -60),
)


@dataclass
class SpkGroup:
    size: int
    volume: int
    pan: int


@dataclass
class SpkNote:
    reverb: int
    pan: int
    volume: int
    root_key: int
    root_fine: int
    priority: int
    key_min: int
    key_max: int
    bend_down: int
    bend_up: int
    adsr1: int
    adsr2: int
    wave_addr: int  # offset into SPU pool


@dataclass
class SpkInfo:
    bank_id: int
    bank_type: int  # high nibble of bank_id
    field_22: int
    field_25: int
    field_26: int
    field_28: int
    field_29: int
    prog_size: int
    spu_size: int
    groups: list[SpkGroup] = field(default_factory=list)
    notes: list[SpkNote] = field(default_factory=list)
    spu_base: int = 0
    prog_base: int = PROG_BASE
    original_size: int = 0

    @property
    def sample_count(self) -> int:
        return len({n.wave_addr for n in self.notes})


def is_spk(data: bytes) -> bool:
    return len(data) >= SPK_HEADER_SIZE and data[:4] == SPK_MAGIC


def _align4(n: int) -> int:
    return (n + 3) & ~3


def _align16(n: int) -> int:
    return (n + 15) & ~15


def parse_spk(data: bytes) -> SpkInfo:
    if not is_spk(data):
        raise ValueError(
            f"not an SPK bank (magic={data[:4]!r}, want hSPK)"
        )
    (
        bank_id,
        field_22,
        group_count,
        note_count,
        field_25,
        field_26,
        field_28,
        field_29,
        prog_size,
    ) = struct.unpack_from("<HBBBBHBBH", data, 4)
    spu_size = struct.unpack_from("<i", data, 0x10)[0]
    if group_count < 0 or note_count < 0:
        raise ValueError("invalid group/note counts")
    tab_bytes = (note_count * 5 + group_count) * 4
    if SPK_HEADER_SIZE + tab_bytes > len(data):
        raise ValueError(
            f"SPK table overruns file ({tab_bytes} B table, file {len(data)})"
        )
    tab = data[SPK_HEADER_SIZE : SPK_HEADER_SIZE + tab_bytes]

    groups: list[SpkGroup] = []
    for i in range(group_count):
        size, _pad, vol, pan = struct.unpack_from("<BBBB", tab, i * 4)
        groups.append(SpkGroup(size=size, volume=vol, pan=pan))

    notes: list[SpkNote] = []
    base = group_count * 4
    for i in range(note_count):
        o = base + i * 0x14
        if o + 0x14 > len(tab):
            break
        reverb, pan, _pad, vol, root, fine = struct.unpack_from(
            "<BBBBBB", tab, o
        )
        (prio,) = struct.unpack_from("<H", tab, o + 6)
        kmin, kmax, bd, bu = struct.unpack_from("<BBBB", tab, o + 8)
        adsr1, adsr2 = struct.unpack_from("<HH", tab, o + 0xC)
        (wave,) = struct.unpack_from("<i", tab, o + 0x10)
        notes.append(
            SpkNote(
                reverb=reverb,
                pan=pan,
                volume=vol,
                root_key=root,
                root_fine=fine,
                priority=prio,
                key_min=kmin,
                key_max=kmax,
                bend_down=bd,
                bend_up=bu,
                adsr1=adsr1,
                adsr2=adsr2,
                wave_addr=wave,
            )
        )

    prog_base = PROG_BASE
    spu_base = _align16(prog_base + _align4(prog_size))

    return SpkInfo(
        bank_id=bank_id,
        bank_type=(bank_id & 0xF000) >> 12,
        field_22=field_22,
        field_25=field_25,
        field_26=field_26,
        field_28=field_28,
        field_29=field_29,
        prog_size=prog_size,
        spu_size=spu_size,
        groups=groups,
        notes=notes,
        spu_base=spu_base,
        prog_base=prog_base,
        original_size=len(data),
    )


def decode_spu_adpcm(data: bytes) -> list[int]:
    """Decode a contiguous SPU-ADPCM stream to signed 16-bit PCM samples."""
    s_1 = 0
    s_2 = 0
    out: list[int] = []
    for i in range(0, len(data) - 15, 16):
        block = data[i : i + 16]
        shift = block[0] & 0x0F
        filt = block[0] >> 4
        flags = block[1]
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
        if flags & 1:  # end
            break
    return out


def _sample_bounds(info: SpkInfo, data_len: int) -> list[tuple[int, int, int]]:
    """Return ``(wave_addr, file_off, length)`` for each unique sample.

    Length is to the next wave offset, pool end, or file end.
    """
    pool_end = min(info.spu_base + max(info.spu_size, 0), data_len)
    addrs = sorted({n.wave_addr for n in info.notes if n.wave_addr >= 0})
    if not addrs:
        return []
    bounds: list[tuple[int, int, int]] = []
    for i, addr in enumerate(addrs):
        start = info.spu_base + addr
        if start >= data_len:
            continue
        if i + 1 < len(addrs):
            end = info.spu_base + addrs[i + 1]
        else:
            end = pool_end
        end = min(end, data_len)
        if end > start:
            bounds.append((addr, start, end - start))
    return bounds


def extract_samples(
    data: bytes, info: SpkInfo | None = None
) -> list[tuple[int, list[int]]]:
    """Return ``[(wave_addr, pcm_samples), …]`` for each unique sample."""
    if info is None:
        info = parse_spk(data)
    out: list[tuple[int, list[int]]] = []
    for addr, off, length in _sample_bounds(info, len(data)):
        pcm = decode_spu_adpcm(data[off : off + length])
        out.append((addr, pcm))
    return out


def pcm_to_wav_bytes(pcm: list[int], *, rate: int = 22050) -> bytes:
    """Mono s16le WAV at ``rate`` Hz (SPU native is ~22.05 kHz)."""
    import io

    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(array("h", pcm).tobytes())
    return buf.getvalue()


def write_wav(path: Path | str, pcm: list[int], *, rate: int = 22050) -> None:
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(array("h", pcm).tobytes())


def info_to_json_spk(info: SpkInfo) -> dict:
    return {
        "format": "spk",
        "magic": "hSPK",
        "bank_id": f"0x{info.bank_id:04X}",
        "bank_type": info.bank_type,
        "group_count": len(info.groups),
        "note_count": len(info.notes),
        "prog_size": info.prog_size,
        "spu_size": info.spu_size,
        "spu_base": info.spu_base,
        "prog_base": info.prog_base,
        "original_size": info.original_size,
        "groups": [
            {"size": g.size, "volume": g.volume, "pan": g.pan}
            for g in info.groups
        ],
        "notes": [
            {
                "wave_addr": n.wave_addr,
                "volume": n.volume,
                "pan": n.pan,
                "root_key": n.root_key,
                "root_fine": n.root_fine,
                "key_min": n.key_min,
                "key_max": n.key_max,
                "adsr1": f"0x{n.adsr1:04X}",
                "adsr2": f"0x{n.adsr2:04X}",
                "reverb": n.reverb,
                "priority": n.priority,
            }
            for n in info.notes
        ],
        "note": (
            "PE2 SPK sound bank (SndLoad). Samples are SPU-ADPCM at ~22050 Hz. "
            "Pack prefers raw/.spk; WAV export is view-only."
        ),
    }


def materialize_spk(
    data: bytes,
    dest_dir: Path,
    *,
    stem: str | None = None,
    rate: int = 22050,
) -> tuple[Path, SpkInfo]:
    """Write ``meta.json`` + ``sample_XX.wav`` under ``dest_dir``.

    Returns ``(meta_path, info)``.
    """
    info = parse_spk(data)
    dest_dir = Path(dest_dir)
    dest_dir.mkdir(parents=True, exist_ok=True)
    meta_path = dest_dir / "meta.json"
    meta = info_to_json_spk(info)
    if stem:
        meta["stem"] = stem
    samples = extract_samples(data, info)
    meta["samples"] = []
    for i, (addr, pcm) in enumerate(samples):
        name = f"sample_{i:02d}.wav"
        write_wav(dest_dir / name, pcm, rate=rate)
        meta["samples"].append(
            {
                "index": i,
                "file": name,
                "wave_addr": addr,
                "frames": len(pcm),
                "duration_sec": round(len(pcm) / rate, 4),
            }
        )
    meta_path.write_text(json.dumps(meta, indent=2) + "\n")
    return meta_path, info
