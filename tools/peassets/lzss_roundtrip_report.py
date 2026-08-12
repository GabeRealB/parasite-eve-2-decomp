#!/usr/bin/env python3
"""Round-trip + pack-layout report for production ``lzss.encode_lzss``.

Checks every independent LZSS stream under ``assets/USA/raw``:

* **Round-trip:** ``decode(encode(decode(retail))) == decode(retail)``
* **Sector layout:** sectors needed to pack the clean payload with the
  asset's ``sector_len`` (from ``stages.json``) — compare retail vs re-encode.

CLUT payloads use ``encode_lzss(..., kind="clut")`` (delegates to
``lzss_clut``). Packages and image strips use the default best-effort greedy
encoder.

Usage::

    python3 tools/peassets/lzss_roundtrip_report.py
    python3 tools/peassets/lzss_roundtrip_report.py --root assets/USA/raw/stage0
    python3 tools/peassets/lzss_roundtrip_report.py --log layout_diff.log
    python3 tools/peassets/lzss_roundtrip_report.py --json report.json

Exit code 0 if every stream round-trips and no re-encode needs *more* sectors
than retail. Exit 1 otherwise (layout shrinks are logged but still OK).
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
from collections import Counter
from dataclasses import asdict, dataclass
from pathlib import Path

_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from format import (  # noqa: E402
    FILE_CHUNK_HEADER_SIZE,
    SECTOR_SIZE,
    validate_sector_len,
)
from lzss import (  # noqa: E402
    compressed_size,
    decode_lzss,
    encode_lzss,
    trim_lzss,
)

CLUT_HEADER = 16
REPO_USA = Path("assets/USA")
DEFAULT_RAW = REPO_USA / "raw"
DEFAULT_STAGES = REPO_USA / "stages.json"


def sectors_for_payload(payload_len: int, sector_len: int) -> int:
    """Sectors needed to pack a clean payload (matches ``pack_chunk_payload``)."""
    sector_len = validate_sector_len(sector_len)
    first_cap = sector_len - FILE_CHUNK_HEADER_SIZE
    cont_cap = sector_len
    if payload_len <= first_cap:
        return 1
    rem = payload_len - first_cap
    return 1 + (rem + cont_cap - 1) // cont_cap


def load_sector_map(stages_path: Path) -> dict[str, tuple[int, int | None]]:
    """Map path keys → (sector_len, chunk_size) from stages.json."""
    sj = json.loads(stages_path.read_text())
    m: dict[str, tuple[int, int | None]] = {}
    for stage, body in sj.items():
        files = body.get("files") or {}
        for folder, entries in files.items():
            for name, meta in entries.items():
                if not isinstance(meta, dict):
                    continue
                sl = int(str(meta.get("sector_len", "0x800")), 0)
                cs = meta.get("chunk_size")
                cs_i = int(str(cs), 0) if cs is not None else None
                path = meta.get("path", "")
                p = Path(path)
                parts = list(p.parts)
                if parts and parts[0] == "decoded":
                    parts[0] = "raw"
                raw_rel = str(Path(*parts)) if parts else ""
                keys = {
                    name,
                    path,
                    raw_rel,
                    str(Path(raw_rel).with_suffix("")),
                    p.name,
                    f"{stage}/{folder}/{name}",
                    f"raw/{stage}/{folder}/{name}",
                }
                if p.suffix.lower() == ".png":
                    keys.add(str(Path(raw_rel).with_suffix(".pe2img")))
                    keys.add(p.with_suffix(".pe2img").name)
                if p.suffix.lower() in (".pe2pkg", ".pe2clut", ".pe2img"):
                    keys.add(str(Path(raw_rel)))
                for k in keys:
                    if k:
                        m[k] = (sl, cs_i)
    return m


def resolve_sector(
    path: Path, raw_root: Path, smap: dict[str, tuple[int, int | None]]
) -> tuple[int, int | None, str]:
    try:
        rel_usa = path.relative_to(raw_root.parent)
        rel_raw = path.relative_to(raw_root)
    except ValueError:
        rel_usa = path
        rel_raw = path
    candidates = [
        str(rel_usa),
        f"raw/{rel_raw}",
        str(rel_raw),
        path.name,
        f"{path.parent.name}/{path.name}",
    ]
    for c in candidates:
        if c in smap:
            return (*smap[c], c)
    return 0x800, None, "default"


def reencode_pkg(raw: bytes) -> bytes:
    return encode_lzss(decode_lzss(trim_lzss(raw)))


def reencode_clut(raw: bytes) -> bytes:
    if len(raw) < CLUT_HEADER:
        raise ValueError("pe2clut shorter than header")
    body = decode_lzss(trim_lzss(raw[CLUT_HEADER:]))
    return raw[:CLUT_HEADER] + encode_lzss(body, kind="clut")


def reencode_img(raw: bytes) -> bytes:
    off = 0
    while off + 8 <= len(raw):
        x = struct.unpack_from("<H", raw, off)[0]
        off += 8
        if x == 0xFFFF:
            break
    out = bytearray(raw[:off])
    stream = raw[off:]
    ibcar = 0
    while ibcar < len(stream):
        z0 = ibcar
        while ibcar < len(stream) and stream[ibcar] == 0:
            ibcar += 1
        out += stream[z0:ibcar]
        if ibcar >= len(stream):
            break
        n = compressed_size(stream[ibcar:])
        if n <= 0:
            break
        strip = trim_lzss(stream[ibcar : ibcar + n])
        out += encode_lzss(decode_lzss(strip))
        ibcar += n
    return bytes(out)


def roundtrip_ok(kind: str, raw: bytes, enc: bytes) -> bool:
    if kind == "pkg":
        return decode_lzss(trim_lzss(raw)) == decode_lzss(trim_lzss(enc))
    if kind == "clut":
        return (
            enc[:CLUT_HEADER] == raw[:CLUT_HEADER]
            and decode_lzss(trim_lzss(raw[CLUT_HEADER:]))
            == decode_lzss(trim_lzss(enc[CLUT_HEADER:]))
        )

    def inflate_strips(data: bytes) -> tuple[bytes, list[bytes]]:
        off = 0
        while off + 8 <= len(data):
            x = struct.unpack_from("<H", data, off)[0]
            off += 8
            if x == 0xFFFF:
                break
        table = data[:off]
        stream = data[off:]
        ibcar = 0
        outs: list[bytes] = []
        while ibcar < len(stream):
            while ibcar < len(stream) and stream[ibcar] == 0:
                ibcar += 1
            if ibcar >= len(stream):
                break
            n = compressed_size(stream[ibcar:])
            if n <= 0:
                break
            outs.append(decode_lzss(trim_lzss(stream[ibcar : ibcar + n])))
            ibcar += n
        return table, outs

    t0, s0 = inflate_strips(raw)
    t1, s1 = inflate_strips(enc)
    return t0 == t1 and s0 == s1


@dataclass
class LayoutDiff:
    path: str
    kind: str
    sector_len: int
    map_key: str
    retail_bytes: int
    encode_bytes: int
    retail_sectors: int
    encode_sectors: int
    sector_delta: int
    byte_delta: int
    chunk_size: int | None
    retail_alloc_sectors: int | None
    encode_exceeds_chunk_size: bool
    note: str


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=DEFAULT_RAW,
        help="raw asset tree (default: assets/USA/raw)",
    )
    ap.add_argument(
        "--stages",
        type=Path,
        default=DEFAULT_STAGES,
        help="stages.json for sector_len / chunk_size",
    )
    ap.add_argument(
        "--log",
        type=Path,
        default=None,
        help="write human-readable layout-diff log (default: stdout only)",
    )
    ap.add_argument(
        "--json",
        type=Path,
        default=None,
        help="optional JSON summary path",
    )
    args = ap.parse_args(argv)

    raw_root = args.root.resolve()
    if not raw_root.is_dir():
        print(f"error: root not a directory: {raw_root}", file=sys.stderr)
        return 2

    stages_path = args.stages
    if not stages_path.is_file():
        # allow running from repo root with relative default
        alt = Path(__file__).resolve().parents[2] / "assets/USA/stages.json"
        if alt.is_file():
            stages_path = alt
    smap = load_sector_map(stages_path) if stages_path.is_file() else {}

    kinds = [
        ("*.pe2pkg", "pkg", reencode_pkg),
        ("*.pe2clut", "clut", reencode_clut),
        ("*.pe2img", "img", reencode_img),
    ]

    rt_ok = Counter()
    rt_fail: list[tuple[str, str]] = []
    sec_delta = Counter()  # (kind, delta)
    diffs: list[LayoutDiff] = []
    map_default = Counter()

    log_lines: list[str] = []

    def log(msg: str = "") -> None:
        log_lines.append(msg)
        print(msg, flush=True)

    log(f"root={raw_root}")
    log(f"stages={stages_path}  map_keys={len(smap)}")
    log("encoder: encode_lzss (kind=clut → lzss_clut; else greedy newest)")
    log("")

    for glob, kind, reenc in kinds:
        files = sorted(raw_root.rglob(glob))
        log(f"=== {kind}: {len(files)} files ===")
        for i, f in enumerate(files):
            if i and i % 500 == 0:
                log(f"  … {i}/{len(files)}")
            raw = f.read_bytes()
            rel = str(f.relative_to(raw_root))
            try:
                enc = reenc(raw)
                if not roundtrip_ok(kind, raw, enc):
                    rt_fail.append((rel, "decode mismatch"))
                    rt_ok[f"{kind}_fail"] += 1
                    continue
            except Exception as e:
                rt_fail.append((rel, str(e)))
                rt_ok[f"{kind}_err"] += 1
                continue
            rt_ok[f"{kind}_ok"] += 1

            ret_payload = trim_lzss(raw) if kind == "pkg" else raw
            enc_payload = trim_lzss(enc) if kind == "pkg" else enc
            sl, cs, how = resolve_sector(f, raw_root, smap)
            if how == "default":
                map_default[kind] += 1
            rs = sectors_for_payload(len(ret_payload), sl)
            es = sectors_for_payload(len(enc_payload), sl)
            delta = es - rs
            sec_delta[(kind, delta)] += 1

            retail_n = (cs // SECTOR_SIZE) if cs is not None else None
            exceeds = retail_n is not None and es > retail_n
            note_parts: list[str] = []
            if delta != 0:
                note_parts.append(
                    "sector_count_changed"
                    if delta > 0
                    else "sector_count_smaller"
                )
            if exceeds:
                note_parts.append("encode_exceeds_chunk_size")
            if retail_n is not None and rs > retail_n:
                note_parts.append("retail_also_exceeds_chunk_size")
            if how == "default":
                note_parts.append("sector_len_default_0x800")

            if delta != 0 or exceeds or how == "default":
                diffs.append(
                    LayoutDiff(
                        path=rel,
                        kind=kind,
                        sector_len=sl,
                        map_key=how,
                        retail_bytes=len(ret_payload),
                        encode_bytes=len(enc_payload),
                        retail_sectors=rs,
                        encode_sectors=es,
                        sector_delta=delta,
                        byte_delta=len(enc_payload) - len(ret_payload),
                        chunk_size=cs,
                        retail_alloc_sectors=retail_n,
                        encode_exceeds_chunk_size=exceeds,
                        note=",".join(note_parts) if note_parts else "",
                    )
                )

    log("")
    log("======== ROUND-TRIP ========")
    total_ok = sum(rt_ok[k] for k in rt_ok if k.endswith("_ok"))
    total_bad = sum(rt_ok[k] for k in rt_ok if not k.endswith("_ok"))
    for k in sorted(rt_ok):
        log(f"  {k}: {rt_ok[k]}")
    if rt_fail:
        log(f"  failures ({len(rt_fail)}):")
        for p, msg in rt_fail[:30]:
            log(f"    {p}: {msg}")
        if len(rt_fail) > 30:
            log(f"    … {len(rt_fail) - 30} more")

    log("")
    log("======== SECTOR COUNT (encode − retail payload need) ========")
    any_grow = False
    for kind in ("pkg", "clut", "img"):
        rows = [(d, n) for (k, d), n in sec_delta.items() if k == kind]
        total = sum(n for _, n in rows) or 0
        eq = sec_delta.get((kind, 0), 0)
        log(f"  {kind}: equal={eq}/{total}  unmapped_sector_len={map_default[kind]}")
        for d, n in sorted(rows):
            if d != 0:
                log(f"    delta={d:+d}: {n}")
                if d > 0:
                    any_grow = True

    layout_diffs = [d for d in diffs if d.sector_delta != 0]
    log("")
    log(f"======== LAYOUT DIFFERENCES ({len(layout_diffs)} sector≠retail) ========")
    if not layout_diffs:
        log("  (none — all re-encodes need the same sector count as retail)")
    else:
        log(
            "  path  kind  sec_retail→enc (Δ)  bytes_retail→enc (Δ)  "
            "sector_len  map  notes"
        )
        for d in sorted(layout_diffs, key=lambda x: (-abs(x.sector_delta), x.path)):
            log(
                f"  {d.path}  {d.kind}  "
                f"{d.retail_sectors}→{d.encode_sectors} ({d.sector_delta:+d})  "
                f"{d.retail_bytes}→{d.encode_bytes} ({d.byte_delta:+d})  "
                f"0x{d.sector_len:X}  {d.map_key}  {d.note}"
            )

    grow = [d for d in layout_diffs if d.sector_delta > 0]
    shrink = [d for d in layout_diffs if d.sector_delta < 0]
    log("")
    log("======== SUMMARY ========")
    log(f"  round-trip: {total_ok} ok / {total_ok + total_bad} total")
    sec_tot = sum(sec_delta.values())
    sec_eq = sum(sec_delta[(k, 0)] for k in ("pkg", "clut", "img") if (k, 0) in sec_delta)
    log(f"  sector-equal: {sec_eq}/{sec_tot}")
    log(f"  sector-shrink: {len(shrink)}  sector-grow: {len(grow)}")
    log(
        "  note: shrinks are pack-safe (fewer sectors). "
        "Grows would expand the on-disc chunk."
    )

    if args.log is not None:
        args.log.parent.mkdir(parents=True, exist_ok=True)
        args.log.write_text("\n".join(log_lines) + "\n", encoding="utf-8")
        print(f"\nwrote log: {args.log}", flush=True)

    if args.json is not None:
        payload = {
            "root": str(raw_root),
            "roundtrip_ok": total_ok,
            "roundtrip_fail": total_bad,
            "sector_equal": sec_eq,
            "sector_total": sec_tot,
            "sector_grow": len(grow),
            "sector_shrink": len(shrink),
            "failures": [{"path": p, "error": e} for p, e in rt_fail],
            "layout_diffs": [asdict(d) for d in diffs if d.sector_delta != 0],
            "all_notes": [asdict(d) for d in diffs],
        }
        args.json.parent.mkdir(parents=True, exist_ok=True)
        args.json.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        print(f"wrote json: {args.json}", flush=True)

    if total_bad or any_grow:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
