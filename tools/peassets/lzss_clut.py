"""CLUT-only PE2 LZSS encoder (Okumura last-at-max).

Production entry: ``lzss.encode_lzss(data, kind="clut")``.

Policy
------
1. Greedy longest match in the last 256 written bytes.
2. Force literal when write ring index is ``0xFF``.
3. Force literal when the sole max-length candidate has age ≥ 242,
   or ``best==2`` and age ≥ 240.
4. Multi_max: persistent last-at-max if that ref is a max-cand, else newest.
5. Prefer newest over last-at-max for a few zero-FP shapes (newest is L;
   M17+12 vs L; T2 vs old L; end M17+16@1; BST age ≥ 256 or ≥ 248 with nc==2).

Always ``decode_lzss(encode_clut(data)) == data``.
Measured exact: **383/420** unique-raw USA ``*.pe2clut`` (``trim_lzss``).
"""

from __future__ import annotations

from lzss import (
    DICT_SIZE,
    MAX_MATCH,
    MIN_MATCH,
    _match_length,
    _offset_for_ref,
    _pack_tokens,
)
from lzss_bst import PersistentBST

SOLE_STALE_AGE = 242
SOLE_BEST2_STALE_AGE = 240
BST_F = 17
BST_REPLACE_AT_F = True
BST_SEED_ZEROS = True


def _tip_kind(ref: int, token_starts: dict[int, tuple]) -> str | None:
    tip = token_starts.get(ref)
    if tip is None:
        return None
    return tip[0]


def _should_force_lit_sole(
    sole: int | None, *, pos: int, best_len: int | None = None
) -> bool:
    if sole is None:
        return False
    age = pos - sole
    if age >= SOLE_STALE_AGE:
        return True
    if best_len == 2 and age >= SOLE_BEST2_STALE_AGE:
        return True
    return False


def _ref_class(
    ref: int, token_starts: dict[int, tuple], owners: dict[int, int]
) -> tuple[str, int, int]:
    """Return ``(kind, owner_pl, into)`` with kind ``L`` / ``T`` / ``mid`` / ``?``."""
    k = _tip_kind(ref, token_starts)
    if k == "L":
        return "L", 1, 0
    if k == "M":
        return "T", int(token_starts[ref][1]), 0
    owner = owners.get(ref, ref)
    tok = token_starts.get(owner)
    if tok is not None and tok[0] == "M":
        return "mid", int(tok[1]), ref - owner
    return "?", 0, 0


def _prefer_newest_over_bst(
    cands: list[int],
    newest: int,
    bst: int,
    *,
    pos: int,
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
) -> bool:
    """Take newest instead of last-at-max for encode-validated shapes (+11/−0)."""
    if bst not in cands or bst == newest:
        return False
    nc = len(cands)
    bst_age = pos - bst
    nk, npl, ninto = _ref_class(newest, token_starts, owners)
    bk, _bpl, _binto = _ref_class(bst, token_starts, owners)

    if nk == "L":
        return True
    if (
        best_len == 3
        and nc == 2
        and nk == "mid"
        and ninto == 12
        and npl >= 17
        and bk == "L"
    ):
        return True
    if (
        best_len == 2
        and nk == "T"
        and npl == 2
        and bk == "L"
        and bst_age >= 240
    ):
        return True
    if (
        nk == "mid"
        and ninto == 16
        and npl >= 17
        and (pos - newest) == 1
        and (pos & 0xFF) >= 245
    ):
        return True
    if bst_age >= 256:
        return True
    if bst_age >= 248 and nc == 2:
        return True
    return False


def _pick_bst_if_cand(cands: list[int], tree: PersistentBST, pos: int) -> int:
    newest = max(cands)
    last = tree.last_at_max(pos)
    if last in cands:
        return last
    return newest


def encode_clut(data: bytes) -> bytes:
    """Encode ``data`` as PE2 LZSS with the CLUT Okumura policy."""
    n = len(data)
    if n == 0:
        return _pack_tokens([("EOS",)])

    tree = PersistentBST(
        data,
        f=BST_F,
        last_at_max=True,
        replace_at_f=BST_REPLACE_AT_F,
        seed_zeros=BST_SEED_ZEROS,
    )
    head = [-1] * 256
    prev = [-1] * n
    token_starts: dict[int, tuple] = {}
    owners: dict[int, int] = {}

    def insert(i: int) -> None:
        prev[i] = head[data[i]]
        head[data[i]] = i
        tree.feed(i)

    tokens: list[tuple] = []
    pos = 0

    while pos < n:
        if (pos & 0xFF) == 0xFF:
            tokens.append(("L", data[pos]))
            token_starts[pos] = ("L",)
            owners[pos] = pos
            insert(pos)
            pos += 1
            continue

        window_start = max(0, pos - DICT_SIZE)
        best_len = 0
        cands: list[int] = []

        p = head[data[pos]]
        while p >= window_start:
            if _offset_for_ref(p) is not None:
                length = _match_length(data, pos, p, MAX_MATCH)
                if length >= MIN_MATCH:
                    if length > best_len:
                        best_len = length
                        cands = [p]
                    elif length == best_len:
                        cands.append(p)
            p = prev[p]

        if best_len >= MIN_MATCH and cands:
            sole = cands[0] if len(cands) == 1 else None
            if _should_force_lit_sole(sole, pos=pos, best_len=best_len):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            newest = max(cands)
            chosen = _pick_bst_if_cand(cands, tree, pos)
            if _prefer_newest_over_bst(
                cands,
                newest,
                chosen,
                pos=pos,
                best_len=best_len,
                token_starts=token_starts,
                owners=owners,
            ):
                chosen = newest
            off = _offset_for_ref(chosen)
            assert off is not None
            tokens.append(("M", off, best_len))
            token_starts[pos] = ("M", best_len)
            for i in range(best_len):
                owners[pos + i] = pos
            end = pos + best_len
            cut = end - DICT_SIZE - (MAX_MATCH - 1)
            if cut > 0:
                token_starts = {k: v for k, v in token_starts.items() if k >= cut}
                owners = {k: v for k, v in owners.items() if k >= cut}
            while pos < end:
                insert(pos)
                pos += 1
        else:
            tokens.append(("L", data[pos]))
            token_starts[pos] = ("L",)
            owners[pos] = pos
            insert(pos)
            pos += 1

    tokens.append(("EOS",))
    return _pack_tokens(tokens)


def encode_lzss_clut(data: bytes) -> bytes:
    """Alias for :func:`encode_clut`."""
    return encode_clut(data)
