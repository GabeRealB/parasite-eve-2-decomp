#!/usr/bin/env python3
"""Report encode bit-identity of retail LZSS assets (pe2pkg / pe2clut / pe2img).

Goal: byte-identical ``encode_lzss(decode(retail))`` for every independent
LZSS stream used by stage assets. Uses the **frozen** cascade encoder in
``lzss_cascading`` (not the simple ``lzss.encode_lzss``).

Streams
-------
* ``.pe2pkg`` — whole file is one LZSS payload (after ``trim_lzss``)
* ``.pe2clut`` — 16-byte header + one LZSS palette payload
* ``.pe2img`` — work-entry table + sequential independent LZSS strips
  (dictionary reset between strips; each strip inflates to 0x1000 bytes)

Usage::

    python3 tools/peassets/lzss_identity_report.py
    python3 tools/peassets/lzss_identity_report.py --root assets/USA/raw/stage0
    python3 tools/peassets/lzss_identity_report.py --pure-only
    python3 tools/peassets/lzss_identity_report.py --limit 50 --json report.json
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterator

# Allow running as script from repo root or tools/peassets/
_HERE = Path(__file__).resolve().parent
if str(_HERE) not in sys.path:
    sys.path.insert(0, str(_HERE))

from lzss import (  # noqa: E402
    DICT_SIZE,
    MAX_MATCH,
    MIN_MATCH,
    _match_length,
    _offset_for_ref,
    compressed_size,
    decode_lzss,
    trim_lzss,
)
from lzss_cascading import encode_lzss  # noqa: E402  frozen identity cascade

STRIP_BYTES = 0x1000
CLUT_HEADER = 16


@dataclass
class StreamResult:
    path: str
    kind: str  # pe2pkg | pe2clut | pe2img_strip
    index: int  # strip index for pe2img; 0 otherwise
    data_len: int
    retail_len: int
    encode_len: int
    exact: bool
    pure_max: bool
    not_max_steps: int
    multi_max_steps: int
    token_prefix: int
    token_total: int
    first_miss: str | None
    first_miss_kind: str | None  # multi_max | not_max | path | None
    error: str | None = None


def tokenize(stream: bytes) -> list[tuple]:
    """Return retail tokens: ('L', byte, pos) | ('M', off, length, pos) | ('EOS', pos)."""
    buf = 0
    mask = 0
    ibcar = 0

    def get_bit(n: int) -> int:
        nonlocal mask, buf, ibcar
        x = 0
        for _ in range(n):
            if mask == 0:
                if ibcar >= len(stream):
                    buf = 0
                    mask = 0
                    return x
                buf = stream[ibcar]
                ibcar += 1
                mask = 128
            x = (x << 1) | (1 if (buf & mask) else 0)
            mask >>= 1
        return x

    toks: list[tuple] = []
    pos = 0
    while True:
        if get_bit(1):
            lit = get_bit(8)
            toks.append(("L", lit, pos))
            pos += 1
        else:
            off = get_bit(8)
            if off == 0:
                toks.append(("EOS", pos))
                break
            length = get_bit(4) + 2
            toks.append(("M", off, length, pos))
            pos += length
    return toks


def tokens_from_encode(data: bytes) -> list[tuple]:
    """Tokenize our encoder output with absolute positions (for prefix compare)."""
    return tokenize(encode_lzss(data))


def token_prefix_len(retail: list[tuple], ours: list[tuple]) -> tuple[int, int, str | None]:
    """Return (matching_prefix, retail_token_count_excl_eos, first_miss_str)."""
    rt = [t for t in retail if t[0] != "EOS"]
    ot = [t for t in ours if t[0] != "EOS"]
    pref = 0
    miss = None
    for a, b in zip(rt, ot):
        if a[0] == "L" and b[0] == "L" and a[1] == b[1]:
            pref += 1
            continue
        if a[0] == "M" and b[0] == "M" and a[1] == b[1] and a[2] == b[2]:
            pref += 1
            continue
        miss = f"retail={a} ours={b}"
        break
    else:
        if len(rt) != len(ot):
            miss = f"length retail={len(rt)} ours={len(ot)}"
    return pref, len(rt), miss


def analyze_retail_path(data: bytes, retail_toks: list[tuple]) -> tuple[bool, int, int]:
    """Walk retail tokens; return (pure_max, not_max_count, multi_max_count)."""
    head = [-1] * 256
    prev = [-1] * len(data)

    def insert(i: int) -> None:
        b = data[i]
        prev[i] = head[b]
        head[b] = i

    pos = 0
    pure = True
    not_max = 0
    multi_max = 0
    for t in retail_toks:
        if t[0] == "EOS":
            break
        if t[0] == "L":
            insert(pos)
            pos += 1
            continue
        off, length, rpos = t[1], t[2], t[3]
        if rpos != pos:
            # path desync — treat as impure
            return False, not_max + 1, multi_max
        ws = max(0, pos - DICT_SIZE)
        best_len = 0
        cands: list[int] = []
        p = head[data[pos]]
        while p >= ws:
            if _offset_for_ref(p) is not None:
                ml = _match_length(data, pos, p, MAX_MATCH)
                if ml >= MIN_MATCH:
                    if ml > best_len:
                        best_len = ml
                        cands = [p]
                    elif ml == best_len:
                        cands.append(p)
            p = prev[p]
        if best_len != length:
            pure = False
            not_max += 1
        elif len(cands) > 1:
            multi_max += 1
        end = pos + length
        while pos < end:
            insert(pos)
            pos += 1
    return pure, not_max, multi_max


def classify_first_miss(
    data: bytes, retail_toks: list[tuple], ours_toks: list[tuple]
) -> str | None:
    """Classify first token mismatch as multi_max / not_max / path."""
    rt = [t for t in retail_toks if t[0] != "EOS"]
    ot = [t for t in ours_toks if t[0] != "EOS"]
    # Find first position where tokens differ; re-walk to that pos for best_len
    head = [-1] * 256
    prev = [-1] * len(data)

    def insert(i: int) -> None:
        b = data[i]
        prev[i] = head[b]
        head[b] = i

    pos = 0
    for a, b in zip(rt, ot):
        same = (a[0] == "L" and b[0] == "L" and a[1] == b[1]) or (
            a[0] == "M" and b[0] == "M" and a[1] == b[1] and a[2] == b[2]
        )
        if same:
            if a[0] == "L":
                insert(pos)
                pos += 1
            else:
                end = pos + a[2]
                while pos < end:
                    insert(pos)
                    pos += 1
            continue
        # mismatch at pos
        if a[0] != "M" or pos >= len(data):
            return "path"
        ws = max(0, pos - DICT_SIZE)
        best_len = 0
        cands: list[int] = []
        p = head[data[pos]]
        while p >= ws:
            if _offset_for_ref(p) is not None:
                ml = _match_length(data, pos, p, MAX_MATCH)
                if ml >= MIN_MATCH:
                    if ml > best_len:
                        best_len = ml
                        cands = [p]
                    elif ml == best_len:
                        cands.append(p)
            p = prev[p]
        if a[0] == "M" and a[2] < best_len:
            return "not_max"
        if a[0] == "M" and a[2] == best_len and len(cands) > 1:
            return "multi_max"
        return "path"
    if len(rt) != len(ot):
        return "path"
    return None


def analyze_stream(
    path: str,
    kind: str,
    index: int,
    retail_stream: bytes,
    *,
    deep: bool = True,
    max_data: int = 0,
) -> StreamResult | None:
    """Analyse one compressed stream.

    ``deep=False`` only checks exactness + sizes (fast path for bulk scans).
    ``deep=True`` also computes pure-max, token prefix, and miss classification.
    Returns ``None`` if ``max_data`` is set and inflated size exceeds it
    (skipped *before* encode).
    """
    try:
        retail = trim_lzss(retail_stream)
        data = decode_lzss(retail)
        if max_data and len(data) > max_data:
            return None
        enc = encode_lzss(data)
        exact = enc == retail
        if exact and not deep:
            return StreamResult(
                path=path,
                kind=kind,
                index=index,
                data_len=len(data),
                retail_len=len(retail),
                encode_len=len(enc),
                exact=True,
                pure_max=False,  # not computed in shallow mode
                not_max_steps=0,
                multi_max_steps=0,
                token_prefix=0,
                token_total=0,
                first_miss=None,
                first_miss_kind=None,
            )
        rtoks = tokenize(retail)
        pure, n_not, n_multi = analyze_retail_path(data, rtoks)
        if exact:
            ntok = sum(1 for t in rtoks if t[0] != "EOS")
            return StreamResult(
                path=path,
                kind=kind,
                index=index,
                data_len=len(data),
                retail_len=len(retail),
                encode_len=len(enc),
                exact=True,
                pure_max=pure,
                not_max_steps=n_not,
                multi_max_steps=n_multi,
                token_prefix=ntok,
                token_total=ntok,
                first_miss=None,
                first_miss_kind=None,
            )
        otoks = tokenize(enc)
        pref, total, miss = token_prefix_len(rtoks, otoks)
        miss_kind = classify_first_miss(data, rtoks, otoks)
        return StreamResult(
            path=path,
            kind=kind,
            index=index,
            data_len=len(data),
            retail_len=len(retail),
            encode_len=len(enc),
            exact=False,
            pure_max=pure,
            not_max_steps=n_not,
            multi_max_steps=n_multi,
            token_prefix=pref,
            token_total=total,
            first_miss=miss,
            first_miss_kind=miss_kind,
        )
    except Exception as e:
        return StreamResult(
            path=path,
            kind=kind,
            index=index,
            data_len=0,
            retail_len=len(retail_stream),
            encode_len=0,
            exact=False,
            pure_max=False,
            not_max_steps=0,
            multi_max_steps=0,
            token_prefix=0,
            token_total=0,
            first_miss=None,
            first_miss_kind=None,
            error=str(e),
        )


def iter_pe2img_compressed_strips(body: bytes) -> Iterator[bytes]:
    """Yield each independent compressed LZSS strip (trimmed) from pe2img body."""
    # Skip work-entry table: entries until x==0xFFFF
    import struct

    off = 0
    while off + 8 <= len(body):
        x = struct.unpack_from("<H", body, off)[0]
        off += 8
        if x == 0xFFFF:
            break
    stream = body[off:]
    ibcar = 0
    while ibcar < len(stream):
        while ibcar < len(stream) and stream[ibcar] == 0:
            ibcar += 1
        if ibcar >= len(stream):
            break
        chunk = stream[ibcar:]
        try:
            n = compressed_size(chunk)
        except Exception:
            break
        if n <= 0:
            break
        yield trim_lzss(chunk[:n])
        ibcar += n


def collect_streams(root: Path) -> Iterator[tuple[str, str, int, bytes]]:
    """Yield (path, kind, index, compressed_stream) under root."""
    for dirpath, _dirs, files in os.walk(root):
        for name in sorted(files):
            path = Path(dirpath) / name
            suf = path.suffix.lower()
            rel = str(path)
            try:
                raw = path.read_bytes()
            except OSError:
                continue
            if suf == ".pe2pkg":
                if len(raw) < 2:
                    continue
                yield rel, "pe2pkg", 0, raw
            elif suf == ".pe2clut":
                if len(raw) <= CLUT_HEADER:
                    continue
                yield rel, "pe2clut", 0, raw[CLUT_HEADER:]
            elif suf == ".pe2img":
                try:
                    for i, strip in enumerate(iter_pe2img_compressed_strips(raw)):
                        if strip:
                            yield rel, "pe2img_strip", i, strip
                except Exception:
                    continue


def _compressed_cap_for_max_data(max_data: int) -> int:
    """Skip files whose compressed size already exceeds a loose bound.

    Inflated size is ≥ a few bytes and usually ≫ compressed size for these
    assets; using compressed size avoids decoding multi-hundred-KiB packages
    when the caller asked for small streams only.
    """
    # Allow some expansion; still drop huge overlays early.
    return max(max_data * 2, max_data + 64) if max_data else 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--root",
        type=Path,
        default=Path("assets/USA/raw"),
        help="Asset tree to scan (default assets/USA/raw)",
    )
    ap.add_argument("--limit", type=int, default=0, help="Max streams (0=all)")
    ap.add_argument("--pure-only", action="store_true", help="Only pure-max streams")
    ap.add_argument(
        "--misses-only", action="store_true", help="Only non-exact streams"
    )
    ap.add_argument(
        "--kind",
        choices=["pe2pkg", "pe2clut", "pe2img_strip", "all"],
        default="all",
    )
    ap.add_argument("--json", type=Path, default=None, help="Write full JSON report")
    ap.add_argument(
        "--max-data",
        type=int,
        default=0,
        help="Skip streams with inflated size > N (0=no limit)",
    )
    ap.add_argument(
        "--fast",
        action="store_true",
        help="Exact-check only for matches; deep analysis only on misses",
    )
    ap.add_argument(
        "--progress-every",
        type=int,
        default=200,
        help="Print progress every N streams (0=off)",
    )
    args = ap.parse_args()

    root = args.root
    if not root.is_dir():
        print(f"error: root not found: {root}", file=sys.stderr)
        return 1

    results: list[StreamResult] = []
    n_seen = 0
    n_skip = 0
    comp_cap = _compressed_cap_for_max_data(args.max_data)
    for path, kind, index, stream in collect_streams(root):
        if args.kind != "all" and kind != args.kind:
            continue
        if comp_cap and len(stream) > comp_cap:
            n_skip += 1
            continue
        n_seen += 1
        # pure-only / misses need deep; --fast keeps exact shallow
        deep = (not args.fast) or args.pure_only
        r = analyze_stream(
            path, kind, index, stream, deep=deep, max_data=args.max_data
        )
        if r is None:
            n_skip += 1
            continue
        if args.pure_only and not r.pure_max:
            continue
        if args.misses_only and r.exact:
            continue
        results.append(r)
        if args.progress_every and n_seen % args.progress_every == 0:
            exact_so_far = sum(1 for x in results if x.exact)
            print(
                f"  … {n_seen} streams, kept={len(results)}, "
                f"exact={exact_so_far}, skipped_large={n_skip}",
                flush=True,
            )
        if args.limit and len(results) >= args.limit:
            break

    if n_skip:
        print(f"(skipped {n_skip} streams over size caps)")

    # Summary by kind
    by_kind: dict[str, list[StreamResult]] = {}
    for r in results:
        by_kind.setdefault(r.kind, []).append(r)

    print(f"root={root} streams={len(results)} (scanned files walk)")
    print()
    for kind in ("pe2pkg", "pe2clut", "pe2img_strip"):
        rs = by_kind.get(kind, [])
        if not rs:
            continue
        exact = sum(1 for r in rs if r.exact)
        pure = sum(1 for r in rs if r.pure_max)
        errs = sum(1 for r in rs if r.error)
        miss_kinds: dict[str, int] = {}
        prefs = []
        for r in rs:
            if not r.exact and r.first_miss_kind:
                miss_kinds[r.first_miss_kind] = miss_kinds.get(r.first_miss_kind, 0) + 1
            if r.token_total:
                prefs.append(r.token_prefix / r.token_total)
        avg_pref = sum(prefs) / len(prefs) if prefs else 0.0
        print(
            f"{kind:14s}  n={len(rs):5d}  exact={exact:5d} ({100*exact/len(rs):5.1f}%)  "
            f"pure={pure:5d}  avg_token_prefix={avg_pref:.3f}  errors={errs}"
        )
        if miss_kinds:
            parts = ", ".join(f"{k}={v}" for k, v in sorted(miss_kinds.items()))
            print(f"{'':14s}  first_miss: {parts}")

    # Pure non-exact (actionable multi-max work)
    pure_miss = [r for r in results if r.pure_max and not r.exact and not r.error]
    pure_ok = [r for r in results if r.pure_max and r.exact]
    print()
    print(f"pure-max exact: {len(pure_ok)}  pure-max miss: {len(pure_miss)}")
    if pure_miss:
        print("pure-max misses (sorted by token_prefix ratio, then data_len):")
        pure_miss.sort(
            key=lambda r: (
                -(r.token_prefix / r.token_total if r.token_total else 0),
                r.data_len,
            )
        )
        for r in pure_miss[:40]:
            ratio = r.token_prefix / r.token_total if r.token_total else 0
            print(
                f"  {r.kind} {r.path}"
                + (f"#{r.index}" if r.kind == "pe2img_strip" else "")
                + f"  data={r.data_len}  pref={r.token_prefix}/{r.token_total}"
                f" ({ratio:.0%})  miss={r.first_miss_kind}  {r.first_miss}"
            )

    # Shortest impure prefix (good not-max targets)
    impure = [
        r
        for r in results
        if not r.exact and not r.pure_max and not r.error and r.token_total
    ]
    if impure:
        impure.sort(key=lambda r: (r.token_prefix, r.data_len))
        print()
        print("shortest token-prefix impure misses (not-max targets):")
        for r in impure[:20]:
            print(
                f"  {r.kind} {r.path}"
                + (f"#{r.index}" if r.kind == "pe2img_strip" else "")
                + f"  data={r.data_len}  pref={r.token_prefix}/{r.token_total}"
                f"  not_max_steps={r.not_max_steps}  miss={r.first_miss_kind}"
                f"  {r.first_miss}"
            )

    if args.json:
        args.json.write_text(
            json.dumps([asdict(r) for r in results], indent=2) + "\n"
        )
        print(f"\nwrote {args.json}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
