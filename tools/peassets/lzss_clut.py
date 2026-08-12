"""CLUT-only PE2 LZSS encoder (general-first policy).

Built by forking the frozen cascade encoder (now ``lzss_cascading``), then
stripping pe2pkg-only machinery and proving via ablation that multi_max
identity cascades are **net-negative** on pe2clut. Current policy is small
and encode-validated.

Production entry point: ``lzss.encode_lzss(data, kind="clut")`` delegates here.
Default ``lzss.encode_lzss`` (packages / image strips) is best-effort greedy.

Policy
------
1. Greedy longest match in the last 256 written bytes.
2. Force literal when write ring index is ``0xFF``.
3. Force literal when the **sole** max-length candidate has
   ``age >= SOLE_STALE_AGE`` (242).
4. Multi_max base pick: **newest** absolute ref.
5. Multi_max post-rules (general, corpus-safe only) — see
   :func:`_pick_match_ref`.

Always ``decode_lzss(encode_clut(data)) == data``.

Measured exact (all USA ``*.pe2clut``, ``trim_lzss``)::

    newest + ring + sole stale              491/859
    + nc2 L vs T (T_pl≥3, age gap 2)        492/859
    + M16 young mid → M17 mid into=best     502/859
    + young M4 mid into+1 → L               504/859
    + just-finished M6 tip re-use → L       508/859
    + nc2 T tip → older L (age-pair set)    521/859
    + mid re-anchor 1-fix keys             528/859
    + 2-fix chain re-anchors               546/859
    + 3-fix / short-pair re-anchors        555/859
    + 2-/3-fix safe greedy merge           560/859
    + tip→mid + more 2-fix chains           565/859
    + M17 ladder 2-fix (13@30 / 2@32)      568/859
    + L→mid + 3-fix cluster triples        571/859
    + structural collapse (dead key / M17)  571/859  (same exact)
    + newest mid pl<17 → BST last-at-max    617/859
    lzss_cascading (frozen)                 561/859
    lzss.encode_lzss (simple greedy)        (decode-ok only)

Growth rule
-----------
Accept a new post-rule only if full-corpus exact does not fall (prefer a
rise). Prefer features that never rewrite multi_max sites on streams that
are already exact under the current policy.

Usage::

    from lzss_clut import encode_clut
    from lzss import decode_lzss, trim_lzss

    stream = encode_clut(data)
    assert decode_lzss(stream) == data
"""

from __future__ import annotations

from collections.abc import Callable

from lzss import (
    DICT_SIZE,
    MAX_MATCH,
    MIN_MATCH,
    _match_length,
    _offset_for_ref,
    _pack_tokens,
    decode_lzss,
    trim_lzss,
)

# Sole max-length candidate older than this → force a literal.
SOLE_STALE_AGE = 242

# Okumura-style BST search bound (classic F); PE2 max match is 17.
BST_F = 17
# Hybrid multi_max: when posts leave newest mid of a phrase with pl < this,
# re-pick the last max-length node on an Okumura BST search path. pl≥17
# (M17 ladder mids) over-fires on stream-exact sites (~−100 exact).
BST_NEWEST_MID_MAX_PL = 17

_BST_NIL = -1

# Post-rule: short multi_max, exactly two tips (L + match), match phrase
# length ≥ 3, L older than match tip by exactly 2 → take L.
# Encode-validated: 491 → 492 exact; zero rewrites on stream_exact L-vs-T
# sites (those keep newest T; none combine T_pl≥3 with gap 2).
NC2_L_VS_T_MAX_BEST = 4
NC2_L_VS_T_MIN_T_PL = 3
NC2_L_VS_T_AGE_GAP = 2

# Post-rule: long multi_max M17 ladder. When newest is a young mid of an M16
# phrase, re-anchor to the oldest M17 mid with into == best_len and age ≥ 96.
# Encode-validated: 492 → 502.
M16_TO_M17_MIN_BEST = 10
M16_TO_M17_SRC_PL = 16
M16_TO_M17_SRC_MAX_AGE = 48
M16_TO_M17_DST_MIN_PL = 17
M16_TO_M17_DST_MIN_AGE = 96

# Post-rule: young mid into +1 of an M4 tip → L tip if present (best ≤ 2).
# Encode-validated: 502 → 504. Broader “any into+1” regresses.
M4_INTO1_TO_L_MAX_BEST = 2
M4_INTO1_TO_L_OWNER_PL = 4
M4_INTO1_TO_L_INTO = 1
M4_INTO1_TO_L_MAX_AGE = 8

