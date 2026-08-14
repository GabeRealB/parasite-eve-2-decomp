#!/usr/bin/env python3
"""PE2 assets viewer — browse by type or stage tree with previews.

Usage (from repo root)::

    python3 tools/peassets/viewer.py
    python3 tools/peassets/viewer.py assets/USA

Works with full extract (inflated type dirs + ``stages.json``) and
minimal/raw-only extract (``raw/{type}/`` + optional ``pe2pkg/`` overlays).

**Audio streams** (``audio/*.wav`` + ``raw/audio/*.mts``) show waveform +
transport (Play/Stop/seek).

**Movies** (``movie/*.mp4`` + ``*.json``) show meta, a poster frame, and the
same transport — Play opens **ffplay** with video (lossless H.264 + ALAC).
Install ``ffmpeg`` for poster extraction and playback.
"""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import time
import tkinter as tk
import wave
from array import array
from dataclasses import dataclass
from pathlib import Path
from tkinter import filedialog, ttk
from typing import Any

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from names import (  # noqa: E402
    asset_name_key,
    chunk_key,
    lookup_image_bpp,
    reverse_chunk_idx,
    reverse_file_id,
    reverse_folder_id,
)

TYPE_DIRS = (
    "pe2pkg",
    "pe2img",
    "pe2clut",
    "pe2cap2",
    "bs",
    "spk",
    "audio",
    "movie",
    "txt",
)
IMAGE_EXTS = {".png", ".pe2img", ".pe2clut", ".bs"}
TEXT_EXTS = {".txt", ".json", ".md", ".xml", ".cnf"}
PREVIEW_HEX_BYTES = 512
PREVIEW_TEXT_CHARS = 200_000

# stages.json type id → raw type-dir + extension
_TYPE_RAW_EXT: dict[str, tuple[str, str]] = {
    "room_pkg": ("pe2pkg", ".pe2pkg"),
    "image": ("pe2img", ".pe2img"),
    "clut": ("pe2clut", ".pe2clut"),
    "music": ("spk", ".spk"),
    "ascii": ("txt", ".txt"),
    "cap2": ("pe2cap2", ".pe2cap2"),
    "room_background": ("bs", ".bs"),
}


def _wav_info(path: Path) -> dict[str, Any]:
    """Return nchannels, framerate, nframes, duration_sec, sampwidth."""
    with wave.open(str(path), "rb") as w:
        nch = w.getnchannels()
        rate = w.getframerate()
        nframes = w.getnframes()
        sw = w.getsampwidth()
    return {
        "nchannels": nch,
        "framerate": rate,
        "nframes": nframes,
        "sampwidth": sw,
        "duration_sec": nframes / rate if rate else 0.0,
    }


def _wav_pcm_mono_preview(path: Path, max_samples: int = 200_000) -> list[int]:
    """Downmix WAV to mono int samples for waveform drawing (may decimate)."""
    with wave.open(str(path), "rb") as w:
        nch = w.getnchannels()
        nframes = w.getnframes()
        sw = w.getsampwidth()
        raw = w.readframes(nframes)
    if sw != 2:
        # Only s16 supported for preview
        return []
    samples = array("h")
    samples.frombytes(raw)
    if nch == 2:
        mono = [
            (samples[i] + samples[i + 1]) // 2 for i in range(0, len(samples) - 1, 2)
        ]
    elif nch == 1:
        mono = list(samples)
    else:
        mono = [samples[i] for i in range(0, len(samples), nch)]
    if len(mono) > max_samples:
        step = len(mono) / max_samples
        mono = [mono[int(i * step)] for i in range(max_samples)]
    return mono


def _find_audio_player() -> list[str] | None:
    """Return argv prefix for a background WAV (audio-only) player, or None."""
    if shutil.which("ffplay"):
        return [
            "ffplay",
            "-nodisp",
            "-autoexit",
            "-loglevel",
            "quiet",
            "-nostats",
        ]
    if shutil.which("afplay"):  # macOS
        return ["afplay"]
    if shutil.which("aplay"):  # ALSA
        return ["aplay", "-q"]
    if shutil.which("paplay"):  # PulseAudio
        return ["paplay"]
    return None


def _find_video_player() -> list[str] | None:
    """Return argv prefix for windowed video playback (ffplay preferred)."""
    if shutil.which("ffplay"):
        return [
            "ffplay",
            "-autoexit",
            "-loglevel",
            "quiet",
            "-nostats",
            # Slightly larger than native 320×240 for readability
            "-x",
            "640",
            "-y",
            "480",
        ]
    if shutil.which("mpv"):
        return ["mpv", "--force-window=yes", "--really-quiet"]
    if shutil.which("vlc"):
        return ["vlc", "--play-and-exit", "-I", "dummy", "--quiet"]
    return None


def _mp4_poster_pil(mp4: Path, *, at_sec: float = 0.5):
    """Grab one RGB frame from *mp4* via ffmpeg; return PIL Image or None."""
    if not shutil.which("ffmpeg") or not mp4.is_file():
        return None
    try:
        from PIL import Image as PILImage
    except ImportError:
        return None
    cmd = [
        "ffmpeg",
        "-hide_banner",
        "-loglevel",
        "error",
        "-ss",
        f"{at_sec:.3f}",
        "-i",
        str(mp4),
        "-frames:v",
        "1",
        "-f",
        "image2pipe",
        "-vcodec",
        "png",
        "pipe:1",
    ]
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0 or not proc.stdout:
        # Retry from t=0 if mid-clip seek failed
        if at_sec > 0:
            return _mp4_poster_pil(mp4, at_sec=0.0)
        return None
    try:
        from io import BytesIO

        img = PILImage.open(BytesIO(proc.stdout))
        img.load()
        return img.convert("RGB")
    except Exception:
        return None