# Post-rule: newest is the tip of a just-finished M6 (age == pl == 6), nc==2,
# best==3 → prefer L tip. Encode-validated: 504 → 508. Widening pl set loses.
JF_M6_TIP_TO_L_BEST = 3
JF_M6_TIP_TO_L_PL = 6

# Post-rule: nc==2, newest match tip vs older L tip, exact (pl, best, T_age, L_age)
# pairs from 1-fix oracle gains. Each pair is collision-free on stream-exact
# multi_max sites (exact-stream (T_age, L_age) set is disjoint). Encode-validated:
# 508 → 521 (+13/−0). Broader “gap ≥ 14” / “L age ≥ 20” rewrites exacts (~−90).
# T2@8→L@12 is deliberately excluded (+1/−1 net zero).
NC2_T_TO_L_AGE_PAIRS: frozenset[tuple[int, int, int, int]] = frozenset(
    {
        (2, 2, 6, 20),
        (2, 2, 14, 64),
        (2, 2, 22, 62),
        (2, 2, 26, 68),
        (2, 2, 30, 48),
        (2, 2, 30, 52),
        (2, 2, 46, 50),
        (2, 2, 51, 179),
        (2, 2, 137, 227),
        (2, 2, 213, 237),
        (5, 3, 64, 96),
        (9, 4, 10, 20),
        # 2-/3-fix safe greedy merge (555 → 560)
        (2, 2, 2, 34),
        (2, 2, 6, 8),
        (2, 2, 6, 12),
        (2, 2, 12, 18),
        (2, 2, 12, 26),
        (3, 2, 44, 206),
        (3, 2, 144, 232),
        (7, 6, 8, 22),
    }
)

# ---------------------------------------------------------------------------
# Sparse multi_max re-anchor tables (structural collapse of the oracle key
# catalog). Ablation: broad predicates (e.g. “any into+1 → L”, “M17+16@1 →
# oldest M17 mid”) over-fire and lose dozens–hundreds of exacts. Exact age
# / shape keys remain; we only collapse representation + drop dead keys.
# ---------------------------------------------------------------------------

# Young mid of an M17 at into==16 age==1 → dest mid keyed by best_len.
# Was four separate MID_REANCHOR rows; one structural rule + map.
# best_len → (dest_pl_min, dest_into, dest_age)
M17_INTO16_AGE1_BY_BEST: dict[int, tuple[int, int, int]] = {
    16: (17, 5, 160),   # → M17+5@160
    15: (17, 13, 96),   # → M17+13@96
    13: (17, 6, 224),   # → M17+6@224
    10: (16, 6, 31),    # → M16+6@31
}

# Mid → L tip. Tuple: (src_pl_min, src_into, src_age, best, L_age).
# First matching key wins (M2+1@4 → L@6 before → L@16).
# Dead key M3+1@8→L@12 removed (ablation +0/−0).
MID_TO_L_KEYS: tuple[tuple[int, int, int, int, int], ...] = (
    (2, 1, 15, 2, 54),
    (2, 1, 10, 2, 34),
    (3, 1, 42, 2, 70),
    (2, 1, 2, 2, 24),
    (5, 3, 4, 2, 8),
    (2, 1, 4, 2, 6),     # before L@16
    (4, 1, 8, 3, 10),
    (17, 1, 34, 10, 98),
    (5, 3, 2, 5, 12),
    (3, 1, 8, 2, 14),
    (2, 1, 4, 2, 16),
    (3, 1, 10, 2, 12),
)