def _probe_media_duration(path: Path) -> float | None:
    """Return duration in seconds via ffprobe, or None."""
    if not shutil.which("ffprobe") or not path.is_file():
        return None
    cmd = [
        "ffprobe",
        "-v",
        "error",
        "-show_entries",
        "format=duration",
        "-of",
        "default=noprint_wrappers=1:nokey=1",
        str(path),
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
    except (OSError, subprocess.TimeoutExpired):
        return None
    if proc.returncode != 0:
        return None
    try:
        return float((proc.stdout or "").strip())
    except ValueError:
        return None


@dataclass
class AssetItem:
    """One browsable asset (on-disk file)."""

    path: Path
    label: str
    kind: str  # type dir name or "stage" / "sidecar"
    source: str  # "inflated" | "raw" | "stage"
    meta: dict[str, Any] | None = None
    # Sibling CLUT paths when known from the same stage file (for pe2img).
    sibling_cluts: list[Path] | None = None


def _fmt_size(n: int) -> str:
    if n < 1024:
        return f"{n} B"
    if n < 1024 * 1024:
        return f"{n / 1024:.1f} KiB"
    return f"{n / (1024 * 1024):.2f} MiB"


def _hexdump(data: bytes, limit: int = PREVIEW_HEX_BYTES) -> str:
    lines: list[str] = []
    chunk = data[:limit]
    for i in range(0, len(chunk), 16):
        row = chunk[i : i + 16]
        hex_part = " ".join(f"{b:02x}" for b in row)
        ascii_part = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        lines.append(f"{i:08x}  {hex_part:<48}  {ascii_part}")
    if len(data) > limit:
        lines.append(f"… ({len(data) - limit} more bytes)")
    return "\n".join(lines)


def _printable_strings(data: bytes, min_len: int = 6, limit: int = 40) -> list[str]:
    out: list[str] = []
    cur: list[str] = []
    for b in data:
        if 32 <= b < 127:
            cur.append(chr(b))
        else:
            if len(cur) >= min_len:
                out.append("".join(cur))
                if len(out) >= limit:
                    return out
            cur = []
    if len(cur) >= min_len and len(out) < limit:
        out.append("".join(cur))
    return out


def _load_stages(assets_root: Path) -> dict[str, Any] | None:
    path = assets_root / "stages.json"
    if not path.exists():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def prefer_raw_blob(assets_root: Path, path: Path) -> Path:
    """Prefer ``raw/{type}/stem.pe2*`` over inflated PNG/duplicates.

    Sibling CLUT lists must point at on-disc ``.pe2clut`` blobs. Inflated
    ``pe2clut/*.png`` is a colour strip for viewing, not the palette payload;
    loading it as a CLUT produces wrong results vs ``raw/pe2clut/*.pe2clut``.
    """
    if not path:
        return path
    suf = path.suffix.lower()
    stem = path.stem
    # Already raw pe2 / bs
    if "raw" in path.parts and suf in (
        ".pe2img",
        ".pe2clut",
        ".pe2pkg",
        ".bs",
    ):
        return path
    if suf == ".pe2img":
        raw = assets_root / "raw" / "pe2img" / f"{stem}.pe2img"
        return raw if raw.exists() else path
    if suf == ".pe2clut":
        raw = assets_root / "raw" / "pe2clut" / f"{stem}.pe2clut"
        return raw if raw.exists() else path
    if suf == ".bs":
        raw = assets_root / "raw" / "bs" / f"{stem}.bs"
        return raw if raw.exists() else path
    if suf == ".png":
        # pe2img/foo.png, pe2clut/foo.png, or bs/foo.png
        if "bs" in path.parts or path.parent.name == "bs":
            raw = assets_root / "raw" / "bs" / f"{stem}.bs"
            return raw if raw.exists() else path
        if "pe2img" in path.parts or path.parent.name == "pe2img":
            raw = assets_root / "raw" / "pe2img" / f"{stem}.pe2img"
            if raw.exists():
                return raw
        if "pe2clut" in path.parts or path.parent.name == "pe2clut":
            raw = assets_root / "raw" / "pe2clut" / f"{stem}.pe2clut"
            if raw.exists():
                return raw
    return path


def _resolve_on_disk(
    assets_root: Path, rel: str, type_id: str | None = None
) -> Path | None:
    """Resolve a stages.json path, preferring raw pe2 blobs for image/clut."""
    if not rel:
        return None
    stem = Path(rel).stem
    parts = Path(rel).parts

    # Prefer raw for image / clut (decode-ready payloads).
    if type_id in ("image", "clut") or (
        parts and parts[0] in ("pe2img", "pe2clut")
    ):
        if type_id in _TYPE_RAW_EXT:
            tdir, ext = _TYPE_RAW_EXT[type_id]
        elif parts:
            tdir = parts[0]
            ext = ".pe2img" if tdir == "pe2img" else ".pe2clut"
        else:
            tdir, ext = "pe2img", ".pe2img"
        raw = assets_root / "raw" / tdir / f"{stem}{ext}"
        if raw.exists():
            return raw

    p = assets_root / rel
    if p.exists():
        return prefer_raw_blob(assets_root, p)

    if type_id in _TYPE_RAW_EXT:
        tdir, ext = _TYPE_RAW_EXT[type_id]
        raw = assets_root / "raw" / tdir / f"{stem}{ext}"
        if raw.exists():
            return raw
    if len(parts) >= 2 and parts[0] in TYPE_DIRS:
        tdir = parts[0]
        for ext in (
            ".pe2img",
            ".pe2clut",
            ".pe2pkg",
            ".spk",
            ".txt",
            ".bs",
            ".pe2cap2",
            Path(rel).suffix,
        ):
            raw = assets_root / "raw" / tdir / f"{stem}{ext}"
            if raw.exists():
                return raw
    return None


def _iter_stage_files(
    stages: dict[str, Any],
) -> list[dict[str, dict[str, Any]]]:
    """Yield each file's chunks dict from stages.json."""
    files: list[dict[str, dict[str, Any]]] = []
    for stage in stages.values():
        if not isinstance(stage, dict):
            continue
        if isinstance(stage.get("files"), dict):
            for chunks in stage["files"].values():
                if isinstance(chunks, dict):
                    files.append(chunks)
        if isinstance(stage.get("folders"), dict):
            for folder_files in stage["folders"].values():
                if not isinstance(folder_files, dict):
                    continue
                for chunks in folder_files.values():
                    if isinstance(chunks, dict):
                        files.append(chunks)
    return files


def build_image_clut_index(
    assets_root: Path, stages: dict[str, Any] | None
) -> dict[str, list[Path]]:
    """Map image identity → sibling CLUT paths from the same stage file.

    Keys include resolved path, file name, and stem so lookups work for both
    inflated PNGs and raw ``.pe2img`` paths.
    """
    index: dict[str, list[Path]] = {}
    if not stages:
        return index

    def _add(key: str, cluts: list[Path]) -> None:
        existing = index.setdefault(key, [])
        for c in cluts:
            if c not in existing:
                existing.append(c)

    for chunks in _iter_stage_files(stages):
        images: list[Path] = []
        cluts: list[Path] = []
        for ent in chunks.values():
            if not isinstance(ent, dict):
                continue
            t = ent.get("type")
            path = _resolve_on_disk(assets_root, ent.get("path") or "", t)
            if path is None:
                continue
            is_img = t == "image" or path.suffix.lower() == ".pe2img" or (
                path.suffix.lower() == ".png" and "pe2img" in path.parts
            )
            is_clut = t == "clut" or path.suffix.lower() == ".pe2clut" or (
                path.suffix.lower() == ".png" and "pe2clut" in path.parts
            )
            if is_img:
                path = prefer_raw_blob(assets_root, path)
                if path.exists() and path.suffix.lower() == ".pe2img":
                    images.append(path.resolve())
            elif is_clut:
                path = prefer_raw_blob(assets_root, path)
                if path.exists() and path.suffix.lower() == ".pe2clut":
                    cluts.append(path.resolve())
        if not images or not cluts:
            continue
        seen: set[Path] = set()
        uniq_cluts: list[Path] = []
        for c in cluts:
            if c not in seen:
                seen.add(c)
                uniq_cluts.append(c)
        for img in images:
            _add(str(img), uniq_cluts)
            _add(img.name, uniq_cluts)
            _add(img.stem, uniq_cluts)
    return index


def build_image_canon_index(
    stages: dict[str, Any] | None,
) -> dict[str, list[str]]:
    """Map image stem / filename / path → canonical chunk keys.

    Canonical form matches :data:`names.NAMES` / :data:`names.IMAGE_BPP`
    (``stage0/file2/2``, ``stage1/101/file0/1``).
    """
    index: dict[str, list[str]] = {}
    if not stages:
        return index

    def _add(key: str, canon: str) -> None:
        existing = index.setdefault(key, [])
        if canon not in existing:
            existing.append(canon)

    def _index_file(
        stage: int,
        folder_id: int | None,
        file_name: str,
        chunks: dict[str, Any],
    ) -> None:
        file_id = reverse_file_id(stage, file_name, folder_id)
        if file_id is None or not isinstance(chunks, dict):
            return
        for chunk_name, ent in chunks.items():
            if not isinstance(ent, dict):
                continue
            t = ent.get("type")
            rel = ent.get("path") or ""
            is_img = t == "image" or str(rel).endswith(".pe2img") or (
                str(rel).endswith(".png") and "pe2img" in str(rel).replace("\\", "/")
            )
            if not is_img:
                continue
            stem = Path(chunk_name).stem
            idx = reverse_chunk_idx(stage, file_id, stem, folder_id)
            if idx is None and Path(rel).stem:
                idx = reverse_chunk_idx(stage, file_id, Path(rel).stem, folder_id)
            if idx is None:
                continue
            canon = chunk_key(stage, file_id, idx, folder_id)
            _add(stem, canon)
            _add(Path(chunk_name).name, canon)
            if rel:
                _add(rel.replace("\\", "/"), canon)
                _add(Path(rel).stem, canon)
                _add(Path(rel).name, canon)

    for stage_name, stage in stages.items():
        if not isinstance(stage, dict) or not str(stage_name).startswith("stage"):
            continue
        try:
            stage_n = int(str(stage_name)[5:])
        except ValueError:
            continue
        files = stage.get("files")
        if isinstance(files, dict):
            for file_name, chunks in files.items():
                _index_file(stage_n, None, str(file_name), chunks)
        folders = stage.get("folders")
        if isinstance(folders, dict):
            for folder_name, folder_files in folders.items():
                if not isinstance(folder_files, dict):
                    continue
                folder_id = reverse_folder_id(stage_n, str(folder_name))
                if folder_id is None:
                    continue
                for file_name, chunks in folder_files.items():
                    _index_file(stage_n, folder_id, str(file_name), chunks)
    return index


def list_all_cluts(assets_root: Path) -> list[Path]:
    """All available pe2clut blobs (raw preferred)."""
    out: list[Path] = []
    raw = assets_root / "raw" / "pe2clut"
    infl = assets_root / "pe2clut"
    if raw.is_dir():
        out.extend(
            sorted(
                (p for p in raw.glob("*.pe2clut") if p.is_file()),
                key=asset_name_key,
            )
        )
    elif infl.is_dir():
        # only pngs — skip, need pe2 for palette load
        pass
    return out


def pe2img_path_for_preview(assets_root: Path, path: Path) -> Path | None:
    """Return a .pe2img path for decoding (raw), or None if only PNG available."""
    if path.suffix.lower() == ".pe2img":
        return path
    if path.suffix.lower() == ".png":
        raw = assets_root / "raw" / "pe2img" / f"{path.stem}.pe2img"
        if raw.exists():
            return raw
    return None


def scan_type_assets(assets_root: Path) -> dict[str, list[AssetItem]]:
    """Group assets by type dir, preferring inflated over raw of the same name."""
    by_type: dict[str, list[AssetItem]] = {t: [] for t in TYPE_DIRS}
    for type_dir in TYPE_DIRS:
        infl_dir = assets_root / type_dir
        raw_dir = assets_root / "raw" / type_dir
        seen_stems: set[str] = set()

        if infl_dir.is_dir():
            # CD audio streams: audio/{stem}.wav + {stem}.json (+ raw/audio/*.mts)
            if type_dir == "audio":
                for p in sorted(infl_dir.glob("*.wav"), key=asset_name_key):
                    if not p.is_file():
                        continue
                    seen_stems.add(p.stem)
                    meta_path = infl_dir / f"{p.stem}.json"
                    meta: dict[str, Any] | None = None
                    if meta_path.is_file():
                        try:
                            meta = json.loads(
                                meta_path.read_text(encoding="utf-8")
                            )
                        except (OSError, json.JSONDecodeError):
                            meta = None
                    by_type[type_dir].append(
                        AssetItem(
                            path=p,
                            label=p.stem,
                            kind=type_dir,
                            source="inflated",
                            meta=meta,
                        )
                    )
                # fall through to raw for stems without WAV
            elif type_dir == "movie":
                # movie/{stem}.mp4 + {stem}.json (legacy: dir/meta, webp, wav)
                for p in sorted(infl_dir.glob("*.mp4"), key=asset_name_key):
                    seen_stems.add(p.stem)
                    meta: dict[str, Any] | None = None
                    meta_path = infl_dir / f"{p.stem}.json"
                    if meta_path.is_file():
                        try:
                            meta = json.loads(meta_path.read_text(encoding="utf-8"))
                        except (OSError, json.JSONDecodeError):
                            meta = None
                    by_type[type_dir].append(
                        AssetItem(
                            path=p,
                            label=p.stem,
                            kind=type_dir,
                            source="inflated",
                            meta=meta,
                        )
                    )
                for p in sorted(infl_dir.iterdir(), key=asset_name_key):
                    if p.is_dir() and (p / "meta.json").is_file() and p.name not in seen_stems:
                        seen_stems.add(p.name)
                        meta = None
                        try:
                            meta = json.loads(
                                (p / "meta.json").read_text(encoding="utf-8")
                            )
                        except (OSError, json.JSONDecodeError):
                            meta = None
                        by_type[type_dir].append(
                            AssetItem(
                                path=p / "meta.json",
                                label=p.name,
                                kind=type_dir,
                                source="inflated",
                                meta=meta,
                            )
                        )
                    elif (
                        p.is_file()
                        and p.suffix.lower() in (".webp", ".wav", ".json")
                        and p.stem not in seen_stems
                    ):
                        if p.suffix.lower() == ".json":
                            continue  # listed via .mp4
                        seen_stems.add(p.stem)
                        by_type[type_dir].append(
                            AssetItem(
                                path=p,
                                label=p.stem,
                                kind=type_dir,
                                source="inflated",
                            )
                        )
            else:
                for p in sorted(infl_dir.iterdir(), key=asset_name_key):
                    # SPK inflates to directories: spk/{stem}/meta.json + sample_*.wav
                    if type_dir == "spk" and p.is_dir():
                        meta = p / "meta.json"
                        if meta.is_file():
                            seen_stems.add(p.name)
                            by_type[type_dir].append(
                                AssetItem(
                                    path=meta,
                                    label=p.name,
                                    kind=type_dir,
                                    source="inflated",
                                )
                            )
                        continue
                    if not p.is_file():
                        continue
                    # Skip sidecar meta next to PNGs in listing primary? keep them.
                    if p.suffix.lower() == ".json" and p.name.endswith(
                        (".pe2img.json", ".pe2clut.json", ".bs.json")
                    ):
                        continue
                    if type_dir == "audio" and p.suffix.lower() == ".json":
                        continue  # listed via .wav
                    if p.name == "streams.json":
                        continue
                    seen_stems.add(p.stem)
                    by_type[type_dir].append(
                        AssetItem(
                            path=p,
                            label=p.name,
                            kind=type_dir,
                            source="inflated",
                        )
                    )

        if raw_dir.is_dir():
            for p in sorted(raw_dir.iterdir(), key=asset_name_key):
                if not p.is_file():
                    continue
                # Prefer inflated if same stem exists (e.g. pe2img_0.png vs .pe2img)
                if p.stem in seen_stems:
                    continue
                # Inflated PNG / SPK dir / same-suffix opaque
                if (assets_root / type_dir / f"{p.stem}.png").exists():
                    continue
                if (assets_root / type_dir / p.stem).is_dir():
                    continue
                if (assets_root / type_dir / f"{p.stem}{p.suffix}").exists():
                    continue
                if type_dir == "audio" and (
                    assets_root / type_dir / f"{p.stem}.wav"
                ).exists():
                    continue
                by_type[type_dir].append(
                    AssetItem(
                        path=p,
                        label=f"{p.name}  [raw]",
                        kind=type_dir,
                        source="raw",
                    )
                )

    return by_type


class AssetViewer(tk.Tk):
    def __init__(self, assets_root: Path):
        super().__init__()
        self.title("PE2 Assets Viewer")
        self.geometry("1100x720")
        self.minsize(800, 500)

        self.assets_root = assets_root.resolve()
        self.stages = _load_stages(self.assets_root)
        self._photo_ref: Any = None  # keep PhotoImage alive
        self._bg_photo: Any = None
        self._palette_photo: Any = None
        self._item_map: dict[str, AssetItem] = {}
        self._type_items: dict[str, list[AssetItem]] = {}  # type_dir → items
        self._filter_var = tk.StringVar(value="")
        self._status_var = tk.StringVar(value="")
        self._filter_after: str | None = None
        # image → sibling CLUTs from stages.json
        self._img_clut_index: dict[str, list[Path]] = {}
        # image stem/name → canonical chunk keys (for IMAGE_BPP)
        self._img_canon_index: dict[str, list[str]] = {}
        # current pe2img decode state for CLUT swapping
        self._pe2img_path: Path | None = None
        self._pe2img_data: bytes | None = None
        self._clut_choices: list[tuple[str, Path | None]] = []  # label, path|None
        self._clut_color_cache: dict[str, list[int] | None] = {}
        self._current_clut_colors: list[int] | None = None

        # Media player state (WAV audio streams + movie MP4)
        self._audio_proc: subprocess.Popen[Any] | None = None
        self._audio_path: Path | None = None
        self._audio_duration: float = 0.0
        self._audio_started_at: float = 0.0  # time.monotonic when play began
        self._audio_start_offset: float = 0.0  # seek offset at play start
        self._audio_tick_after: str | None = None
        self._audio_player_cmd: list[str] | None = _find_audio_player()
        self._video_player_cmd: list[str] | None = _find_video_player()
        self._audio_seeking = False
        self._media_is_video = False

        self.protocol("WM_DELETE_WINDOW", self._on_close)
        self._build_ui()
        self.reload()

    # ------------------------------------------------------------------ UI
    def _build_ui(self) -> None:
        # Menu
        menubar = tk.Menu(self)
        file_menu = tk.Menu(menubar, tearoff=0)
        file_menu.add_command(label="Open assets folder…", command=self._pick_root)
        file_menu.add_command(label="Reload", command=self.reload)
        file_menu.add_separator()
        file_menu.add_command(label="Quit", command=self.destroy)
        menubar.add_cascade(label="File", menu=file_menu)
        self.config(menu=menubar)

        # Top bar
        top = ttk.Frame(self, padding=(8, 6))
        top.pack(side=tk.TOP, fill=tk.X)
        ttk.Label(top, text="Assets:").pack(side=tk.LEFT)
        self.root_label = ttk.Label(top, text=str(self.assets_root), foreground="#444")
        self.root_label.pack(side=tk.LEFT, padx=(4, 12))
        ttk.Label(top, text="Filter:").pack(side=tk.LEFT)
        filt = ttk.Entry(top, textvariable=self._filter_var, width=28)
        filt.pack(side=tk.LEFT, padx=4)
        filt.bind("<KeyRelease>", self._on_filter_key)
        ttk.Button(top, text="Reload", command=self.reload).pack(side=tk.RIGHT)

        # Main split
        paned = ttk.Panedwindow(self, orient=tk.HORIZONTAL)
        paned.pack(fill=tk.BOTH, expand=True, padx=8, pady=(0, 4))

        left = ttk.Frame(paned)
        right = ttk.Frame(paned)
        paned.add(left, weight=1)
        paned.add(right, weight=2)

        self.notebook = ttk.Notebook(left)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        # By type
        type_frame = ttk.Frame(self.notebook)
        self.notebook.add(type_frame, text="By type")
        self.type_tree = ttk.Treeview(
            type_frame, columns=("info",), show="tree headings", selectmode="browse"
        )
        self.type_tree.heading("#0", text="Asset")
        self.type_tree.heading("info", text="Size")
        self.type_tree.column("#0", width=260, stretch=True)
        self.type_tree.column("info", width=90, stretch=False, anchor=tk.E)
        type_scroll = ttk.Scrollbar(
            type_frame, orient=tk.VERTICAL, command=self.type_tree.yview
        )
        self.type_tree.configure(yscrollcommand=type_scroll.set)
        self.type_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        type_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.type_tree.bind("<<TreeviewSelect>>", lambda e: self._on_select(self.type_tree))
        self.type_tree.bind("<<TreeviewOpen>>", self._on_type_open)

        # Stage tree
        stage_frame = ttk.Frame(self.notebook)
        self.notebook.add(stage_frame, text="Stage tree")
        self.stage_tree = ttk.Treeview(
            stage_frame, columns=("info",), show="tree headings", selectmode="browse"
        )
        self.stage_tree.heading("#0", text="Stage / file / chunk")
        self.stage_tree.heading("info", text="Type")
        self.stage_tree.column("#0", width=280, stretch=True)
        self.stage_tree.column("info", width=90, stretch=False)
        stage_scroll = ttk.Scrollbar(
            stage_frame, orient=tk.VERTICAL, command=self.stage_tree.yview
        )
        self.stage_tree.configure(yscrollcommand=stage_scroll.set)
        self.stage_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        stage_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.stage_tree.bind(
            "<<TreeviewSelect>>", lambda e: self._on_select(self.stage_tree)
        )
        self.stage_tree.bind("<<TreeviewOpen>>", self._on_stage_open)

        # Preview
        prev_header = ttk.Frame(right)
        prev_header.pack(side=tk.TOP, fill=tk.X, pady=(0, 4))
        ttk.Label(prev_header, text="Preview", font=("", 11, "bold")).pack(side=tk.LEFT)
        self.info_label = ttk.Label(prev_header, text="", foreground="#555")
        self.info_label.pack(side=tk.LEFT, padx=12)

        self.preview_nb = ttk.Notebook(right)
        self.preview_nb.pack(fill=tk.BOTH, expand=True)

        # Image canvas + BPP / CLUT controls
        img_frame = ttk.Frame(self.preview_nb)
        self.preview_nb.add(img_frame, text="Image")
        clut_bar = ttk.Frame(img_frame)
        clut_bar.pack(side=tk.TOP, fill=tk.X, pady=(0, 4))
        ttk.Label(clut_bar, text="BPP:").pack(side=tk.LEFT)
        self._bpp_var = tk.StringVar(value="Auto")
        self._bpp_combo = ttk.Combobox(
            clut_bar,
            textvariable=self._bpp_var,
            values=("Auto", "4", "8", "16"),
            state="disabled",
            width=6,
        )
        self._bpp_combo.pack(side=tk.LEFT, padx=(4, 10))
        self._bpp_combo.bind(
            "<<ComboboxSelected>>", lambda _e: self._on_clut_changed()
        )
        ttk.Label(clut_bar, text="CLUT:").pack(side=tk.LEFT)
        self._clut_apply = tk.BooleanVar(value=True)
        self._clut_apply_cb = ttk.Checkbutton(
            clut_bar,
            text="Apply",
            variable=self._clut_apply,
            command=self._on_clut_changed,
        )
        self._clut_apply_cb.pack(side=tk.LEFT, padx=(4, 8))
        self._clut_var = tk.StringVar(value="(none)")
        self._clut_combo = ttk.Combobox(
            clut_bar,
            textvariable=self._clut_var,
            state="disabled",
            width=42,
        )
        self._clut_combo.pack(side=tk.LEFT, fill=tk.X, expand=True)
        self._clut_combo.bind("<<ComboboxSelected>>", lambda _e: self._on_clut_changed())
        self._clut_scope = tk.StringVar(value="siblings")
        ttk.Radiobutton(
            clut_bar,
            text="Siblings",
            variable=self._clut_scope,
            value="siblings",
            command=self._on_clut_scope_changed,
        ).pack(side=tk.LEFT, padx=(8, 0))
        ttk.Radiobutton(
            clut_bar,
            text="All CLUTs",
            variable=self._clut_scope,
            value="all",
            command=self._on_clut_scope_changed,
        ).pack(side=tk.LEFT)
        self.img_canvas = tk.Canvas(img_frame, bg="#2b2b2b", highlightthickness=0)
        self.img_canvas.pack(fill=tk.BOTH, expand=True)
        self.img_canvas.bind("<Configure>", self._on_canvas_resize)

        # Audio transport (shown for WAV / stream previews)
        self.audio_bar = ttk.Frame(img_frame)
        self.audio_bar.pack(side=tk.BOTTOM, fill=tk.X, pady=(4, 0))
        self._audio_play_btn = ttk.Button(
            self.audio_bar, text="▶ Play", command=self._audio_play, width=10
        )
        self._audio_play_btn.pack(side=tk.LEFT, padx=(0, 4))
        self._audio_stop_btn = ttk.Button(
            self.audio_bar, text="■ Stop", command=self._audio_stop, width=10
        )
        self._audio_stop_btn.pack(side=tk.LEFT, padx=(0, 8))
        self._audio_time_var = tk.StringVar(value="0:00 / 0:00")
        ttk.Label(self.audio_bar, textvariable=self._audio_time_var, width=14).pack(
            side=tk.LEFT, padx=(0, 8)
        )
        self._audio_pos_var = tk.DoubleVar(value=0.0)
        self._audio_scale = ttk.Scale(
            self.audio_bar,
            from_=0.0,
            to=1.0,
            orient=tk.HORIZONTAL,
            variable=self._audio_pos_var,
            command=self._on_audio_seek_drag,
        )
        self._audio_scale.pack(side=tk.LEFT, fill=tk.X, expand=True, padx=(0, 8))
        self._audio_scale.bind("<ButtonPress-1>", self._on_audio_seek_press)
        self._audio_scale.bind("<ButtonRelease-1>", self._on_audio_seek_release)
        player = (
            self._audio_player_cmd[0]
            if self._audio_player_cmd
            else "no player (install ffmpeg/ffplay)"
        )
        self._audio_backend_lbl = ttk.Label(
            self.audio_bar, text=player, foreground="#888"
        )
        self._audio_backend_lbl.pack(side=tk.RIGHT)
        self._set_audio_controls_enabled(False)

        # Txt/hex (decoded / inflated view) and Raw (on-disc bytes)
        self.text_deflated = self._make_scrolled_text(
            self.preview_nb, "Txt / hex"
        )
        self.text_raw = self._make_scrolled_text(self.preview_nb, "Raw")

        # Status
        ttk.Label(self, textvariable=self._status_var, anchor=tk.W, padding=(8, 2)).pack(
            side=tk.BOTTOM, fill=tk.X
        )

        self._current_pil = None
        self._current_item: AssetItem | None = None

    def _make_scrolled_text(self, notebook: ttk.Notebook, title: str) -> tk.Text:
        frame = ttk.Frame(notebook)
        notebook.add(frame, text=title)
        mono = ("Menlo", 11) if sys.platform == "darwin" else ("Consolas", 10)
        text = tk.Text(
            frame,
            wrap=tk.NONE,
            font=mono,
            bg="#1e1e1e",
            fg="#d4d4d4",
            insertbackground="#d4d4d4",
            relief=tk.FLAT,
        )
        sy = ttk.Scrollbar(frame, orient=tk.VERTICAL, command=text.yview)
        sx = ttk.Scrollbar(frame, orient=tk.HORIZONTAL, command=text.xview)
        text.configure(yscrollcommand=sy.set, xscrollcommand=sx.set)
        text.grid(row=0, column=0, sticky="nsew")
        sy.grid(row=0, column=1, sticky="ns")
        sx.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        return text

    @staticmethod
    def _set_text_widget(widget: tk.Text, content: str) -> None:
        widget.configure(state=tk.NORMAL)
        widget.delete("1.0", tk.END)
        widget.insert(tk.END, content)
        widget.configure(state=tk.DISABLED)

    def _clear_text_tabs(self, msg: str = "") -> None:
        self._set_text_widget(self.text_deflated, msg)
        self._set_text_widget(self.text_raw, msg if msg else "")

    # -------------------------------------------------------------- data load
    def _pick_root(self) -> None:
        path = filedialog.askdirectory(
            initialdir=str(self.assets_root), title="Select assets root"
        )
        if path:
            self.assets_root = Path(path).resolve()
            self.root_label.configure(text=str(self.assets_root))
            self.reload()

    def reload(self) -> None:
        self.stages = _load_stages(self.assets_root)
        self._img_clut_index = build_image_clut_index(self.assets_root, self.stages)
        self._img_canon_index = build_image_canon_index(self.stages)
        self._item_map.clear()
        self._populate_type_tree()
        self._populate_stage_tree()
        mode = "full" if self.stages else "raw/minimal"
        n_types = sum(
            1
            for t in TYPE_DIRS
            if (self.assets_root / t).is_dir()
            or (self.assets_root / "raw" / t).is_dir()
        )
        n_links = sum(len(v) for v in self._img_clut_index.values())
        self._status_var.set(
            f"{self.assets_root}  ·  {mode} extract  ·  {n_types} type dirs  ·  "
            f"{len(self._img_clut_index)} images with sibling CLUTs ({n_links} links)"
        )
        self._clear_preview("Select an asset")

    def _populate_type_tree(self) -> None:
        tree = self.type_tree
        tree.delete(*tree.get_children())
        # Drop stale asset iids for this tree
        self._item_map = {
            k: v for k, v in self._item_map.items() if not k.startswith("asset:")
        }
        self._type_items = scan_type_assets(self.assets_root)
        filt = self._filter_var.get().strip().lower()

        for type_dir in TYPE_DIRS:
            items = self._type_items.get(type_dir, [])
            if filt:
                items = [i for i in items if filt in i.label.lower()]
            if not items and not (
                (self.assets_root / type_dir).is_dir()
                or (self.assets_root / "raw" / type_dir).is_dir()
            ):
                continue
            # Lazy: placeholder child so the type node is expandable
            parent = tree.insert(
                "",
                tk.END,
                iid=f"type:{type_dir}",
                text=f"{type_dir}  ({len(items)})",
                values=("",),
                open=False,
            )
            if items:
                tree.insert(parent, tk.END, iid=f"typeph:{type_dir}", text="…")

    def _on_type_open(self, event: tk.Event) -> None:
        tree = self.type_tree
        iid = tree.focus()
        if not iid.startswith("type:") or iid.startswith("typeph:"):
            return
        type_dir = iid.split(":", 1)[1]
        # Already populated?
        kids = tree.get_children(iid)
        if kids and not str(kids[0]).startswith("typeph:"):
            return
        for kid in kids:
            tree.delete(kid)
        items = self._type_items.get(type_dir, [])
        filt = self._filter_var.get().strip().lower()
        if filt:
            items = [i for i in items if filt in i.label.lower()]
        for item in items:
            path = item.path
            if type_dir == "pe2img":
                pe2 = pe2img_path_for_preview(self.assets_root, path)
                if pe2 is not None:
                    path = pe2
                cluts = self._lookup_cluts(path)
                item = AssetItem(
                    path=path,
                    label=item.label,
                    kind=item.kind,
                    source=item.source,
                    meta=item.meta,
                    sibling_cluts=cluts or None,
                )
            aid = f"asset:{item.source}:{item.path}"
            self._item_map[aid] = item
            try:
                size = _fmt_size(item.path.stat().st_size)
            except OSError:
                size = "?"
            tree.insert(iid, tk.END, iid=aid, text=item.label, values=(size,))

    def _lookup_cluts(self, pe2img_path: Path) -> list[Path]:
        keys = [
            str(pe2img_path.resolve()),
            pe2img_path.name,
            pe2img_path.stem,
        ]
        for k in keys:
            if k in self._img_clut_index:
                return list(self._img_clut_index[k])
        return []

    def _on_filter_key(self, _event: tk.Event | None = None) -> None:
        if self._filter_after is not None:
            self.after_cancel(self._filter_after)
        self._filter_after = self.after(250, self._apply_filter)

    def _on_stage_open(self, _event: tk.Event) -> None:
        # Stage tree is fully built; nothing lazy yet.
        pass

    def _populate_stage_tree(self) -> None:
        tree = self.stage_tree
        tree.delete(*tree.get_children())
        if not self.stages:
            tree.insert(
                "",
                tk.END,
                text="(no stages.json — run full extract)",
                values=("",),
            )
            # Fall back: list stage*/ sidecar folders if present
            for stage_dir in sorted(
                self.assets_root.glob("stage*"), key=asset_name_key
            ):
                if stage_dir.is_dir() and stage_dir.name.startswith("stage"):
                    tree.insert(
                        "",
                        tk.END,
                        text=f"{stage_dir.name}/  [sidecars only]",
                        values=("sidecar",),
                    )
            return

        filt = self._filter_var.get().strip().lower()

        for stage_name in sorted(self.stages.keys(), key=asset_name_key):
            stage = self.stages[stage_name]
            if not isinstance(stage, dict):
                continue
            stage_id = f"stage:{stage_name}"
            stage_node = tree.insert(
                "", tk.END, iid=stage_id, text=stage_name, values=("",), open=False
            )

            if "files" in stage and isinstance(stage["files"], dict):
                self._add_files_to_stage_tree(
                    tree, stage_node, stage["files"], stage_name, None, filt
                )
            if "folders" in stage and isinstance(stage["folders"], dict):
                for folder_name, files in sorted(
                    stage["folders"].items(), key=lambda kv: asset_name_key(kv[0])
                ):
                    if not isinstance(files, dict):
                        continue
                    fid = f"folder:{stage_name}/{folder_name}"
                    folder_node = tree.insert(
                        stage_node,
                        tk.END,
                        iid=fid,
                        text=str(folder_name),
                        values=("folder",),
                        open=False,
                    )
                    self._add_files_to_stage_tree(
                        tree,
                        folder_node,
                        files,
                        stage_name,
                        str(folder_name),
                        filt,
                    )

    def _add_files_to_stage_tree(
        self,
        tree: ttk.Treeview,
        parent: str,
        files: dict[str, Any],
        stage_name: str,
        folder: str | None,
        filt: str,
    ) -> None:
        for file_name, chunks in sorted(
            files.items(), key=lambda kv: asset_name_key(kv[0])
        ):
            if not isinstance(chunks, dict):
                continue
            prefix = f"{stage_name}/{folder + '/' if folder else ''}{file_name}"
            if filt and filt not in prefix.lower() and not any(
                filt in str(k).lower() or filt in str(v.get("path", "")).lower()
                for k, v in chunks.items()
                if isinstance(v, dict)
            ):
                continue
            file_id = f"file:{prefix}"
            file_node = tree.insert(
                parent,
                tk.END,
                iid=file_id,
                text=str(file_name),
                values=("file",),
                open=False,
            )
            for chunk_key, ent in sorted(
                chunks.items(), key=lambda kv: asset_name_key(kv[0])
            ):
                if not isinstance(ent, dict):
                    continue
                rel = ent.get("path") or ""
                path = _resolve_on_disk(self.assets_root, rel, ent.get("type"))
                iid = f"chunk:{prefix}/{chunk_key}"
                item = None
                if path is not None and path.exists():
                    # Sibling CLUTs from this same stage file
                    sibs: list[Path] = []
                    if ent.get("type") == "image":
                        for okey, oent in chunks.items():
                            if (
                                isinstance(oent, dict)
                                and oent.get("type") == "clut"
                            ):
                                cp = _resolve_on_disk(
                                    self.assets_root,
                                    oent.get("path") or "",
                                    "clut",
                                )
                                if cp is not None:
                                    cp = prefer_raw_blob(self.assets_root, cp)
                                if (
                                    cp is not None
                                    and cp.exists()
                                    and cp.suffix.lower() == ".pe2clut"
                                ):
                                    sibs.append(cp)
                    item = AssetItem(
                        path=path,
                        label=f"{chunk_key} → {rel}",
                        kind=str(ent.get("type") or ""),
                        source="stage",
                        meta=ent,
                        sibling_cluts=sibs or None,
                    )
                    self._item_map[iid] = item
                info = ent.get("type") or ""
                if ent.get("load_addr"):
                    info = f"{info}  {ent['load_addr']}"
                tree.insert(
                    file_node,
                    tk.END,
                    iid=iid,
                    text=str(chunk_key) + ("" if item else "  (missing)"),
                    values=(info,),
                )

    def _apply_filter(self) -> None:
        # Rebuild trees with filter (simple approach)
        self._populate_type_tree()
        self._populate_stage_tree()

    # -------------------------------------------------------------- selection
    def _on_select(self, tree: ttk.Treeview) -> None:
        sel = tree.selection()
        if not sel:
            return
        iid = sel[0]
        item = self._item_map.get(iid)
        if item is None:
            self._clear_preview(tree.item(iid, "text"))
            return
        self._show_item(item)

    def _clear_preview(self, msg: str = "") -> None:
        self._audio_stop()
        self._set_audio_controls_enabled(False)
        self._current_item = None
        self._current_pil = None
        self._photo_ref = None
        self._pe2img_path = None
        self._pe2img_data = None
        self._clut_choices = []
        self._set_clut_controls_enabled(False)
        self.img_canvas.delete("all")
        self._clear_text_tabs(msg)
        self.info_label.configure(text="")

    def _on_close(self) -> None:
        self._audio_stop()
        self.destroy()

    def _set_clut_controls_enabled(self, enabled: bool) -> None:
        if enabled:
            self._clut_combo.configure(state="readonly")
            self._clut_apply_cb.configure(state=tk.NORMAL)
            self._bpp_combo.configure(state="readonly")
        else:
            self._clut_combo.configure(state="disabled", values=[])
            self._clut_var.set("(none)")
            self._clut_apply_cb.configure(state=tk.DISABLED)
            self._clut_choices = []
            self._current_clut_colors = None
            self._bpp_combo.configure(state="disabled")
            self._bpp_var.set("Auto")

    def _show_item(self, item: AssetItem) -> None:
        # Stop previous stream when switching assets (new audio re-enables).
        self._audio_stop()
        self._set_audio_controls_enabled(False)
        self._media_is_video = False

        self._current_item = item
        self._current_clut_colors = None
        path = item.path
        # Do not load multi‑MB movies into RAM for hex preview.
        heavy = path.suffix.lower() in (".mp4", ".mkv", ".avi", ".webm", ".mov")
        try:
            if heavy:
                data = b""
            else:
                data = path.read_bytes()
        except OSError as e:
            self._clear_preview(f"Cannot read {path}: {e}")
            return

        try:
            size = _fmt_size(path.stat().st_size if heavy else len(data))
        except OSError:
            size = _fmt_size(len(data))
        rel = path
        try:
            rel = path.relative_to(self.assets_root)
        except ValueError:
            pass
        meta_bits = [f"{rel}", size, item.source]
        if item.meta:
            if item.meta.get("chunk_size"):
                meta_bits.append(f"chunk_size={item.meta['chunk_size']}")
            if item.meta.get("load_addr"):
                meta_bits.append(f"load={item.meta['load_addr']}")
            if item.meta.get("duration_sec") is not None and item.kind == "audio":
                meta_bits.append(f"{item.meta['duration_sec']}s")
            if item.kind == "movie" and item.meta.get("frame_count") is not None:
                meta_bits.append(f"{item.meta['frame_count']} frames")
        self.info_label.configure(text="  ·  ".join(meta_bits))
        self._status_var.set(str(path))

        suf = path.suffix.lower()
        kind = item.kind

        # --- Fast paths first (images) — avoid heavy pairing until needed ---
        pe2img = pe2img_path_for_preview(self.assets_root, path)
        if pe2img is not None or suf == ".pe2img" or kind == "image":
            src = pe2img or path
            try:
                img_data = data if src == path else src.read_bytes()
            except OSError as e:
                self._clear_preview(f"Cannot read {src}: {e}")
                return
            raw_img = (
                src
                if src.suffix.lower() == ".pe2img"
                else self.assets_root / "raw" / "pe2img" / f"{src.stem}.pe2img"
            )
            if not item.sibling_cluts:
                item = AssetItem(
                    path=item.path,
                    label=item.label,
                    kind=item.kind,
                    source=item.source,
                    meta=item.meta,
                    sibling_cluts=self._lookup_cluts(src) or None,
                )
                self._current_item = item
            if self._show_pe2img(src, img_data, item=item):
                self.preview_nb.select(0)
                # Lightweight text tabs (short hex only — selection stays snappy)
                raw_bytes = img_data
                if raw_img.exists() and raw_img != src:
                    try:
                        raw_bytes = raw_img.read_bytes()
                    except OSError:
                        pass
                self._fill_binary_pair(
                    defl_data=img_data,
                    raw_data=raw_bytes,
                    defl_path=src,
                    raw_path=raw_img if raw_img.exists() else src,
                    image_note=True,
                    defl_label="pe2img clean payload",
                    short=True,
                )
                return

        if suf == ".pe2clut" or kind == "clut":
            self._set_clut_controls_enabled(False)
            if self._show_pe2clut(path, data):
                self.preview_nb.select(0)
                self._fill_binary_pair(
                    defl_data=data,
                    raw_data=data,
                    defl_path=path,
                    raw_path=path,
                    image_note=True,
                    defl_label="pe2clut payload",
                    short=True,
                )
                return

        if suf == ".bs" or kind in ("bs", "room_background"):
            self._set_clut_controls_enabled(False)
            if self._show_bs(path, data):
                self.preview_nb.select(0)
                raw_bs = path
                if path.suffix.lower() == ".png":
                    cand = self.assets_root / "raw" / "bs" / f"{path.stem}.bs"
                    if cand.exists():
                        raw_bs = cand
                elif "raw" not in path.parts:
                    cand = self.assets_root / "raw" / "bs" / f"{path.stem}.bs"
                    if cand.exists():
                        raw_bs = cand
                try:
                    raw_bytes = (
                        data if raw_bs == path else raw_bs.read_bytes()
                    )
                except OSError:
                    raw_bytes = data
                self._fill_binary_pair(
                    defl_data=data if suf == ".png" else raw_bytes,
                    raw_data=raw_bytes,
                    defl_path=path,
                    raw_path=raw_bs,
                    image_note=True,
                    defl_label="BS v2 MDEC frame",
                    short=True,
                )
                return

        if suf == ".png" and kind not in ("image", "bs", "room_background"):
            self._set_clut_controls_enabled(False)
            if self._show_png(path):
                self.preview_nb.select(0)
                self._fill_binary_pair(
                    defl_data=data,
                    raw_data=data,
                    defl_path=path,
                    raw_path=path,
                    image_note=True,
                    defl_label="PNG",
                    short=True,
                )
                return

        if (
            suf == ".spk"
            or kind in ("spk", "music")
            or (path.name == "meta.json" and "spk" in path.parts)
        ):
            self._set_clut_controls_enabled(False)
            if self._show_spk(path, data):
                return

        if (
            suf in (".wav", ".mts")
            or kind == "audio"
            or "raw/audio" in str(path).replace("\\", "/")
            or (
                path.parent.name == "audio"
                and path.parent.parent == self.assets_root
            )
        ):
            self._set_clut_controls_enabled(False)
            if self._show_audio_stream(path, data, item=item):
                return

        if (
            kind == "movie"
            or suf in (".str", ".webp", ".mp4", ".mkv")
            or (path.name == "meta.json" and "movie" in path.parts)
            or (path.suffix.lower() == ".json" and path.parent.name == "movie")
            or "raw/movie" in str(path).replace("\\", "/")
        ):
            self._set_clut_controls_enabled(False)
            if self._show_movie(path, data, item=item):
                return

        # Pair raw/deflated only for non-image types (can read a second file)
        raw_path, raw_data, defl_path, defl_data = self._pair_raw_deflated(
            path, data, kind=kind
        )

        if suf == ".txt" or kind in ("txt", "ascii"):
            self._fill_ascii_tabs(raw_data, defl_data, raw_path, defl_path)
            self.preview_nb.select(1)  # Txt / hex
            self._set_clut_controls_enabled(False)
            return

        if suf in TEXT_EXTS:
            self._fill_text_tabs(raw_data, defl_data, raw_path, defl_path)
            self.preview_nb.select(1)
            self._set_clut_controls_enabled(False)
            return

        self._set_clut_controls_enabled(False)
        if suf == ".pe2pkg" or kind in ("pe2pkg", "room_pkg"):
            self._fill_pe2pkg_tabs(raw_path, raw_data, defl_path, defl_data)
            self.preview_nb.select(1)
            return

        self._fill_binary_pair(
            defl_data=defl_data,
            raw_data=raw_data,
            defl_path=defl_path,
            raw_path=raw_path,
            short=True,
        )
        self.preview_nb.select(1)

    def _pair_raw_deflated(
        self, path: Path, data: bytes, *, kind: str
    ) -> tuple[Path, bytes, Path, bytes]:
        """Return (raw_path, raw_bytes, deflated_path, deflated_bytes)."""
        # Default: selected file is both
        raw_path, raw_data = path, data
        defl_path, defl_data = path, data

        # If under inflated type dir, pair with raw/
        try:
            rel = path.relative_to(self.assets_root)
        except ValueError:
            return raw_path, raw_data, defl_path, defl_data

        parts = rel.parts
        if parts and parts[0] == "raw" and len(parts) >= 3:
            # Viewing raw — try inflate counterpart
            type_dir, name = parts[1], parts[-1]
            infl = self.assets_root / type_dir / name
            # png for images
            if not infl.exists() and Path(name).suffix in (
                ".pe2img",
                ".pe2clut",
                ".bs",
            ):
                infl = self.assets_root / type_dir / f"{Path(name).stem}.png"
            if not infl.exists() and Path(name).suffix == ".pe2pkg":
                # inflated pe2pkg keeps same name
                infl = self.assets_root / type_dir / name
            raw_path, raw_data = path, data
            if infl.exists():
                try:
                    defl_path, defl_data = infl, infl.read_bytes()
                except OSError:
                    defl_path, defl_data = path, data
            elif Path(name).suffix == ".pe2pkg":
                try:
                    from asset_decode import decode_pe2pkg_payload

                    defl_data = decode_pe2pkg_payload(data)
                    defl_path = path  # virtual
                except Exception:
                    defl_data = data
            elif Path(name).suffix == ".txt":
                try:
                    from asset_decode import decode_ascii_payload

                    defl_data = decode_ascii_payload(data)
                except Exception:
                    end = len(data)
                    while end > 0 and data[end - 1] == 0:
                        end -= 1
                    defl_data = data[:end]
            else:
                defl_path, defl_data = path, data
            return raw_path, raw_data, defl_path, defl_data

        if parts and parts[0] in TYPE_DIRS:
            # Viewing inflated — find raw
            type_dir = parts[0]
            stem = path.stem
            ext_candidates = [
                path.suffix,
                ".pe2img",
                ".pe2clut",
                ".pe2pkg",
                ".txt",
                ".spk",
                ".bs",
                ".pe2cap2",
            ]
            defl_path, defl_data = path, data
            for ext in ext_candidates:
                cand = self.assets_root / "raw" / type_dir / f"{stem}{ext}"
                if cand.exists():
                    try:
                        raw_path, raw_data = cand, cand.read_bytes()
                    except OSError:
                        pass
                    break
            # txt inflated is already stripped; raw has pad
            if path.suffix == ".txt" or kind in ("txt", "ascii"):
                try:
                    from asset_decode import decode_ascii_payload

                    defl_data = decode_ascii_payload(
                        raw_data if raw_path != path else data
                    )
                except Exception:
                    defl_data = data
            return raw_path, raw_data, defl_path, defl_data

        return raw_path, raw_data, defl_path, defl_data

    def _fill_ascii_tabs(
        self,
        raw_data: bytes,
        defl_data: bytes,
        raw_path: Path,
        defl_path: Path,
    ) -> None:
        try:
            from asset_decode import decode_ascii_payload

            body = decode_ascii_payload(raw_data)
        except Exception:
            body = defl_data
            end = len(body)
            while end > 0 and body[end - 1] == 0:
                end -= 1
            body = body[:end]
        try:
            text = body.decode("utf-8")
        except UnicodeDecodeError:
            text = body.decode("latin-1", errors="replace")
        if len(text) > PREVIEW_TEXT_CHARS:
            text = text[:PREVIEW_TEXT_CHARS] + "\n… (truncated)"
        stripped = len(raw_data) - len(body)
        header = (
            f"# deflated (inflated form)  {defl_path.name}\n"
            f"# {len(body)} bytes text"
            + (f", stripped {stripped} trailing zeros from raw\n\n" if stripped else "\n\n")
        )
        self._set_text_widget(self.text_deflated, header + text)

        raw_header = (
            f"# raw on-disc clean payload  {raw_path.name}\n"
            f"# size {len(raw_data)} ({_fmt_size(len(raw_data))})\n\n"
        )
        self._set_text_widget(
            self.text_raw,
            raw_header + _hexdump(raw_data, min(len(raw_data), PREVIEW_HEX_BYTES * 4)),
        )

    def _fill_text_tabs(
        self,
        raw_data: bytes,
        defl_data: bytes,
        raw_path: Path,
        defl_path: Path,
    ) -> None:
        def as_text(b: bytes) -> str:
            try:
                t = b.decode("utf-8")
            except UnicodeDecodeError:
                t = b.decode("latin-1", errors="replace")
            if len(t) > PREVIEW_TEXT_CHARS:
                t = t[:PREVIEW_TEXT_CHARS] + "\n… (truncated)"
            return t

        self._set_text_widget(
            self.text_deflated,
            f"# {defl_path}\n# {_fmt_size(len(defl_data))}\n\n{as_text(defl_data)}",
        )
        self._set_text_widget(
            self.text_raw,
            f"# {raw_path}\n# {_fmt_size(len(raw_data))}\n\n{as_text(raw_data)}",
        )

    def _fill_pe2pkg_tabs(
        self,
        raw_path: Path,
        raw_data: bytes,
        defl_path: Path,
        defl_data: bytes,
    ) -> None:
        # If defl still looks compressed (same as raw), try decode
        body = defl_data
        if defl_path == raw_path or defl_data == raw_data:
            try:
                from asset_decode import decode_pe2pkg_payload

                body = decode_pe2pkg_payload(raw_data)
            except Exception:
                body = defl_data
        defl_txt = (
            f"# deflated (LZSS-decoded)  {defl_path.name}\n"
            f"# size {len(body)} ({_fmt_size(len(body))})\n\n"
            f"strings:\n"
            + "\n".join(f"  {s[:120]}" for s in _printable_strings(body)[:40])
            + "\n\nhex:\n"
            + _hexdump(body)
        )
        raw_txt = (
            f"# raw LZSS stream  {raw_path.name}\n"
            f"# size {len(raw_data)} ({_fmt_size(len(raw_data))})\n\n"
            f"hex:\n"
            + _hexdump(raw_data)
        )
        self._set_text_widget(self.text_deflated, defl_txt)
        self._set_text_widget(self.text_raw, raw_txt)

    def _fill_binary_pair(
        self,
        *,
        defl_data: bytes,
        raw_data: bytes,
        defl_path: Path,
        raw_path: Path,
        image_note: bool = False,
        defl_label: str = "txt / hex (decoded view)",
        short: bool = False,
    ) -> None:
        hex_limit = 256 if short else PREVIEW_HEX_BYTES
        str_limit = 12 if short else 30

        def summary(data: bytes, path: Path, label: str) -> str:
            lines = [
                f"# {label}",
                f"# path: {path}",
                f"# size: {len(data)} ({_fmt_size(len(data))})",
            ]
            if image_note:
                lines.append("# (see Image tab for raster preview)")
            lines.append("")
            if not short:
                strings = _printable_strings(data, limit=str_limit)
                if strings:
                    lines.append("strings:")
                    for s in strings:
                        lines.append(f"  {s[:120]}")
                    lines.append("")
            lines.append("hex:")
            lines.append(_hexdump(data, hex_limit))
            return "\n".join(lines)

        self._set_text_widget(
            self.text_deflated, summary(defl_data, defl_path, defl_label)
        )
        self._set_text_widget(
            self.text_raw, summary(raw_data, raw_path, "raw on-disc")
        )

    def _show_png(self, path: Path) -> bool:
        try:
            from PIL import Image
        except ImportError:
            self._show_text_message("Pillow not installed — cannot preview PNG")
            return False
        try:
            img = Image.open(path)
            img.load()
        except Exception as e:
            self._show_text_message(f"PNG open failed: {e}")
            return False
        self._pe2img_path = None
        self._pe2img_data = None
        self._set_pil_image(img)
        return True

    def _show_bs(self, path: Path, data: bytes) -> bool:
        """Preview a BS v2 frame (raw .bs or already-inflated PNG)."""
        self._pe2img_path = None
        self._pe2img_data = None
        if path.suffix.lower() == ".png":
            return self._show_png(path)
        try:
            from asset_decode import render_bs
        except ImportError as e:
            self._show_text_message(f"BS decode unavailable: {e}")
            return False
        try:
            # Prefer raw blob if we were pointed at a non-raw path
            blob = data
            if path.suffix.lower() != ".bs" or "raw" not in path.parts:
                cand = self.assets_root / "raw" / "bs" / f"{path.stem}.bs"
                if cand.exists():
                    blob = cand.read_bytes()
            img, info = render_bs(blob)
            self._set_pil_image(img)
            self.info_label.configure(
                text=(
                    f"{self.info_label.cget('text')}  ·  "
                    f"BS v2 {info.width}×{info.height} q={info.quant_scale} "
                    f"mbs={info.macroblocks}"
                )
            )
            return True
        except Exception as e:
            self._show_text_message(f"BS decode failed: {e}")
            return False

    # -------------------------------------------------------------- audio
    @staticmethod
    def _fmt_time(sec: float) -> str:
        if sec < 0:
            sec = 0.0
        m = int(sec) // 60
        s = int(sec) % 60
        return f"{m}:{s:02d}"

    def _set_audio_controls_enabled(self, enabled: bool) -> None:
        state = tk.NORMAL if enabled else tk.DISABLED
        self._audio_play_btn.configure(state=state)
        self._audio_stop_btn.configure(state=state)
        self._audio_scale.configure(state=state)
        if enabled and self._media_is_video:
            self._audio_play_btn.configure(text="▶ Play movie")
        else:
            self._audio_play_btn.configure(text="▶ Play")
        if not enabled:
            self._audio_pos_var.set(0.0)
            self._audio_time_var.set("0:00 / 0:00")
            self._audio_duration = 0.0
            self._audio_path = None
            self._media_is_video = False

    def _audio_stop(self) -> None:
        if self._audio_tick_after is not None:
            try:
                self.after_cancel(self._audio_tick_after)
            except Exception:
                pass
            self._audio_tick_after = None
        if self._audio_proc is not None:
            try:
                self._audio_proc.terminate()
                self._audio_proc.wait(timeout=1.0)
            except Exception:
                try:
                    self._audio_proc.kill()
                except Exception:
                    pass
            self._audio_proc = None
        # Keep path/duration so Play can restart; only reset position display
        if self._audio_duration > 0:
            pos = self._audio_pos_var.get()
            self._audio_time_var.set(
                f"{self._fmt_time(pos)} / {self._fmt_time(self._audio_duration)}"
            )

    def _audio_play(self) -> None:
        if self._audio_path is None or not self._audio_path.is_file():
            return
        is_video = self._media_is_video or self._audio_path.suffix.lower() in (
            ".webp",
            ".webm",
            ".mp4",
            ".avi",
            ".mkv",
            ".mov",
        )
        if is_video:
            player = self._video_player_cmd or self._audio_player_cmd
            if player is None:
                self._status_var.set(
                    "No video player found (install ffmpeg/ffplay or mpv/vlc)"
                )
                return
            cmd = list(player)
        else:
            if self._audio_player_cmd is None:
                self._status_var.set(
                    "No audio player found (install ffmpeg for ffplay)"
                )
                return
            cmd = list(self._audio_player_cmd)

        start = float(self._audio_pos_var.get() or 0.0)
        if start >= self._audio_duration - 0.05:
            start = 0.0
            self._audio_pos_var.set(0.0)
        self._audio_stop()

        if cmd[0] == "ffplay" and start > 0.05:
            cmd.extend(["-ss", f"{start:.3f}"])
        elif cmd[0] == "mpv" and start > 0.05:
            cmd.extend([f"--start={start:.3f}"])
        elif cmd[0] == "afplay" and start > 0.05:
            start = 0.0
            self._audio_pos_var.set(0.0)
        cmd.append(str(self._audio_path))
        try:
            self._audio_proc = subprocess.Popen(
                cmd,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
        except OSError as e:
            self._status_var.set(f"Play failed: {e}")
            self._audio_proc = None
            return
        self._audio_started_at = time.monotonic()
        self._audio_start_offset = start
        kind = "movie" if is_video else "audio"
        self._status_var.set(f"Playing {kind}: {self._audio_path.name}")
        self._audio_tick()

    def _audio_position(self) -> float:
        if self._audio_proc is None:
            return float(self._audio_pos_var.get() or 0.0)
        elapsed = time.monotonic() - self._audio_started_at
        return min(self._audio_duration, self._audio_start_offset + elapsed)

    def _audio_tick(self) -> None:
        self._audio_tick_after = None
        if self._audio_proc is None:
            return
        # Process exited?
        if self._audio_proc.poll() is not None:
            self._audio_proc = None
            self._audio_pos_var.set(0.0)
            self._audio_time_var.set(
                f"0:00 / {self._fmt_time(self._audio_duration)}"
            )
            return
        if not self._audio_seeking:
            pos = self._audio_position()
            self._audio_pos_var.set(pos)
            self._audio_time_var.set(
                f"{self._fmt_time(pos)} / {self._fmt_time(self._audio_duration)}"
            )
            if pos >= self._audio_duration - 0.05:
                self._audio_stop()
                self._audio_pos_var.set(0.0)
                self._audio_time_var.set(
                    f"0:00 / {self._fmt_time(self._audio_duration)}"
                )
                return
        self._audio_tick_after = self.after(200, self._audio_tick)

    def _on_audio_seek_press(self, _event: tk.Event | None = None) -> None:
        self._audio_seeking = True

    def _on_audio_seek_drag(self, _value: str = "") -> None:
        if self._audio_duration <= 0:
            return
        pos = float(self._audio_pos_var.get() or 0.0)
        self._audio_time_var.set(
            f"{self._fmt_time(pos)} / {self._fmt_time(self._audio_duration)}"
        )

    def _on_audio_seek_release(self, _event: tk.Event | None = None) -> None:
        self._audio_seeking = False
        was_playing = self._audio_proc is not None
        if was_playing:
            self._audio_play()  # restart from new position

    def _draw_waveform(self, pcm: list[int], *, title: str = "") -> bool:
        """Draw mono PCM waveform into the image canvas via PIL."""
        if not pcm:
            return False
        try:
            from PIL import Image as PILImage
        except ImportError:
            return False
        w, h = 720, 180
        img = PILImage.new("RGB", (w, h), (20, 20, 28))
        step = max(1, len(pcm) // w)
        mid = h // 2
        px = img.load()
        # centre line
        for x in range(w):
            px[x, mid] = (40, 40, 52)
        for x in range(w):
            i0 = x * step
            chunk = pcm[i0 : i0 + step] or [0]
            peak = max(abs(v) for v in chunk)
            amp = int(peak / 32768.0 * (mid - 4))
            for y in range(mid - amp, mid + amp + 1):
                if 0 <= y < h:
                    px[x, y] = (80, 200, 140)
        self._set_pil_image(img)
        return True

    def _show_audio_stream(
        self, path: Path, data: bytes, *, item: AssetItem | None = None
    ) -> bool:
        """Preview MTS CD stream / WAV: meta, waveform, playable transport."""
        self._pe2img_path = None
        self._pe2img_data = None
        self._audio_stop()

        stem = path.stem
        wav_path: Path | None = None
        mts_path: Path | None = None
        meta: dict[str, Any] = dict(item.meta) if item and item.meta else {}

        if path.suffix.lower() == ".wav":
            wav_path = path
            cand_mts = self.assets_root / "raw" / "audio" / f"{stem}.mts"
            if cand_mts.is_file():
                mts_path = cand_mts
            meta_file = self.assets_root / "audio" / f"{stem}.json"
            if meta_file.is_file() and not meta:
                try:
                    meta = json.loads(meta_file.read_text(encoding="utf-8"))
                except (OSError, json.JSONDecodeError):
                    pass
        elif path.suffix.lower() == ".mts":
            mts_path = path
            cand_wav = self.assets_root / "audio" / f"{stem}.wav"
            if cand_wav.is_file():
                wav_path = cand_wav
            meta_file = self.assets_root / "audio" / f"{stem}.json"
            if meta_file.is_file():
                try:
                    meta = json.loads(meta_file.read_text(encoding="utf-8"))
                except (OSError, json.JSONDecodeError):
                    pass
        else:
            # Directory or other — try stem.wav
            cand = self.assets_root / "audio" / f"{stem}.wav"
            if cand.is_file():
                wav_path = cand

        lines = [
            f"CD audio stream  {stem}",
            f"format=MTS → SPU-ADPCM → WAV @ ~22050 Hz",
        ]
        if meta:
            for key in (
                "duration_sec",
                "channels",
                "period",
                "chunk_count",
                "body_sectors",
                "preamble_sectors",
                "sample_rate",
                "disk",
                "stage",
                "folder_id",
                "stream_id",
            ):
                if key in meta:
                    lines.append(f"  {key}={meta[key]}")
            desc = meta.get("descriptor")
            if isinstance(desc, dict):
                lines.append("descriptor:")
                for k, v in desc.items():
                    lines.append(f"  {k}: {v}")
        if wav_path and wav_path.is_file():
            try:
                wi = _wav_info(wav_path)
                lines.append("")
                lines.append(
                    f"WAV  {wav_path.name}  "
                    f"{wi['nchannels']}ch  {wi['framerate']} Hz  "
                    f"{wi['nframes']} frames  {wi['duration_sec']:.2f}s  "
                    f"{_fmt_size(wav_path.stat().st_size)}"
                )
            except Exception as e:
                lines.append(f"WAV unreadable: {e}")
                wi = None
        else:
            wi = None
            lines.append("")
            lines.append(
                "No WAV yet — re-run extract.py, or:\n"
                "  python3 tools/peassets/extract_streams.py "
                "--rom rom/USA --out assets/USA"
            )
        if mts_path and mts_path.is_file():
            try:
                lines.append(
                    f"raw MTS  {mts_path.name}  {_fmt_size(mts_path.stat().st_size)}"
                )
            except OSError:
                lines.append(f"raw MTS  {mts_path.name}")

        text = "\n".join(lines)
        self._set_text_widget(self.text_deflated, text)

        raw_bits = []
        if mts_path and mts_path.is_file():
            try:
                mts_bytes = (
                    data if path == mts_path else mts_path.read_bytes()
                )
                raw_bits.append(
                    f"raw {mts_path}\n{len(mts_bytes)} bytes\n\n"
                    + _hexdump(mts_bytes, 256)
                )
            except OSError as e:
                raw_bits.append(f"raw read failed: {e}")
        elif wav_path:
            raw_bits.append(f"(no .mts sibling; showing WAV path)\n{wav_path}")
        self._set_text_widget(
            self.text_raw, "\n".join(raw_bits) if raw_bits else ""
        )

        # Waveform + player
        if wav_path and wav_path.is_file():
            try:
                pcm = _wav_pcm_mono_preview(wav_path)
                if self._draw_waveform(pcm):
                    self.preview_nb.select(0)
                else:
                    self.preview_nb.select(1)
            except Exception as e:
                self._set_text_widget(
                    self.text_deflated, text + f"\n\n(waveform failed: {e})"
                )
                self.preview_nb.select(1)

            dur = wi["duration_sec"] if wi else 0.0
            if dur <= 0 and meta.get("duration_sec"):
                try:
                    dur = float(meta["duration_sec"])
                except (TypeError, ValueError):
                    dur = 0.0
            self._audio_path = wav_path
            self._audio_duration = dur
            self._audio_pos_var.set(0.0)
            self._audio_scale.configure(from_=0.0, to=max(dur, 0.001))
            self._audio_time_var.set(f"0:00 / {self._fmt_time(dur)}")
            self._set_audio_controls_enabled(True)
        else:
            self._set_audio_controls_enabled(False)
            self.preview_nb.select(1)

        try:
            base = self.info_label.cget("text")
        except Exception:
            base = ""
        extra = []
        if wi:
            extra.append(f"{wi['duration_sec']:.1f}s")
            extra.append(f"{wi['nchannels']}ch")
        if meta.get("period") is not None:
            extra.append(f"period={meta['period']}")
        self.info_label.configure(
            text=(f"{base}  ·  audio stream  " + "  ".join(extra)).strip(" ·")
        )
        return True

    def _show_movie(
        self, path: Path, data: bytes, *, item: AssetItem | None = None
    ) -> bool:
        """Preview STR movie MP4: meta, poster frame, Play opens video window."""
        self._pe2img_path = None
        self._pe2img_data = None
        self._audio_stop()
        self._set_audio_controls_enabled(False)

        stem = (
            path.parent.name
            if path.name == "meta.json"
            else path.stem
        )
        movie_root = self.assets_root / "movie"
        movie_dir = movie_root / stem  # legacy frame dump
        meta: dict[str, Any] = dict(item.meta) if item and item.meta else {}
        for meta_path in (movie_root / f"{stem}.json", movie_dir / "meta.json"):
            if meta_path.is_file():
                try:
                    loaded = json.loads(meta_path.read_text(encoding="utf-8"))
                    if not meta:
                        meta = loaded
                    else:
                        # Prefer richer fields from disk
                        for k, v in loaded.items():
                            meta.setdefault(k, v)
                except (OSError, json.JSONDecodeError):
                    pass

        mp4 = movie_root / f"{stem}.mp4"
        if path.suffix.lower() in (".mp4", ".mkv", ".mov"):
            mp4 = path

        lines = [
            f"STR movie  {stem}",
            "PSX STR/MDEC → lossless MP4 (H.264 yuv444p crf0 + ALAC when XA)",
            "Play: ffplay/mpv window (not Windows Media Player)",
        ]
        for key in (
            "frame_count",
            "width",
            "height",
            "version",
            "fps",
            "frame_range",
            "raw_sectors",
            "disk",
            "stage",
            "folder_id",
            "source",
            "container",
            "audio_duration_sec",
            "audio_sample_rate",
        ):
            if key in meta:
                lines.append(f"  {key}={meta[key]}")
        if isinstance(meta.get("mp4_encode"), dict):
            enc = meta["mp4_encode"]
            lines.append(
                f"  encode: v={enc.get('video_codec')} "
                f"pix={enc.get('pix_fmt')} crf={enc.get('crf')} "
                f"a={enc.get('audio_codec')}"
            )
            if enc.get("player_hint"):
                lines.append(f"  note: {enc['player_hint']}")
        desc = meta.get("descriptor")
        if isinstance(desc, dict):
            lines.append("descriptor:")
            for k, v in list(desc.items())[:16]:
                lines.append(f"  {k}: {v}")

        lines.append("")
        if mp4.is_file():
            lines.append(f"MP4: {mp4.name}  ({_fmt_size(mp4.stat().st_size)})")
        else:
            lines.append(
                "No MP4 yet — re-run extract.py, or:\n"
                "  python3 tools/peassets/extract_movies.py "
                "--rom rom/USA --out assets/USA -j 16"
            )
        if isinstance(meta.get("xa"), dict):
            xa = meta["xa"]
            if xa.get("format") == "none":
                lines.append("XA audio: none (CDF/ISO Form 1 only → silent MP4)")
            elif xa.get("audio_sectors"):
                lines.append(
                    f"XA: {xa.get('audio_sectors')} sectors  "
                    f"{xa.get('sample_rate')} Hz  {xa.get('duration_sec')}s"
                )
        raw_str = self.assets_root / "raw" / "movie" / f"{stem}.str"
        if raw_str.is_file():
            lines.append(f"raw STR: {raw_str.name}  ({_fmt_size(raw_str.stat().st_size)})")

        text = "\n".join(lines)
        self._set_text_widget(self.text_deflated, text)

        if raw_str.is_file():
            try:
                raw_bytes = raw_str.read_bytes()[:4096]
                self._set_text_widget(
                    self.text_raw,
                    f"raw {raw_str} (first 4 KiB)\n\n" + _hexdump(raw_bytes, 256),
                )
            except OSError as e:
                self._set_text_widget(self.text_raw, str(e))
        else:
            self._set_text_widget(
                self.text_raw,
                "(no raw STR; MP4 is the primary inflated asset)",
            )

        # Poster: ffmpeg frame from MP4, else legacy PNG dump
        shown = False
        if mp4.is_file():
            poster = _mp4_poster_pil(mp4, at_sec=0.5)
            if poster is not None:
                self._set_pil_image(poster)
                self.preview_nb.select(0)
                shown = True
        if not shown:
            frames = (
                sorted(movie_dir.glob("frame_*.png")) if movie_dir.is_dir() else []
            )
            if frames:
                try:
                    from PIL import Image as PILImage

                    img = PILImage.open(frames[0])
                    self._set_pil_image(img.convert("RGB"))
                    self.preview_nb.select(0)
                    shown = True
                except Exception:
                    shown = False
        if not shown:
            self.preview_nb.select(1)

        # Transport bar → Play opens external video window
        if mp4.is_file() and (self._video_player_cmd or self._audio_player_cmd):
            n = int(meta.get("frame_count") or 0)
            fps = float(meta.get("fps") or 15)
            dur = float(meta.get("audio_duration_sec") or 0)
            if dur <= 0 and fps > 0 and n:
                dur = n / fps
            probed = _probe_media_duration(mp4)
            if probed and probed > 0:
                dur = probed
            self._media_is_video = True
            self._audio_path = mp4
            self._audio_duration = dur if dur > 0 else 1.0
            self._audio_pos_var.set(0.0)
            self._audio_scale.configure(
                from_=0.0, to=max(self._audio_duration, 0.001)
            )
            self._audio_time_var.set(
                f"0:00 / {self._fmt_time(self._audio_duration)}"
            )
            self._set_audio_controls_enabled(True)
            backend = (self._video_player_cmd or self._audio_player_cmd or ["?"])[0]
            self._audio_backend_lbl.configure(text=f"{backend} (video)")
            self._status_var.set(
                f"Movie ready — ▶ Play movie  ({mp4.name})"
            )
        else:
            self._set_audio_controls_enabled(False)
            if not mp4.is_file():
                self._status_var.set(
                    "Movie MP4 missing — re-run extract.py or extract_movies.py"
                )
            else:
                self._status_var.set(
                    "No video player (install ffmpeg for ffplay, or mpv/vlc)"
                )

        try:
            base = self.info_label.cget("text")
        except Exception:
            base = ""
        extra = []
        if meta.get("frame_count") is not None:
            extra.append(f"{meta['frame_count']} frames")
        if meta.get("width") and meta.get("height"):
            extra.append(f"{meta['width']}×{meta['height']}")
        if mp4.is_file():
            extra.append("mp4")
        self.info_label.configure(
            text=(f"{base}  ·  movie  " + "  ".join(extra)).strip(" ·")
        )
        return True

    def _show_spk(self, path: Path, data: bytes) -> bool:
        """Preview SPK bank: metadata text + first sample waveform as PNG."""
        self._pe2img_path = None
        self._pe2img_data = None
        self._audio_stop()
        self._set_audio_controls_enabled(False)
        try:
            from spk_codec import extract_samples, is_spk, parse_spk
        except ImportError as e:
            self._show_text_message(f"SPK decode unavailable: {e}")
            return False

        # Resolve raw .spk blob
        blob = data
        raw_path = path
        if path.suffix.lower() != ".spk" or not is_spk(data):
            stem = path.parent.name if path.name == "meta.json" else path.stem
            cand = self.assets_root / "raw" / "spk" / f"{stem}.spk"
            if cand.exists():
                try:
                    blob = cand.read_bytes()
                    raw_path = cand
                except OSError:
                    pass
            elif path.name == "meta.json":
                # Inflated bank dir — summarise meta + list wavs
                try:
                    import json as _json

                    meta = _json.loads(data.decode("utf-8", errors="replace"))
                except Exception:
                    meta = {}
                wavs = sorted(
                    path.parent.glob("sample_*.wav"), key=asset_name_key
                )
                lines = [
                    f"SPK bank (inflated)  {path.parent.name}",
                    f"bank_id={meta.get('bank_id')}  notes={meta.get('note_count')}  "
                    f"samples={len(wavs)}",
                    "",
                    "Samples:",
                ]
                for w in wavs:
                    lines.append(f"  {w.name}  ({w.stat().st_size} B)")
                self._show_text_message("\n".join(lines))
                self.preview_nb.select(1)
                return True

        if not is_spk(blob):
            self._show_text_message("Not a valid hSPK bank")
            return False

        try:
            info = parse_spk(blob)
            samples = extract_samples(blob, info)
        except Exception as e:
            self._show_text_message(f"SPK parse failed: {e}")
            return False

        lines = [
            f"SPK bank  id=0x{info.bank_id:04X}  type={info.bank_type}",
            f"groups={len(info.groups)}  notes={len(info.notes)}  "
            f"unique_samples={len(samples)}",
            f"prog_size={info.prog_size}  spu_size={info.spu_size}  "
            f"spu_base=0x{info.spu_base:X}",
            f"file={info.original_size} B",
            "",
            "Notes (wave / keys / vol):",
        ]
        for i, n in enumerate(info.notes[:24]):
            lines.append(
                f"  [{i:02d}] wave={n.wave_addr:6d}  keys={n.key_min}-{n.key_max}  "
                f"root={n.root_key}  vol={n.volume}  pan={n.pan}"
            )
        if len(info.notes) > 24:
            lines.append(f"  … +{len(info.notes) - 24} more")
        lines.append("")
        lines.append("Samples (PCM @ 22050 Hz):")
        for i, (addr, pcm) in enumerate(samples[:16]):
            dur = len(pcm) / 22050.0
            peak = max((abs(x) for x in pcm), default=0)
            lines.append(
                f"  [{i:02d}] wave_addr={addr:6d}  frames={len(pcm):6d}  "
                f"{dur:.2f}s  peak={peak}"
            )
        if len(samples) > 16:
            lines.append(f"  … +{len(samples) - 16} more")

        text = "\n".join(lines)
        self._set_text_widget(self.text_deflated, text)
        self._set_text_widget(
            self.text_raw,
            f"raw {raw_path}\n{len(blob)} bytes\n\n" + text[:4000],
        )

        # Waveform of first non-silent sample as preview image
        pcm_show = None
        for _addr, pcm in samples:
            if pcm and max(abs(x) for x in pcm) > 64:
                pcm_show = pcm
                break
        if pcm_show is None and samples:
            pcm_show = samples[0][1]
        if pcm_show:
            try:
                from PIL import Image as PILImage

                w, h = 640, 160
                img = PILImage.new("RGB", (w, h), (20, 20, 28))
                step = max(1, len(pcm_show) // w)
                mid = h // 2
                px = img.load()
                for x in range(w):
                    i0 = x * step
                    chunk = pcm_show[i0 : i0 + step] or [0]
                    peak = max(abs(v) for v in chunk)
                    amp = int(peak / 32768.0 * (mid - 2))
                    for y in range(mid - amp, mid + amp + 1):
                        if 0 <= y < h:
                            px[x, y] = (80, 200, 140)
                self._set_pil_image(img)
                self.preview_nb.select(0)
            except Exception as e:
                self._set_text_widget(
                    self.text_deflated,
                    text + f"\n\n(waveform preview failed: {e})",
                )
                self.preview_nb.select(1)
        else:
            self.preview_nb.select(1)

        try:
            base = self.info_label.cget("text")
        except Exception:
            base = ""
        self.info_label.configure(
            text=(
                f"{base}  ·  "
                f"SPK 0x{info.bank_id:04X}  {len(info.notes)} notes  "
                f"{len(samples)} samples"
            ).strip(" ·")
        )
        return True

    def _sibling_cluts_for(self, pe2img_path: Path, item: AssetItem | None) -> list[Path]:
        raw: list[Path] = []
        if item is not None and item.sibling_cluts:
            raw = list(item.sibling_cluts)
        if not raw:
            raw = self._lookup_cluts(pe2img_path)
        # Always normalise to raw .pe2clut blobs (never inflated PNG strips).
        out: list[Path] = []
        seen: set[Path] = set()
        for p in raw:
            p = prefer_raw_blob(self.assets_root, p)
            if p.suffix.lower() != ".pe2clut" or not p.exists():
                continue
            rp = p.resolve()
            if rp not in seen:
                seen.add(rp)
                out.append(rp)
        return out

    def _refresh_clut_list(self) -> None:
        """Rebuild CLUT combobox for the current pe2img."""
        if self._pe2img_path is None:
            self._set_clut_controls_enabled(False)
            return
        item = self._current_item
        choices: list[tuple[str, Path | None]] = [("(none — index greyscale)", None)]
        if self._clut_scope.get() == "all":
            for p in list_all_cluts(self.assets_root):
                try:
                    label = str(p.relative_to(self.assets_root))
                except ValueError:
                    label = p.name
                choices.append((label, p))
        else:
            sibs = self._sibling_cluts_for(self._pe2img_path, item)
            if not sibs:
                choices.append(
                    ("(no stage siblings — try All CLUTs)", None)
                )
            for p in sibs:
                try:
                    label = str(p.relative_to(self.assets_root))
                except ValueError:
                    label = p.name
                choices.append((f"{label}  [sibling]", p))
        self._clut_choices = choices
        labels = [c[0] for c in choices]
        # Prefer first real CLUT when applying by default
        prefer = None
        for lab, p in choices:
            if p is not None:
                prefer = lab
                break
        self._clut_combo.configure(values=labels)
        if prefer is not None and self._clut_apply.get():
            self._clut_var.set(prefer)
        elif self._clut_var.get() not in labels:
            self._clut_var.set(labels[0])
        self._clut_combo.configure(state="readonly")
        self._clut_apply_cb.configure(state=tk.NORMAL)
        self._bpp_combo.configure(state="readonly")

    def _selected_clut_path(self) -> Path | None:
        label = self._clut_var.get()
        for lab, path in self._clut_choices:
            if lab == label:
                return path
        return None

    def _get_clut_colors(self, clut_path: Path, bpp: int) -> list[int] | None:
        clut_path = prefer_raw_blob(self.assets_root, clut_path)
        if clut_path.suffix.lower() != ".pe2clut":
            return None
        cache_key = f"{clut_path.resolve()}|{bpp}"
        if cache_key in self._clut_color_cache:
            return self._clut_color_cache[cache_key]
        try:
            from asset_decode import load_clut_palette

            colors = load_clut_palette(clut_path, bpp=bpp)
        except Exception:
            colors = None
        self._clut_color_cache[cache_key] = colors
        return colors

    def _on_clut_scope_changed(self) -> None:
        self._refresh_clut_list()
        self._on_clut_changed()

    def _on_clut_changed(self) -> None:
        if self._pe2img_path is None or self._pe2img_data is None:
            return
        self._decode_pe2img_to_canvas(
            self._pe2img_path,
            self._pe2img_data,
            apply_clut=bool(self._clut_apply.get()),
            clut_path=self._selected_clut_path(),
        )

    def _selected_bpp(self) -> int | None:
        """Manual BPP combo value, or None for Auto."""
        raw = self._bpp_var.get()
        if raw in ("4", "8", "16"):
            return int(raw)
        return None

    def _auto_bpp_for_current(self) -> int | None:
        """IMAGE_BPP override for the current pe2img, or None to guess."""
        idents: list[str] = []
        if self._pe2img_path is not None:
            p = self._pe2img_path
            idents.extend((p.stem, p.name, str(p)))
        item = self._current_item
        if item is not None:
            idents.extend((item.path.stem, item.path.name, item.label))
        for ident in list(idents):
            for canon in self._img_canon_index.get(ident, []):
                idents.append(canon)
        return lookup_image_bpp(*idents)

    def _show_pe2img(
        self, path: Path, data: bytes, *, item: AssetItem
    ) -> bool:
        self._pe2img_path = path
        self._pe2img_data = data
        self._clut_apply.set(True)
        self._bpp_var.set("Auto")
        self._refresh_clut_list()
        clut_path = self._selected_clut_path()
        return self._decode_pe2img_to_canvas(
            path,
            data,
            apply_clut=True,
            clut_path=clut_path,
        )

    def _decode_pe2img_to_canvas(
        self,
        path: Path,
        data: bytes,
        *,
        apply_clut: bool,
        clut_path: Path | None,
    ) -> bool:
        """Preview pe2img via shared :func:`asset_decode.render_pe2img`."""
        try:
            from asset_decode import render_pe2img
        except ImportError as e:
            self._show_text_message(f"Image decode unavailable: {e}")
            return False
        try:
            clut = prefer_raw_blob(self.assets_root, clut_path) if clut_path else None
            if clut is not None and clut.suffix.lower() != ".pe2clut":
                clut = None
            manual = self._selected_bpp()
            if manual is not None:
                bpp = manual
                bpp_src = "manual"
            else:
                bpp = self._auto_bpp_for_current()
                bpp_src = "override" if bpp is not None else "guess"
            img, info, colors = render_pe2img(
                data,
                apply_clut=bool(apply_clut and clut is not None),
                clut_path=clut if apply_clut else None,
                bpp=bpp,
            )
            self._current_clut_colors = colors
            bpp_note = (
                f"bpp={info.bpp}"
                if bpp_src != "guess"
                else f"bpp≈{info.bpp}"
            )
            bpp_note += f" ({bpp_src})"
            if apply_clut and clut is not None and colors is not None:
                note = (
                    f"{bpp_note}  cols={len(info.entries)}  h={info.height}  "
                    f"clut={clut.name}  {len(colors)} entries"
                )
            elif apply_clut and clut is not None:
                note = (
                    f"{bpp_note}  cols={len(info.entries)}  "
                    f"h={info.height}  clut load failed: {clut.name}"
                )
            else:
                note = (
                    f"{bpp_note}  cols={len(info.entries)}  "
                    f"h={info.height}  clut=off"
                )
        except Exception as e:
            self._show_text_message(f"pe2img decode failed:\n{e}")
            return False

        self._set_pil_image(img)
        self._status_var.set(f"{path}  ·  {note}")
        return True

    def _show_pe2clut(self, path: Path, data: bytes) -> bool:
        try:
            from asset_decode import load_clut_palette, render_pe2clut
            from image_codec import _load_clut_blob
        except ImportError as e:
            self._show_text_message(f"Image decode unavailable: {e}")
            return False
        try:
            path = prefer_raw_blob(self.assets_root, path)
            data = path.read_bytes()
            img, info = render_pe2clut(data)
            blob = _load_clut_blob(path)
            self._current_clut_colors = blob[4] if blob else load_clut_palette(path)
        except Exception as e:
            self._show_text_message(f"pe2clut decode failed:\n{e}")
            return False
        self._pe2img_path = None
        self._pe2img_data = None
        self._set_pil_image(img)
        return True

    def _set_pil_image(self, img: Any) -> None:
        self._current_pil = img
        self._draw_image()

    def _on_canvas_resize(self, _event: tk.Event) -> None:
        if self._current_pil is not None:
            self._draw_image()

    def _checkerboard(self, w: int, h: int, cell: int = 8) -> Any:
        """Light/dark checkerboard for transparent pixel visibility."""
        from PIL import Image

        img = Image.new("RGB", (max(w, 1), max(h, 1)))
        px = img.load()
        c0, c1 = (60, 60, 60), (90, 90, 90)
        for y in range(h):
            for x in range(w):
                px[x, y] = c0 if ((x // cell) ^ (y // cell)) & 1 == 0 else c1
        return img

    def _draw_image(self) -> None:
        img = self._current_pil
        if img is None:
            return
        try:
            from PIL import Image, ImageTk
            from image_codec import _abgr1555_to_rgba
        except ImportError:
            return

        self.img_canvas.update_idletasks()
        cw = max(self.img_canvas.winfo_width(), 64)
        ch = max(self.img_canvas.winfo_height(), 64)
        palette_h = 28 if self._current_clut_colors else 0
        iw, ih = img.size
        avail_h = max(ch - palette_h - 8, 32)
        scale = min(cw / iw, avail_h / ih, 4.0)
        if scale >= 1:
            scale = max(1, int(scale))
        nw, nh = max(1, int(iw * scale)), max(1, int(ih * scale))
        resample = Image.Resampling.NEAREST if scale >= 1 else Image.Resampling.BILINEAR
        # Composite onto checkerboard so alpha is visible
        rgba = img.convert("RGBA")
        disp = rgba.resize((nw, nh), resample)
        board = self._checkerboard(nw, nh, cell=max(4, int(8 * min(scale, 2))))
        board = board.convert("RGBA")
        composed = Image.alpha_composite(board, disp)
        photo = ImageTk.PhotoImage(composed)
        self._photo_ref = photo
        self.img_canvas.delete("all")
        # Fill canvas with checkerboard too (letterboxing)
        full_board = self._checkerboard(cw, max(ch - palette_h, 1), cell=10)
        self._bg_photo = ImageTk.PhotoImage(full_board)
        self.img_canvas.create_image(0, 0, image=self._bg_photo, anchor=tk.NW)
        img_cy = (ch - palette_h) // 2
        self.img_canvas.create_image(cw // 2, img_cy, image=photo, anchor=tk.CENTER)
        self.img_canvas.create_text(
            8,
            8,
            anchor=tk.NW,
            text=f"{iw}×{ih}  ×{scale:g}",
            fill="#ddd",
            font=("", 10),
        )
        # CLUT swatch strip along the bottom
        if self._current_clut_colors:
            n = min(len(self._current_clut_colors), 256)
            sw = Image.new("RGBA", (max(n, 1), 1), (0, 0, 0, 255))
            px = sw.load()
            for i in range(n):
                px[i, 0] = _abgr1555_to_rgba(self._current_clut_colors[i])
            strip = sw.resize(
                (max(cw - 16, 8), palette_h - 6), Image.Resampling.NEAREST
            )
            self._palette_photo = ImageTk.PhotoImage(strip)
            self.img_canvas.create_image(
                cw // 2, ch - palette_h // 2, image=self._palette_photo, anchor=tk.CENTER
            )
            self.img_canvas.create_text(
                8,
                ch - palette_h + 2,
                anchor=tk.NW,
                text=f"CLUT  {n} colours",
                fill="#ccc",
                font=("", 9),
            )
        else:
            self._palette_photo = None

    def _show_text_message(self, msg: str) -> None:
        self._set_text_widget(self.text_deflated, msg)
        self._set_text_widget(self.text_raw, "")
        self.preview_nb.select(1)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "assets",
        type=Path,
        nargs="?",
        default=Path("assets/USA"),
        help="Assets root (default: assets/USA)",
    )
    args = parser.parse_args(argv)
    root = args.assets
    if not root.exists():
        # try relative to repo root
        repo = Path(__file__).resolve().parents[2]
        alt = repo / root
        if alt.exists():
            root = alt
        else:
            print(f"assets root not found: {args.assets}", file=sys.stderr)
            return 1

    app = AssetViewer(root)
    app.mainloop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