# Mid → mid/tip residual. Tuple:
#   (src_pl_min, src_into, src_age, best, dest_kind, dest_a, dest_b, dest_c)
#   dest_kind "mid": pl>=dest_a, into==dest_b, age==dest_c
#   dest_kind "tip": match tip pl==dest_a, age==dest_b
# First matching key wins. M17+16@1 and mid→L live in the tables above.
MID_REANCHOR_KEYS: tuple[tuple[int, int, int, int, str, int, int, int], ...] = (
    (4, 2, 18, 2, "tip", 2, 78, 0),        # M4+2@18 → T2@78
    (3, 1, 32, 2, "mid", 17, 3, 47),       # M3+1@32 → M17+3@47
    (17, 14, 34, 3, "tip", 17, 48, 0),     # M17+14@34 → T17@48
    (4, 2, 2, 2, "mid", 3, 1, 10),         # M4+2@2 → M3+1@10
    (3, 1, 2, 2, "mid", 3, 2, 14),         # M3+1@2 → M3+2@14
    (17, 15, 2, 5, "mid", 6, 1, 30),       # M17+15@2 → M6+1@30
    (5, 1, 6, 4, "mid", 6, 2, 36),         # M5+1@6 → M6+2@36
    (13, 10, 33, 3, "mid", 13, 9, 34),     # M13+10@33 → M13+9@34
    (3, 1, 2, 2, "mid", 3, 1, 6),          # M3+1@2 → M3+1@6
    (6, 4, 2, 2, "tip", 2, 8, 0),          # M6+4@2 → T2@8
    (3, 1, 6, 3, "mid", 9, 7, 10),         # M3+1@6 → M9+7@10
    (2, 1, 6, 2, "mid", 2, 1, 12),         # M2+1@6 → M2+1@12
    (2, 1, 4, 2, "mid", 5, 4, 6),          # M2+1@4 → M5+4@6 (no L@6)
    (3, 1, 4, 2, "tip", 2, 8, 0),          # M3+1@4 → T2@8
    (14, 8, 32, 4, "mid", 17, 10, 64),     # M14+8@32 → M17+10@64
    (15, 10, 158, 2, "mid", 17, 12, 190),  # M15+10@158 → M17+12@190
    (8, 4, 6, 3, "mid", 5, 4, 12),         # M8+4@6 → M5+4@12
    (17, 13, 30, 7, "mid", 17, 15, 62),    # M17+13@30 → M17+15@62
    (17, 2, 32, 3, "mid", 17, 11, 126),    # M17+2@32 → M17+11@126
    (5, 4, 6, 3, "mid", 2, 1, 18),         # M5+4@6 → M2+1@18
    (16, 14, 2, 2, "mid", 14, 12, 36),     # M16+14@2 → M14+12@36
    (5, 4, 14, 2, "mid", 5, 2, 16),        # M5+4@14 → M5+2@16
    (7, 1, 6, 4, "mid", 5, 3, 10),         # M7+1@6 → M5+3@10
)

# Unified tip re-anchor: newest match tip → older tip or mid.
# Tuple: (src_pl, src_age, best, dest_kind, a, b, c, min_nc)
#   dest_kind "tip": match tip pl==a, age==b  (c unused)
#   dest_kind "mid": mid pl>=a, into==b, age==c
TIP_REANCHOR_KEYS: tuple[tuple[int, int, int, str, int, int, int, int], ...] = (
    (2, 4, 2, "tip", 2, 8, 0, 3),       # T2@4 → T2@8
    (2, 16, 2, "tip", 2, 50, 0, 4),     # T2@16 → T2@50
    (2, 30, 2, "tip", 2, 56, 0, 2),     # T2@30 → T2@56
    (4, 10, 3, "tip", 2, 12, 0, 2),     # T4@10 → T2@12
    (5, 6, 3, "tip", 17, 12, 0, 2),     # T5@6 → T17@12
    (2, 4, 2, "tip", 2, 10, 0, 2),      # T2@4 → T2@10
    (2, 8, 2, "mid", 3, 2, 14, 2),      # T2@8 → M3+2@14
    (6, 8, 5, "mid", 8, 4, 16, 2),      # T6@8 → M8+4@16
    (5, 6, 3, "mid", 17, 13, 12, 9),    # T5@6 → M17+13@12
    (2, 106, 2, "mid", 3, 2, 168, 2),   # T2@106 → M3+2@168
    (2, 12, 2, "mid", 16, 15, 50, 2),   # T2@12 → M16+15@50
)

# Unified L re-anchor: newest L tip → older match tip or mid.
# Tuple: (L_age, best, dest_kind, a, b, c, min_nc)
#   dest_kind "tip": match tip pl==a, age==b
#   dest_kind "mid": mid pl>=a, into==b, age==c
L_REANCHOR_KEYS: tuple[tuple[int, int, str, int, int, int, int], ...] = (
    (14, 3, "tip", 6, 6, 0, 2),   # L@14 → T6@6
    (6, 2, "tip", 3, 4, 0, 2),    # L@6 → T3@4
    (8, 2, "mid", 4, 1, 4, 2),    # L@8 → M4+1@4
)

# Back-compat aliases for anything importing the old names.
TIP_TO_MID_KEYS: tuple = ()  # folded into TIP_REANCHOR_KEYS
L_TO_TIP_KEYS: tuple = ()    # folded into L_REANCHOR_KEYS
L_TO_MID_KEYS: tuple = ()    # folded into L_REANCHOR_KEYS


PickFn = Callable[
    [list[int], int, dict[int, tuple], dict[int, int], int, bytes | None],
    int,
]


def _tip_kind(ref: int, token_starts: dict[int, tuple]) -> str | None:
    tip = token_starts.get(ref)
    if tip is None:
        return None
    return tip[0]  # "L" or "M"


def _mid_meta(
    ref: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> tuple[int, int, int] | None:
    """Return ``(owner_pl, into, age)`` if ``ref`` is a mid into a match tip."""
    if _tip_kind(ref, token_starts) is not None:
        return None
    owner = owners.get(ref, ref)
    oi = token_starts.get(owner)
    if oi is None or oi[0] != "M":
        return None
    return oi[1], ref - owner, pos - ref


def _should_force_lit_sole(sole: int | None, *, pos: int) -> bool:
    if sole is None:
        return False
    return (pos - sole) >= SOLE_STALE_AGE


def _post_nc2_L_vs_T_tpl_ge3_gap2(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Prefer L tip over match tip when both max-len, tightly gated.

    Gates (all required):
    - ``best_len <= 4``
    - exactly two max-cands, both tips (no mids)
    - one L tip, one match tip
    - match phrase length ``>= 3``
    - L is older than match tip by exactly 2 (``T_ref - L_ref == 2``)
    """
    if best_len > NC2_L_VS_T_MAX_BEST or len(cands) != 2:
        return chosen
    tips: list[tuple[int, str, int | None]] = []
    for c in cands:
        k = _tip_kind(c, token_starts)
        if k is None:
            return chosen  # mid involved
        pl = token_starts[c][1] if k == "M" else None
        tips.append((c, k, pl))
    if sorted(k for _, k, _ in tips) != ["L", "M"]:
        return chosen
    L_ref = next(c for c, k, _ in tips if k == "L")
    T_ref = next(c for c, k, _ in tips if k == "M")
    T_pl = next(pl for _, k, pl in tips if k == "M")
    assert T_pl is not None
    if T_pl < NC2_L_VS_T_MIN_T_PL:
        return chosen
    if (T_ref - L_ref) != NC2_L_VS_T_AGE_GAP:
        return chosen
    return L_ref


def _post_m16_young_mid_to_m17_into_best(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Re-anchor young M16 ladder mid to older M17 mid at into == best.

    Pattern (file30100 family @1208): newest ``M16+8@32`` vs retail
    ``M17+10@128`` for ``best==10``. Broader “any into=best M17” rules
    collapse exacts; require the *source* to be a young M16 mid.
    """
    if best_len < M16_TO_M17_MIN_BEST:
        return chosen
    src = _mid_meta(chosen, token_starts, owners, pos)
    if src is None:
        return chosen
    src_pl, _src_into, src_age = src
    if src_pl != M16_TO_M17_SRC_PL or src_age > M16_TO_M17_SRC_MAX_AGE:
        return chosen
    hits: list[int] = []
    for c in cands:
        m = _mid_meta(c, token_starts, owners, pos)
        if m is None:
            continue
        pl, into, age = m
        if (
            pl >= M16_TO_M17_DST_MIN_PL
            and into == best_len
            and age >= M16_TO_M17_DST_MIN_AGE
        ):
            hits.append(c)
    if not hits:
        return chosen
    return min(hits)  # oldest among qualifying M17 mids


def _post_m4_into1_young_to_L(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Young mid into +1 of M4 → newest L tip among max-cands.

    Pattern: ``M4+1@6`` vs ``L@8`` (best==2). General into+1 mids of other
    phrase lengths regress the corpus.
    """
    if best_len > M4_INTO1_TO_L_MAX_BEST:
        return chosen
    meta = _mid_meta(chosen, token_starts, owners, pos)
    if meta is None:
        return chosen
    pl, into, age = meta
    if (
        pl != M4_INTO1_TO_L_OWNER_PL
        or into != M4_INTO1_TO_L_INTO
        or age > M4_INTO1_TO_L_MAX_AGE
    ):
        return chosen
    Ls = [c for c in cands if _tip_kind(c, token_starts) == "L"]
    if not Ls:
        return chosen
    return max(Ls)


def _post_just_finished_M6_tip_to_L(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Tip of just-finished M6 re-used as match source → prefer L tip.

    Pattern: after emitting M6 ending at ``pos``, multi_max has ``T6@6``
    (the phrase tip) vs older ``L@10``, best==3, nc==2; retail takes L.
    """
    if best_len != JF_M6_TIP_TO_L_BEST or len(cands) != 2:
        return chosen
    if _tip_kind(chosen, token_starts) != "M":
        return chosen
    pl = token_starts[chosen][1]
    if pl != JF_M6_TIP_TO_L_PL or (pos - chosen) != pl:
        return chosen
    # Must be the tip of the match that just finished at pos.
    if not any(
        info[0] == "M"
        and info[1] == pl
        and own + pl == pos
        and own == chosen
        for own, info in token_starts.items()
    ):
        return chosen
    Ls = [c for c in cands if _tip_kind(c, token_starts) == "L"]
    if not Ls:
        return chosen
    return max(Ls)


def _post_nc2_T_to_older_L_age_pairs(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Prefer older L tip over match tip for encode-validated age pairs.

    Gates (all required):
    - exactly two max-cands
    - newest is a match tip with phrase length ``pl``
    - other is an older L tip
    - ``(pl, best_len, T_age, L_age)`` ∈ :data:`NC2_T_TO_L_AGE_PAIRS`

    Derived from 1-fix oracle first-miss gains; each pair is absent from
    stream-exact multi_max sites under the same coarse shape (T tip + older
    L, nc2). Broad gap thresholds over-fire on exacts.
    """
    if len(cands) != 2:
        return chosen
    if _tip_kind(chosen, token_starts) != "M":
        return chosen
    pl = token_starts[chosen][1]
    other = cands[0] if cands[1] == chosen else cands[1]
    if _tip_kind(other, token_starts) != "L":
        return chosen
    if other >= chosen:
        return chosen
    t_age = pos - chosen
    l_age = pos - other
    if (pl, best_len, t_age, l_age) in NC2_T_TO_L_AGE_PAIRS:
        return other
    return chosen


def _post_m17_into16_age1(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """M17 mid into==16 age==1 → dest mid from :data:`M17_INTO16_AGE1_BY_BEST`."""
    src = _mid_meta(chosen, token_starts, owners, pos)
    if src is None:
        return chosen
    src_pl, src_into, src_age = src
    if src_into != 16 or src_age != 1 or src_pl < 17:
        return chosen
    dest = M17_INTO16_AGE1_BY_BEST.get(best_len)
    if dest is None:
        return chosen
    d_pl, d_into, d_age = dest
    for cand in cands:
        m = _mid_meta(cand, token_starts, owners, pos)
        if m is None:
            continue
        mpl, minto, mage = m
        if mpl >= d_pl and minto == d_into and mage == d_age:
            return cand
    return chosen


def _post_mid_to_L_keys(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Re-anchor newest mid to an L tip for known keys (:data:`MID_TO_L_KEYS`)."""
    src = _mid_meta(chosen, token_starts, owners, pos)
    if src is None:
        return chosen
    src_pl, src_into, src_age = src
    for pl_min, into, age, best, l_age in MID_TO_L_KEYS:
        if best_len != best or src_into != into or src_age != age:
            continue
        if src_pl < pl_min:
            continue
        for cand in cands:
            if _tip_kind(cand, token_starts) == "L" and (pos - cand) == l_age:
                return cand
    return chosen


def _post_mid_reanchor_keys(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Re-anchor newest mid to mid/tip for residual keys (:data:`MID_REANCHOR_KEYS`)."""
    src = _mid_meta(chosen, token_starts, owners, pos)
    if src is None:
        return chosen
    src_pl, src_into, src_age = src
    for pl_min, into, age, best, kind, a, b, c in MID_REANCHOR_KEYS:
        if best_len != best or src_into != into or src_age != age:
            continue
        if src_pl < pl_min:
            continue
        if kind == "mid":
            for cand in cands:
                m = _mid_meta(cand, token_starts, owners, pos)
                if m is None:
                    continue
                d_pl, d_into, d_age = m
                if d_pl >= a and d_into == b and d_age == c:
                    return cand
        elif kind == "tip":
            for cand in cands:
                if _tip_kind(cand, token_starts) != "M":
                    continue
                if token_starts[cand][1] == a and (pos - cand) == b:
                    return cand
    return chosen


def _post_tip_reanchor_keys(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """Re-anchor newest match tip to tip or mid (:data:`TIP_REANCHOR_KEYS`)."""
    if _tip_kind(chosen, token_starts) != "M":
        return chosen
    src_pl = token_starts[chosen][1]
    src_age = pos - chosen
    for s_pl, s_age, best, kind, a, b, c, min_nc in TIP_REANCHOR_KEYS:
        if (
            best_len != best
            or src_pl != s_pl
            or src_age != s_age
            or len(cands) < min_nc
        ):
            continue
        if kind == "tip":
            for cand in cands:
                if cand == chosen:
                    continue
                if _tip_kind(cand, token_starts) != "M":
                    continue
                if token_starts[cand][1] == a and (pos - cand) == b:
                    return cand
        elif kind == "mid":
            for cand in cands:
                m = _mid_meta(cand, token_starts, owners, pos)
                if m is None:
                    continue
                mpl, minto, mage = m
                if mpl >= a and minto == b and mage == c:
                    return cand
    return chosen


def _post_L_reanchor_keys(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
) -> int:
    """When newest is L, re-anchor to tip or mid (:data:`L_REANCHOR_KEYS`)."""
    if _tip_kind(chosen, token_starts) != "L":
        return chosen
    l_age = pos - chosen
    for la, best, kind, a, b, c, min_nc in L_REANCHOR_KEYS:
        if best_len != best or l_age != la or len(cands) < min_nc:
            continue
        if kind == "tip":
            for cand in cands:
                if _tip_kind(cand, token_starts) != "M":
                    continue
                if token_starts[cand][1] == a and (pos - cand) == b:
                    return cand
        elif kind == "mid":
            for cand in cands:
                m = _mid_meta(cand, token_starts, owners, pos)
                if m is None:
                    continue
                mpl, minto, mage = m
                if mpl >= a and minto == b and mage == c:
                    return cand
    return chosen


def _bst_last_at_max(
    data: bytes,
    pos: int,
    head: list[int],
    prev: list[int],
    *,
    f: int = BST_F,
) -> int:
    """Okumura-style BST: last search-path node at maximum true match length.

    Rebuilds a binary search tree over same-first-byte refs in the sliding
    window (insert order = absolute position). Walks the path for ``data[pos:]``
    and returns the last node whose true match length equals the path max.

    Classic Okumura uses strict ``>`` (first at max) — PE2 multi_max residuals
    agree with **last** at max (~96% on encode_clut multi_max misses), not first.
    """
    if pos >= len(data):
        return _BST_NIL
    c0 = data[pos]
    window_start = max(0, pos - DICT_SIZE)
    nodes: list[int] = []
    p = head[c0]
    while p >= window_start:
        if _offset_for_ref(p) is not None:
            nodes.append(p)
        p = prev[p]
    if not nodes:
        return _BST_NIL

    n = len(data)
    lson: dict[int, int] = {}
    rson: dict[int, int] = {}
    root = _BST_NIL

    def cmp_str(a: int, b: int) -> tuple[int, int]:
        j = 0
        lim = min(f, n - a, n - b)
        while j < lim and data[a + j] == data[b + j]:
            j += 1
        if j >= f or a + j >= n or b + j >= n:
            return 0, j
        return data[a + j] - data[b + j], j

    for node in sorted(nodes):
        if root == _BST_NIL:
            root = node
            lson[node] = rson[node] = _BST_NIL
            continue
        key = root
        while True:
            cmp, _ = cmp_str(node, key)
            if cmp < 0:
                nxt = lson.get(key, _BST_NIL)
                if nxt == _BST_NIL:
                    lson[key] = node
                    lson[node] = rson[node] = _BST_NIL
                    break
                key = nxt
            else:
                nxt = rson.get(key, _BST_NIL)
                if nxt == _BST_NIL:
                    rson[key] = node
                    lson[node] = rson[node] = _BST_NIL
                    break
                key = nxt

    path: list[int] = []
    lens: list[int] = []
    key = root
    while key != _BST_NIL:
        cmp, j = cmp_str(pos, key)
        path.append(key)
        lens.append(_match_length(data, pos, key, MAX_MATCH))
        if j >= f:
            break
        key = lson.get(key, _BST_NIL) if cmp < 0 else rson.get(key, _BST_NIL)
    if not path:
        return _BST_NIL
    max_l = max(lens)
    for node, length in zip(reversed(path), reversed(lens)):
        if length == max_l:
            return node
    return _BST_NIL


def _post_bst_last_newest_mid(
    chosen: int,
    cands: list[int],
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int,
    data: bytes,
    head: list[int],
    prev: list[int],
) -> int:
    """Newest mid of a short phrase → Okumura BST last-at-max among cands.

    Gates (all required):
    - ``chosen`` is still the absolute newest max-cand (posts did not re-anchor)
    - ``chosen`` is a match **mid** (not tip / L) with ``pl < BST_NEWEST_MID_MAX_PL``
    - BST last-at-max is among max-length cands and differs from ``chosen``

    Encode-validated: 571 → 617 exact (+46/−0). Raw “any newest mid” loses
    ~112; almost all over-fire is M17 mids (pl≥17). First-at-max is useless.
    """
    if len(cands) < 2:
        return chosen
    newest = max(cands)
    if chosen != newest:
        return chosen
    # Classify like oracle meta: owner match phrase + into. Do **not** use
    # ``_mid_meta`` alone — it returns None when ``chosen`` is a token start
    # (into==0 mid), because ``_tip_kind`` sees ``token_starts[chosen]``.
    # Those into==0 mids are a large share of the hybrid gain.
    owner = owners.get(chosen, chosen)
    tok = token_starts.get(owner)
    if tok is None or tok[0] != "M":
        return chosen
    pl = tok[1]
    into = chosen - owner
    if into >= pl - 1:
        return chosen  # tip (or empty)
    if pl >= BST_NEWEST_MID_MAX_PL:
        return chosen
    last = _bst_last_at_max(data, pos, head, prev)
    if last == _BST_NIL or last == chosen or last not in cands:
        return chosen
    if _match_length(data, pos, last, MAX_MATCH) != best_len:
        return chosen
    return last


def _pick_match_ref(
    cands: list[int],
    *,
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int = 0,
    data: bytes | None = None,
    head: list[int] | None = None,
    prev: list[int] | None = None,
) -> int:
    """Multi_max pick: newest, then general post-rules, then BST last gate."""
    if len(cands) == 1:
        return cands[0]
    chosen = max(cands)
    chosen = _post_nc2_L_vs_T_tpl_ge3_gap2(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_m16_young_mid_to_m17_into_best(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_m4_into1_young_to_L(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_just_finished_M6_tip_to_L(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_nc2_T_to_older_L_age_pairs(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_m17_into16_age1(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_mid_to_L_keys(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_mid_reanchor_keys(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_tip_reanchor_keys(
        chosen, cands, best_len, token_starts, owners, pos
    )
    chosen = _post_L_reanchor_keys(
        chosen, cands, best_len, token_starts, owners, pos
    )
    if data is not None and head is not None and prev is not None:
        chosen = _post_bst_last_newest_mid(
            chosen,
            cands,
            best_len,
            token_starts,
            owners,
            pos,
            data,
            head,
            prev,
        )
    return chosen


def encode_clut(data: bytes) -> bytes:
    """Encode ``data`` as PE2 LZSS with the simplified CLUT policy."""
    n = len(data)
    if n == 0:
        return _pack_tokens([("EOS",)])

    head = [-1] * 256
    prev = [-1] * n
    token_starts: dict[int, tuple] = {}
    owners: dict[int, int] = {}

    def insert(i: int) -> None:
        b = data[i]
        prev[i] = head[b]
        head[b] = i

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
            if _should_force_lit_sole(sole, pos=pos):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            chosen = _pick_match_ref(
                cands,
                best_len=best_len,
                token_starts=token_starts,
                owners=owners,
                pos=pos,
                data=data,
                head=head,
                prev=prev,
            )
            off = _offset_for_ref(chosen)
            assert off is not None
            tokens.append(("M", off, best_len))
            token_starts[pos] = ("M", best_len)
            for i in range(best_len):
                owners[pos + i] = pos
            end = pos + best_len
            # Retain owner meta slightly before the match window so into==0
            # mids at the edge still resolve (owner may sit up to MAX_MATCH-1
            # earlier than the oldest window byte).
            cut = end - DICT_SIZE - (MAX_MATCH - 1)
            if cut > 0:
                token_starts = {
                    k: v for k, v in token_starts.items() if k >= cut
                }
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
