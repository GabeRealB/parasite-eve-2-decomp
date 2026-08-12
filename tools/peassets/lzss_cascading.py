"""Frozen multi_max / force-lit cascade encoder for PE2 LZSS identity work.

Soft-freeze
-----------
This module is a **frozen experiment** toward byte-identical re-encoding of
retail streams (``.pe2pkg``, image strips, CLUTs). The large ``_pick_match_ref``
cascade, sole/not_max force-lit gates, and related constants lock measured
exacts from identity mining. Do **not** expand or refactor casually.

* For **production / best-effort matching** (repack, images): ``lzss.encode_lzss``
  (``kind="clut"`` → ``lzss_clut``). Always decoder-compatible.
* For **CLUT identity detail**: ``lzss_clut.encode_clut``.
* For **historical cascade scores**: this module's ``encode_lzss``
  (alias ``encode_lzss_cascading``). Soft-freeze — do not expand casually.

Measure identity with ``lzss_identity_report.py`` (imports this encoder).

Shared format, decode, and pack live in ``lzss.py``.
"""

from __future__ import annotations

from collections import defaultdict

from lzss import (
    DICT_SIZE,
    LIT_SIZE,
    MAX_MATCH,
    MIN_MATCH,
    OFFSET_BITS,
    STRING_LEN_BITS,
    _match_length,
    _offset_for_ref,
    _pack_tokens,
)

LONG_MS_ABS_FLOOR = 14
# If the long-MS anchor is older than this, prefer a covers_pe mid over
# re-anchoring to the distant ladder (file30400 family vs file30102).
LONG_MS_COVERS_AGE = 32
# Medium MS age window when a same-len tip is "too young" (just emitted).
MEDIUM_MS_MAX_AGE = 24
# When the covers tip is younger than this, prefer a cover with age ≥
# OLD_COVER_MIN_AGE (re-anchor to an earlier phrase; file30400@153 / 30402@189).
YOUNG_COVER_MAX_AGE = 16
OLD_COVER_MIN_AGE = 96
# Owner-local AP period floor for short (best ≤ 2) multi-max ties.
OWNER_AP_MIN_PERIOD = 4
# Short-match re-anchor: tip is PE of a long MS and very young → cover of the
# most recent *prior* long MS with a different length (file30400@210: M13@27
# over PE of M17@193). Threshold 13 so M13 ladders qualify (14 would miss them).
PRIOR_LONG_MIN_PL = 13
PRIOR_LONG_TIP_AGE = 2
# Old cover of a *medium* owner (best < prev_len < long thr) can beat a
# same-len MS pick when the long-MS ladder is distant, or when a just-emitted
# same-len tip would otherwise lose to an older same-len / lit.
# file30400@213 (old ladder + M4 cover), file30402@277 (young same tip).
MID_COVER_MIN_AGE = 128
YOUNG_SAME_TIP_AGE = 6
SAME_MS_MIN_AGE_FOR_MID = 32
# Mid-age same-len MS (not just-emitted, not old) can beat a literal when no
# medium MS is present (file30400@240: M4@220 age 20 over L@216; @360 age 12).
# Very young same-len still loses to lit (file30102@137: L@129 over M3@133 age 4).
MID_SAME_MIN_AGE = 12
# If any same-len MS is younger than this, do not prefer old same-len over lit
# (file30400@244: L@64 over old M4@72 while young M4@236 exists).
YOUNG_SAME_BLOCKS_OLD = 12
# best==2: among covers of recent M3 phrases, prefer 2nd-newest
# (file30402@282: cover of M3@273 over cover of M3@277).
B2_M3_OWNER_MAX_AGE = 12
# Very old medium MS (age ≥ this) loses to a literal when both are tips
# (file30400@264: L@48 over M8@56 age 208).
OLD_MED_MIN_AGE = 200
# Full-phrase match-start (pl == best) with other cands inside that phrase:
# prefer newest inside (file30402@292: mid 156 over MS 148 of M17).
FULL_PHRASE_MIN_BEST = 6
# When the chosen tip is an old literal, prefer a medium-owner cover with the
# longest owner phrase (file30400@276: cover of M8@228 over L@76).
OLD_LIT_MIN_AGE = 160
OLD_LIT_COVER_MIN_AGE = 32
OLD_LIT_COVER_MAX_AGE = 64
# Literal tip vs medium-owner cover of age ≥ this: prefer cover
# (file30402@417: cover of M7@285 over L@281).
MIDCOV_OVER_LIT_MIN_AGE = 128
# best ≥ 7: among cands at offset +1 into an M4 phrase, prefer oldest
# (file30400@281: 237 over 265/257).
M4_PLUS1_MIN_BEST = 7
# Young cover tip (age < this) yields to the newest older cand
# (file30400@317: 253 over cover 293 age 24).
YOUNG_COVER_YIELD_AGE = 32
YOUNG_COVER_YIELD_MIN_BEST = 7
# Sole match candidate that is an old literal → force a literal instead
# (file30400@352: sole L@108 age 244; CLUT file20126@271: sole L@29 age 242).
SOLE_OLD_LIT_FORCE_AGE = 242
# not_max: sole best is old L tip (age ≥ 242), but retail emits shorter match from
# M4 tip age == 24 len 4 (file30406/07@324). Must run *before* sole-old-L force-lit.
NOT_MAX_OLD_LIT_TO_M4TIP24_LIT_MIN_AGE = 242
NOT_MAX_OLD_LIT_TO_M4TIP24_TIP_AGE = 24
NOT_MAX_OLD_LIT_TO_M4TIP24_TIP_PL = 4
NOT_MAX_OLD_LIT_TO_M4TIP24_EMIT_LEN = 4
# not_max: sole best old L tip, retail M3 tip age == 239 len 3 (file30200/01@340).
NOT_MAX_OLD_LIT_TO_M3TIP239_LIT_MIN_AGE = 242
NOT_MAX_OLD_LIT_TO_M3TIP239_TIP_AGE = 239
NOT_MAX_OLD_LIT_TO_M3TIP239_TIP_PL = 3
NOT_MAX_OLD_LIT_TO_M3TIP239_EMIT_LEN = 3
# not_max: sole best M3 tip age == 244, best_len == 4 → emit len 3 from M7 mid
# into +3 age == 176 (pe2pkg file50146@584). Path-clean on stage0 ≤4K.
NOT_MAX_M3TIP244_TO_M7INTO3_BEST = 4
NOT_MAX_M3TIP244_TO_M7INTO3_SRC_AGE = 244
NOT_MAX_M3TIP244_TO_M7INTO3_SRC_PL = 3
NOT_MAX_M3TIP244_TO_M7INTO3_DST_OWNER_PL = 7
NOT_MAX_M3TIP244_TO_M7INTO3_DST_INTO = 3
NOT_MAX_M3TIP244_TO_M7INTO3_DST_AGE = 176
NOT_MAX_M3TIP244_TO_M7INTO3_EMIT_LEN = 3
# not_max: sole M17 tip age == 244, best_len == 4, pre == 0 → emit len 3 from
# M17 mid into +5 age == 239 (pe2pkg file30300/01@568). Path-clean on stage0 ≤4K.
NOT_MAX_M17TIP244_TO_M17INTO5_BEST = 4
NOT_MAX_M17TIP244_TO_M17INTO5_SRC_AGE = 244
NOT_MAX_M17TIP244_TO_M17INTO5_SRC_PL = 17
NOT_MAX_M17TIP244_TO_M17INTO5_PRE = 0
NOT_MAX_M17TIP244_TO_M17INTO5_DST_OWNER_PL = 17
NOT_MAX_M17TIP244_TO_M17INTO5_DST_INTO = 5
NOT_MAX_M17TIP244_TO_M17INTO5_DST_AGE = 239
NOT_MAX_M17TIP244_TO_M17INTO5_EMIT_LEN = 3
# not_max: sole M7 mid into +3 age == 252, best_len == 4, pre == 163,
# just-finished M4 → emit len 3 from M3 mid into +2 age == 12
# (pe2pkg file50146@660). Path-clean on stage0 ≤4K.
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_BEST = 4
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_SRC_AGE = 252
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_SRC_OWNER_PL = 7
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_SRC_INTO = 3
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_PRE = 163
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_DST_OWNER_PL = 3
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_DST_INTO = 2
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_DST_AGE = 12
NOT_MAX_M7INTO3_252_TO_M3INTO2_12_EMIT_LEN = 3
# not_max: sole M5 mid into +4 age == 244, best_len ≥ 6, pre == 125,
# just-finished M2 → emit len 2 from M2 tip age == 198
# (pe2pkg file20600@586 — first encode not_max miss among impure ≤4K).
NOT_MAX_M5INTO4_244_TO_M2TIP198_MIN_BEST = 6
NOT_MAX_M5INTO4_244_TO_M2TIP198_SRC_AGE = 244
NOT_MAX_M5INTO4_244_TO_M2TIP198_SRC_OWNER_PL = 5
NOT_MAX_M5INTO4_244_TO_M2TIP198_SRC_INTO = 4
NOT_MAX_M5INTO4_244_TO_M2TIP198_PRE = 125
NOT_MAX_M5INTO4_244_TO_M2TIP198_DST_PL = 2
NOT_MAX_M5INTO4_244_TO_M2TIP198_DST_AGE = 198
NOT_MAX_M5INTO4_244_TO_M2TIP198_EMIT_LEN = 2
# Sole M2 tip age == 252, best_len == 4, pre == 40, just-finished M4 → force lit
# (pe2pkg file30200/01@368 — path miss, retail L vs sole match).
SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_AGE = 252
SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_BEST = 4
SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_PRE = 40
SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_PL = 2
# Sole M3 tip age == 252, best_len == 3, pre == 163, just-finished M2 → force lit
# (pe2pkg file50146@692 — path miss after R364).
SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_AGE = 252
SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_BEST = 3
SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_PRE = 163
SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_PL = 3
# not_max: sole M2 mid into +1 age == 244, best_len == 4, pre == 86,
# just-finished M2 → emit len 3 from M9 mid into +6 age == 164
# (pe2pkg file20600@588).
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_BEST = 4
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_SRC_AGE = 244
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_SRC_OWNER_PL = 2
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_SRC_INTO = 1
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_PRE = 86
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_DST_OWNER_PL = 9
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_DST_INTO = 6
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_DST_AGE = 164
NOT_MAX_M2INTO1_244_TO_M9INTO6_164_EMIT_LEN = 3
# Sole L tip, best_len == 2, age ≥ 240 → force lit (file104900/2@604 age 240;
# also pe2pkg). Lowering SOLE_OLD_LIT to 240 without best==2 regresses pe2pkg
# at best>2; age ≥ 242 any-best remains above.
SOLE_LIT_BEST2_FORCE_AGE = 240
SOLE_LIT_BEST2_FORCE_BEST = 2
# Sole match candidate that is an old M2 tip (age ≥ 242), best_len == 2 → force lit
# (file102000/1@402: sole M2@160 age 242; also pe2pkg file30500@2920).
# Broader age≥242 without best==2 / M2-tip gates is path-unsafe (same family
# @454 best4 age250 still matches).
SOLE_OLD_M2TIP_FORCE_AGE = 242
SOLE_OLD_M2TIP_FORCE_BEST = 2
SOLE_OLD_M2TIP_FORCE_PL = 2
# Sole match candidate that is an old mid (not tip), age ≥ 248, best_len == 2 → force lit
# (file102000/1@450: sole mid into+2 of M4 age 248; file20333@323 age 253;
# pe2pkg file30500@2704). Age ≥ 253 alone left 102000 family path-stuck.
SOLE_OLD_MID_FORCE_AGE = 248
SOLE_OLD_MID_FORCE_BEST = 2
# Sole mid into +1 of M4, age == 250, best_len == 7 → force lit
# (file102000/1@451: sole mid 201 of M4@200). Broader best>2 age≥248 is
# path-unsafe (same family @452 best6 still matches).
SOLE_OLD_M4INTO1_BEST7_FORCE_AGE = 250
SOLE_OLD_M4INTO1_BEST7_FORCE_BEST = 7
SOLE_OLD_M4INTO1_BEST7_FORCE_INTO = 1
SOLE_OLD_M4INTO1_BEST7_FORCE_OWNER_PL = 4
# Sole mid age == 250, best_len == 3 → force lit
# (file444000/4@819 / file403200/4@819: mid into+2 of M3). Age≥248@best3
# regresses pe2pkg and large CLUTs.
SOLE_OLD_MID_BEST3_FORCE_AGE = 250
SOLE_OLD_MID_BEST3_FORCE_BEST = 3
# Sole mid age == 248, best_len == 3 → force lit (file104900/2@793 family:
# mid into+1 of M2). Age ≥ 248@best3 still bad on pe2pkg/90000x.
SOLE_MID_AGE248_BEST3_FORCE_AGE = 248
SOLE_MID_AGE248_BEST3_FORCE_BEST = 3
# best ≥ 6, exactly two untyped cands, one cover one not → non-cover
# (file30402@478: mid 334 over cover 342).
PAIR_NONCOVER_MIN_BEST = 6
# Prior-long re-anchor of same phrase-length → newest old medium MS start
# instead (file30503@326: M6@182 over cover of M17@292).
PRIOR_SAME_PL_MS_MIN_AGE = 144
PRIOR_SAME_PL_MS_MIN_PL = 6
# best==5: among phrase-ends of M3 owners, prefer oldest
# (file30400@383: pe 347 over pe 359/371).
OLDEST_PE3_BEST = 5
# Young long-cover tip (best ≥ 6): re-anchor to 2nd-newest long cover's
# owner + best//2 (file30503@330: 295 over cover 320).
SECOND_LONG_HALF_MIN_BEST = 6
SECOND_LONG_HALF_THR = 14
SECOND_LONG_HALF_YOUNG = 16
# Mid inside a long owner phrase → short-owner cover instead:
# best==3 mid of pl≥8 → newest M4 cover age≥144 (file30400@389)
# best==2 mid of pl≥7 → newest M3 cover age≥128 (file30402@562)
MID_LONG_TO_M4_PL = 8
MID_LONG_TO_M4_AGE = 144
MID_LONG_TO_M3_PL = 7
MID_LONG_TO_M3_AGE = 128
# When pick is same-len MS (best ≥ 4), prefer cover of most recent MS with
# pl == best+1 if that cover is younger (file30400@396: cover of M5@383).
YOUNG_PREV_COVER_MIN_BEST = 4
YOUNG_PREV_COVER_MAX_AGE = 16
YOUNG_PREV_MS_MAX_AGE = 24
# Two old untyped covers → older (file30503@336: 192 over 200).
PAIR_OLDER_COVER_MIN_BEST = 4
PAIR_OLDER_COVER_MIN_AGE = 96
# Two cands inside same long owner (best 5..8) → older
# (file30400@401: 182 over 190 of M17@176).
SAME_OWNER_OLDER_MIN_BEST = 5
SAME_OWNER_OLDER_MAX_BEST = 8
SAME_OWNER_OLDER_MIN_PL = 14
SAME_OWNER_OLDER_MIN_AGE = 96
# Very young phrase-end of a same-len owner → cover of a recent longer MS
# (file30503@340: cover of M6@330 over pe of M4@336).
YOUNG_PE_MID_COVER_MIN_BEST = 4
YOUNG_PE_MAX_AGE = 2
YOUNG_PE_COVER_MAX_AGE = 16
YOUNG_PE_MID_PL_MIN = 5
# Two cands: typed mid-MS vs untyped cover → cover
# (file30503@344: cover of M8@204 over mid M17@216).
MID_VS_COVER_MIN_BEST = 4
# best==3: among mid-MS of the same pl, prefer newest
# (file30400@413: M4@408 over older M4@396).
NEWER_SAME_MID_BEST = 3
# Untyped mid of a medium owner → oldest long cover (age ≥ 128)
# (file30400@424: cover 189 of M17@176 over mid 332 of M10@328).
MID_MED_TO_LONG_COV_MIN_BEST = 4
MID_MED_TO_LONG_COV_MIN_AGE = 128
# Old long cover → newest mid of a medium owner
# (file30400@429: mid 329 of M10 over cover 203 of M17).
LONG_COV_TO_MID_MIN_BEST = 6
LONG_COV_TO_MID_MIN_AGE = 128
# Mid-age same-len pe → mid-owner cover:
# best ≥ 4: newest cover age in [24, 64) (file30503@372: 332 of M6)
# best ≥ 3: else newest cover age ≥ 128 (file30503@413: 269 of M5)
# best == 3: else newest cover age in [24, 64)
PE_MID_COVER_MIN_BEST = 3
PE_MID_COVER_PE_MIN_AGE = 16
PE_MID_COVER_PE_MAX_AGE = 48
PE_MID_COVER_YOUNG_MIN = 24
PE_MID_COVER_YOUNG_MAX = 64
PE_MID_COVER_OLD_MIN = 128
# Young cover of longer owner → mid of medium owner with pl ≥ 8, age ≥ 96
# (file30400@437: mid 333 of M10 over young cover 433 of M7@429).
YOUNG_COV_TO_MID8_MIN_BEST = 3
YOUNG_COV_TO_MID8_MAX_AGE = 8
YOUNG_COV_TO_MID8_OWNER_PL = 8
YOUNG_COV_TO_MID8_MID_AGE = 96
# Long cover → pe of short owner (pl ≤ 2) age in [32, 96)
# (file30503@385: pe 329 of M2 over cover 319 of M17).
LONG_COV_TO_SHORT_PE_MIN_BEST = 6
LONG_COV_TO_SHORT_PE_MIN_AGE = 32
LONG_COV_TO_SHORT_PE_MAX_AGE = 96
LONG_COV_TO_SHORT_PE_MAX_PL = 2
# Two cands: pe of short (pl ≤ 2) loses to mid of long owner age ≥ 128
# (file30503@407: mid 231 of M17 over pe 327 of M2).
SHORT_PE_VS_LONG_MID_MIN_BEST = 3
SHORT_PE_VS_LONG_MID_MIN_AGE = 128
# Mid-age same-len pe (age in [48, 64)) → cover of most recent MS with
# pl ≥ best+4 (file30503@421: cover of M7@385 over pe of M3@369).
PE_RECENT_LONG_MIN_BEST = 3
PE_RECENT_LONG_PE_MIN_AGE = 48
PE_RECENT_LONG_PE_MAX_AGE = 64
PE_RECENT_LONG_PL_ADD = 4
PE_RECENT_LONG_COV_MIN_AGE = 24
PE_RECENT_LONG_COV_MAX_AGE = 48
# best ≤ 2: very young pe of M4 → newest same-pl cover age ≥ 64
# (file30503@446: cover 382 of M4@380 over pe 445 of M4@442).
YOUNG_PE4_TO_COV_MAX_BEST = 2
YOUNG_PE4_MAX_AGE = 2
YOUNG_PE4_OWNER_PL = 4
YOUNG_PE4_COV_MIN_AGE = 64
# best ≤ 2: long MS tip → oldest untyped mid of a long owner
# (file30503@460: mid 234 of M17@233 over tip M17@309).
LONG_MS_TO_OLD_MID_MAX_BEST = 2
# best ≤ 2: young cover of pl > best+1 → newest cover of pl == best+1
# (file30503@462: cover 408 of M3@407 over cover 444 of M4@442).
YOUNG_COV_TO_SHORT_COV_MAX_BEST = 2
YOUNG_COV_TO_SHORT_COV_MAX_AGE = 24
YOUNG_COV_TO_SHORT_COV_MIN_AGE = 32
# Old same-len pe (age ≥ 96) → newest cover of pl == best+1 with age ≥ pe_age
# (file30503@481: cover 365 of M4@364 over pe 371 of M3@369).
OLD_PE_TO_NEXT_COV_MIN_BEST = 3
OLD_PE_TO_NEXT_COV_MIN_AGE = 96
# best==3: very old M4 cover (age ≥ 176) after mid-long redirect, with a
# long mid still in cands → oldest same-len MS age ≥ 96
# (file30503@557: same@349 over M4 cover 377; gated so file30400@389
# M4 cover age 144 still wins).
M4COV_TO_OLD_SAME_BEST = 3
M4COV_TO_OLD_SAME_MIN_AGE = 176
M4COV_TO_OLD_SAME_MS_AGE = 96
# Mid of pl ∈ [8, thr) → newest same-len MS age < 48
# (file30503@585: same M3@557 over mid of M8@352 from young-cover yield).
MID8_TO_SAME_MIN_BEST = 3
MID8_TO_SAME_OWNER_PL = 8
MID8_TO_SAME_MAX_AGE = 48
# best ≤ 2: young cover of pl == best+1 → longer medium cover age ≥ 96
# (file30503@590: cover of M11@485 over young cover of M3@585).
YOUNG_SHORT_COV_TO_MED_MAX_BEST = 2
YOUNG_SHORT_COV_MAX_AGE = 8
YOUNG_SHORT_COV_MED_MIN_AGE = 96
# best==2, exactly lit+same: old lit (age ≥ 160) beats same when same age < 190
# (file30503@632: L@410 over M2@448; gate keeps @646 same@450 over L@420).
B2_LIT_OVER_SAME_LIT_MIN_AGE = 160
B2_LIT_OVER_SAME_SAME_MAX_AGE = 190
# best ≤ 2: young pe of M2 → newest M4 cover age ≥ 68
# (file30503@634: cover 566 of M4@564 over pe 633 of M2@632).
YOUNG_PE2_TO_M4_MAX_BEST = 2
YOUNG_PE2_MAX_AGE = 2
YOUNG_PE2_COV_MIN_AGE = 68
# best==2: mid of medium owner → newest same-len MS age ≥ 160
# (file30503@644: same M2@462 over mid of M6@550).
B2_MID_TO_OLD_SAME_MIN_AGE = 160
# best==2, two same-len MS (one young age < 64, one age ≥ 200) → oldest old
# (file30503@674: M2@448 over young M2@632).
B2_SAME_PAIR_YOUNG_MAX = 64
B2_SAME_PAIR_OLD_MIN = 200
# best==3: mid of M8 (age ≥ 128) after a young cover tip → M7 cover age ≥ 96
# (file30503@781: cover 669 of M7@665 over mid 637 of M8@636).
MID8_YOUNG_COV_TO_M7_BEST = 3
MID8_YOUNG_COV_MID_MIN_AGE = 128
MID8_YOUNG_COV_TIP_MAX_AGE = 8
MID8_YOUNG_COV_M7_MIN_AGE = 96
# Young same-len MS (age < 6) → cover of pl == 2×best age < 16
# (file30503@785: cover of M6@774 over just-emitted same M3@781).
YOUNG_SAME_TO_DOUBLE_COV_MIN_BEST = 3
YOUNG_SAME_TO_DOUBLE_COV_MAX_AGE = 6
YOUNG_SAME_TO_DOUBLE_COV_COVER_MAX = 16
# best ≥ 6: mid of short owner (pl ≤ 2) → young same-len MS tip age < 32
# (file30503@813: same M7@789 over mid of M2@632 after young-cover yield).
MID_SHORT_TO_SAME_MIN_BEST = 6
MID_SHORT_OWNER_MAX_PL = 2
MID_SHORT_TO_SAME_MAX_AGE = 32
# Young med MS (age < 16) → mid-age lit (age in [24, 64))
# (file30503@840: L@800 over M8@832; does not break file30400@129).
YOUNG_MED_TO_LIT_MIN_BEST = 3
YOUNG_MED_TO_LIT_MED_MAX_AGE = 16
YOUNG_MED_TO_LIT_LIT_MIN_AGE = 24
YOUNG_MED_TO_LIT_LIT_MAX_AGE = 64
# best ≤ 2: mid of medium age < 64 → old med MS tip age ≥ 200
# (file30503@856: M8@636 over mid of M7@813).
B2_MID_TO_OLD_MED_MAX_AGE = 64
B2_MID_TO_OLD_MED_MIN_AGE = 200
# best ≥ 4, two cands: mid inside long MS + that tip → tip if tip age ≥ 160
# (file30503@864: M17@676 over mid 688).
MID_LONG_TO_TIP_MIN_BEST = 4
MID_LONG_TO_TIP_MIN_AGE = 160
# best ≥ 4, two untyped covers: older age ≥ 160 beats younger age < 96
# (file30503@868: cover of M7@665 over cover of M6@774).
PAIR_OLDER_COVER_LOOSE_MIN_BEST = 4
PAIR_OLDER_COVER_LOOSE_OLD_AGE = 160
PAIR_OLDER_COVER_LOOSE_YOUNG_AGE = 96
# best==2, AP multi-max, sole same MS age ≥ 144 → newest M3 cover
# (file30503@922/@1066: cover of M3 over old same M2).
AP_SAME_TO_M3_MIN_AGE = 144
# best≤2: young pe of long MS re-anchored to different long cover →
# old med MS tip age ≥ 200, pl in [6, 13) (file30503@974: M6@774).
PRIOR_LONG_DIFF_TO_MED_MIN_AGE = 200
PRIOR_LONG_DIFF_MED_PL_MIN = 6
# best==3: M4 cover age ≥ 148 after mid-long redirect → M7 MS age ≥ 176
# (file30503@977: M7@801; floor 148 preserves file30400@389 age 144).
M4COV_TO_M7_MS_MIN_AGE = 148
M4COV_TO_M7_MS_TIP_AGE = 176
# Young-same branch picked M4 cover → M7 cover age ≥ 160
# (file30503@981: cover of M7@813 over M4@840).
YOUNG_SAME_M4_TO_M7_MIN_AGE = 128
YOUNG_SAME_M7_COV_MIN_AGE = 160
# mid_same age in [12, 24) → young mid of long pl ≥ 11 age < 16
# (file30503@1001: mid of M11@989 over same@985).
MID_SAME_TO_LONG_MID_MIN = 12
MID_SAME_TO_LONG_MID_MAX = 24
MID_SAME_TO_LONG_MID_PL = 11
MID_SAME_TO_LONG_MID_AGE = 16
# best ≥ 4: 2+ untyped mids of same long owner → oldest age ≥ 64
# (file30503@1026: mid 941 over 949 of M17@940).
SAME_LONG_MIDS_MIN_BEST = 4
SAME_LONG_MIDS_MIN_AGE = 64
# Late stream (pos ≥ 1000): 2-cand lit + young same → same
# (file30503@1073; safe for short streams / file30102@137).
LATE_LIT_TO_SAME_MIN_POS = 1000
LATE_LIT_TO_SAME_SAME_MAX = 12
LATE_LIT_TO_SAME_LIT_MAX = 16
# Late stream: multiple old same-len MS → newest (not oldest)
# (file30503@1077: same@929 over @925; pos gate keeps file30402@273).
LATE_OLD_SAME_NEWEST_MIN_POS = 1000
# 2-cand lit + young same-len MS: if the last byte before the same MS
# (data[same-1]) is ≤ this, prefer same; else lit. Separates file30102@137
# (byte 92 → lit) from file30400@449 / file30402@569 (bytes 8/12 → same).
# Measured over stage0 pe2pkg: all 12 retail-same cases have X≤36; all 34
# retail-lit cases have X≥44.
LIT_SAME_PREBYTE_SAME_MAX = 36
LIT_SAME_SAME_MAX_AGE = 12
LIT_SAME_LIT_MAX_AGE = 16
# AP multi-max: if AP 2nd is a medium MS tip and ≥3 same-len MS exist,
# prefer newest same (file30402@573: same@425 over AP 2nd-same@421).
AP_MED_TIP_NEWEST_SAME_MIN = 3
# lit + ≥2 old same + M4 cover → newest old same
# (file30402@705: same@569 over min@565).
LIT_OLD_SAME_M4_MIN_SAME = 2
# M4 cover from young-same+old-same branch → young same tip(s):
# one young → that tip; two+ young age < 12 → 2nd-newest
# (file30402@709/@713).
M4COV_YOUNG_SAME_MAX_AGE = 12
# Young PE of a long MS (age < this) → older long mid just-before-PE
# (off_into == pl-2) with age ≥ 64; newest such
# (file30402@761: mid 667 of M17@652 over PE 757 of M17@741).
YOUNG_LONG_PE_MAX_AGE = 8
YOUNG_LONG_PE_MID_MIN_AGE = 64
# Sole young same-len tip + ≥2 old mid covers → newest mid cover
# (file30402@765: cover 633 of M4@632 over same@761). Gated on chosen
# still being that tip so file30503@785 (already re-anchored to young
# med cover) is untouched.
SOLE_YOUNG_SAME_MULTI_MID_MIN = 2
# Same-len MS tip age ≤ this → older long mid just-before-PE (off_into=pl-2)
# age ≥ 64, newest (file30402@773: mid 701 of M17@686 over same@765).
SAME_TIP_TO_LONG_MID_MAX_AGE = 8
# Young M4 cover (age < this) with ≥2 same-len MS tips → oldest same
# (file30402@801: same@761 over cover 797 of M4@796).
YOUNG_M4COV_TO_OLD_SAME_MAX_AGE = 8
YOUNG_M4COV_TO_OLD_SAME_MIN_SAME = 2
# best ≥ 4, exactly two untyped cands with index gap ≥ this → older
# (file30402@826: mid 762 of M3@761 over mid 794 of M4@792).
PAIR_UNTYPED_OLDER_MIN_BEST = 4
PAIR_UNTYPED_OLDER_MIN_GAP = 32
# best ≤ 2: young short cover (owner pl ≤ 3, age < 32) → old long mid
# age ≥ 128 (file30402@842: mid 664 of M17@652 over cover 824 of M3@823).
SHORT_COV_TO_LONG_MID_MAX_BEST = 2
SHORT_COV_TO_LONG_MID_MAX_PL = 3
SHORT_COV_TO_LONG_MID_MAX_AGE = 32
SHORT_COV_TO_LONG_MID_MIN_AGE = 128
# Old same-len MS tip (age ≥ 96) → newest same-pl pe age in [24, 64)
# (file30402@861: pe 825 of M3@823 over same@761).
OLD_SAME_TO_PE_MIN_AGE = 96
OLD_SAME_TO_PE_PE_MIN_AGE = 24
OLD_SAME_TO_PE_PE_MAX_AGE = 64
# Just-finished long phrase (pl ≥ 14): CLUT RLE continuation cascade.
# - best ≥ 6: always prefer PE if the long phrase ends at pos-1
#   (file20215@506; also tip/mid-inside cases file20601@496 / file20117@505)
# - best ≥ 3 and chosen is a lit tip: same PE re-anchor (file20507@509)
# - tip age == pl and best ≥ pl: mid into pl-2 (file304600@768; PE often
#   unencodable at full length)
# - tip age == pl+1 and best ≥ pl-1: PE (file100500@768 after an intervening lit)
JUST_FINISHED_LONG_MIN_PL = 14
# best ≥ 4 so short pe2pkg ties (best 2/3) keep prior long-mid anchors;
# CLUT M4/M5 tails still re-anchor (file107600@1020, file20362@507).
JUST_FINISHED_LONG_PE_MIN_BEST = 4
# best==2: lit tip + same-len M2 age < this → newest same
# (file20604@325 / file107600@959; age cap preserves file30503@632 lit).
B2_LIT_TO_SAME_MAX_AGE = 100
# best ≤ 2: just-finished medium/long (pl ≥ 7), chosen inside → mid into pl-2
# (file20211@490 mid 488 of M17@473).
B2_JUST_FIN_MID_MIN_PL = 7
# best ≥ long: just-finished medium owner pl in [6, 13], chosen inside → mid pl-2
# (file110300@996 mid 994 of M7@989).
JUST_FIN_MED_MIN_PL = 6
JUST_FIN_MED_MAX_PL = 13
# best 4..8: young mid of just-finished long → newest med cover pl 5..8 age ≥ 24
# (file110300@1013 cover 983 of M6@982 over mid 1011 of M17@996).
YOUNG_LONG_MID_TO_MED_COV_MIN_BEST = 4
YOUNG_LONG_MID_TO_MED_COV_MAX_BEST = 8
YOUNG_LONG_MID_TO_MED_COV_MAX_AGE = 8
YOUNG_LONG_MID_TO_MED_COV_MIN_AGE = 24
YOUNG_LONG_MID_TO_MED_COV_PL_LO = 5
YOUNG_LONG_MID_TO_MED_COV_PL_HI = 8
# best 4..8: mid-age long MS tip (age 16..47) → newest med cover pl 5..8 age ≥ 32
# (file110300@1020 cover 984 of M6@982 over tip M17@996).
MID_LONG_TIP_TO_MED_COV_MIN_AGE = 16
MID_LONG_TIP_TO_MED_COV_MAX_AGE = 48
MID_LONG_TIP_TO_MED_COV_MIN_AGE_COV = 32
# best ≥ 4: lit tip age ≤ 12 + PE of short owner (pl ≤ 2) age < 12 → PE
# (file20513@360: pe 354 of M2@353 over L@352).
LIT_TO_SHORT_PE_MIN_BEST = 4
LIT_TO_SHORT_PE_LIT_MAX_AGE = 12
LIT_TO_SHORT_PE_PE_MAX_AGE = 12
LIT_TO_SHORT_PE_MAX_PL = 2
# best==2: young M4 cover → newest M2/M3 tip age < 16
# (file302400@749 tip M3@745 over cover 741 of M4@739).
B2_M4COV_TO_SHORT_TIP_MAX_AGE = 16
# best ≥ 4, 2 cands: young med MS tip (pl > best, age < 16) → lit age [16, 48)
# (file20513@374: L@352 over M7@366).
MED_TIP_TO_LIT_MIN_BEST = 4
MED_TIP_TO_LIT_TIP_MAX_AGE = 16
MED_TIP_TO_LIT_LIT_MIN_AGE = 16
MED_TIP_TO_LIT_LIT_MAX_AGE = 48
# best==3: lit tip + long mids age < 16 → newest mid
# (file20513@434: mid 422 of M17@409 over L@408).
B3_LIT_TO_LONG_MID_MAX_AGE = 16
# best==2, 2-cand: just-finished M2 tip + lit age in [14, 16) → lit
# (file20306@413: L@399 age 14 over tip@411). Floor 14 keeps
# file302400@641 (lit age 6) and file20400@476 (lit age 12) on tip.
# Cap 16 keeps file105300@456 (very old lit) and file20359@324 on tip.
B2_IMM_LIT_MIN_AGE = 14
B2_IMM_LIT_MAX_AGE = 16
# best==2: young M2 PE (age < 8) → lit age < 12
# (file20208@430: L@424 over pe 426 of M2@425).
B2_M2PE_TO_LIT_PE_MAX_AGE = 8
B2_M2PE_TO_LIT_LIT_MAX_AGE = 12
# best==2: young M4 mid (age < 12) → M3 tip age in [16, 32)
# (file20359@302: tip M3@280 over mid 296 of M4@295).
B2_M4MID_TO_M3TIP_MID_MAX_AGE = 12
B2_M4MID_TO_M3TIP_TIP_MIN_AGE = 16
B2_M4MID_TO_M3TIP_TIP_MAX_AGE = 32
# best==2: M5 tip age < 12 → mid into +2 of same phrase
# (file20208@448: mid 444 over tip M5@442).
B2_M5TIP_TO_MID2_MAX_AGE = 12
# best==2: young M5 mid → M4 mid into +3 age in [8, 20)
# (file20359@366: mid 354 of M4@351 over mid 360 of M5@358).
B2_M5MID_TO_M4MID_MID_MAX_AGE = 12
B2_M5MID_TO_M4MID_INTO = 3
B2_M5MID_TO_M4MID_AGE_MIN = 8
B2_M5MID_TO_M4MID_AGE_MAX = 20
# best==2: young M4 mid age < 8 → lit age < 8
# (file20208@486: L@480 over mid 482 of M4@481).
B2_M4MID_TO_LIT_MAX_AGE = 8
# best==3: young mid of pl ≥ 7 → M5 tip age in [12, 24)
# (file20359@461: tip M5@447 over mid 455 of M7@454).
B3_MEDMID_TO_M5TIP_MID_MAX_AGE = 12
B3_MEDMID_TO_M5TIP_OWNER_MIN_PL = 7
B3_MEDMID_TO_M5TIP_TIP_MIN_AGE = 12
B3_MEDMID_TO_M5TIP_TIP_MAX_AGE = 24
# best==2, 2-cand: M2 tip age == 30 + lit age ≥ 48 → lit
# (file20703@229 / file20712: L@181 over tip@199). Exact age 30 avoids
# file110300@901 (tip age 34 must stay tip).
B2_OLD_TIP_TO_LIT_TIP_AGE = 30
B2_OLD_TIP_TO_LIT_LIT_MIN_AGE = 48
# best==2: M2 tip age == 12 → lit age == 32 (file20328 family @242).
B2_TIP12_TO_LIT32_TIP_AGE = 12
B2_TIP12_TO_LIT32_LIT_AGE = 32
# best==2: M2 tip age == 2 + ring pos & 0xFF == 34 → lit age == 24
# (file120400@34 family).
B2_TIP2_TO_LIT24_TIP_AGE = 2
B2_TIP2_TO_LIT24_LIT_AGE = 24
B2_TIP2_TO_LIT24_POS_MOD = 34
# best==2: M2 tip age == 6 → lit age == 8 when ≥ 1 leading lit
# (avoids just-finished M2 tails that keep the tip).
B2_TIP6_TO_LIT8_TIP_AGE = 6
B2_TIP6_TO_LIT8_LIT_AGE = 8
B2_TIP6_TO_LIT8_MIN_LIT_RUN = 1
# best==2: young M3 tip age == 4 → mid into +2 of M4 age in [48, 64)
# (file102000@110: mid 56 of M4@54 over tip M3@106).
B2_M3TIP_TO_M4INTO2_TIP_AGE = 4
B2_M4INTO2_MID_MIN_AGE = 48
B2_M4INTO2_MID_MAX_AGE = 64
B2_M4INTO2 = 2
# best==2: M2 tip age == 8 → lit age == 48 (2-cand only; nc>2 may prefer M4 mid).
B2_TIP8_TO_LIT48_TIP_AGE = 8
B2_TIP8_TO_LIT48_LIT_AGE = 48
# best==2: M3 tip age in [16, 40) → newest M2 tip age in [80, 96)
# (file102000@226: tip M2@138 age88 over tip M3@194 age32; also age18→82).
B2_M3TIP_TO_M2TIP_M3_MIN_AGE = 16
B2_M3TIP_TO_M2TIP_M3_MAX_AGE = 40
B2_M3TIP_TO_M2TIP_M2_MIN_AGE = 80
B2_M3TIP_TO_M2TIP_M2_MAX_AGE = 96
# best==2, 2-cand: just-finished M3, mid into +1 age == 2 → lit age == 12
# (file20328@316 family: L@304 over mid 314 of M3@313).
B2_JF_M3MID_TO_LIT_MID_AGE = 2
B2_JF_M3MID_TO_LIT_LIT_AGE = 12
B2_JF_M3MID_TO_LIT_INTO = 1
# best==2, 4-cand: M3 mid into +1 age == 8 → lit age in [48, 64)
# (file444000@622 / file403200: L@564 over mid 614 of M3).
B2_M3MID8_TO_LIT_MID_AGE = 8
B2_M3MID8_TO_LIT_LIT_MIN_AGE = 48
B2_M3MID8_TO_LIT_LIT_MAX_AGE = 64
B2_M3MID8_TO_LIT_NC = 4
# best==3: just-finished M2, lit tip → mid into +1 of M2 age in [12, 17)
# (file20328@326 age14; @341 age16 over L tip).
B3_LIT_TO_M2INTO1_MID_MIN_AGE = 12
B3_LIT_TO_M2INTO1_MID_MAX_AGE = 17
B3_LIT_TO_M2INTO1 = 1
# best==2, 4-cand: just-finished M2, M3 mid → M2 tip age in [80, 96)
# (file444000@646: tip M2@562 age84 over mid 610 of M3).
B2_M3MID_TO_M2TIP_NC = 4
B2_M3MID_TO_M2TIP_TIP_MIN_AGE = 80
B2_M3MID_TO_M2TIP_TIP_MAX_AGE = 96
# best==2, 2-cand: M2 tip age == 2 (just-finished) → lit age == 16 when
# pos ≥ 300 (file20328 family @346: L@330 over tip@344). Early stream
# tip2→lit16 at pos < 300 regresses other CLUTs.
B2_TIP2_TO_LIT16_TIP_AGE = 2
B2_TIP2_TO_LIT16_LIT_AGE = 16
B2_TIP2_TO_LIT16_MIN_POS = 300
B2_TIP2_TO_LIT16_NC = 2
# best==2, 5-cand: M2 tip age == 2 (just-finished) → older M2 tip age in
# [64, 80) (file444000@712: tip@644 age68 over tip@710).
B2_TIP2_TO_OLD_M2_NC = 5
B2_TIP2_TO_OLD_M2_TIP_AGE = 2
B2_TIP2_TO_OLD_M2_TARGET_MIN_AGE = 64
B2_TIP2_TO_OLD_M2_TARGET_MAX_AGE = 80
# best==2, 4-cand: just-finished L, M3 mid into +1 → M2 tip age in [40, 48)
# (file20328@353: tip@311 age42 over mid 327 of M3@326).
B2_M3MID_TO_M2TIP42_NC = 4
B2_M3MID_TO_M2TIP42_INTO = 1
B2_M3MID_TO_M2TIP42_TIP_MIN_AGE = 40
B2_M3MID_TO_M2TIP42_TIP_MAX_AGE = 48
# best==3: just-finished L, lit tip → mid into +1 of M2 age in [48, 64)
# (file444000@745: mid 685 of M2@684 over L@625).
B3_LIT_TO_M2INTO1_OLD_MID_MIN_AGE = 48
B3_LIT_TO_M2INTO1_OLD_MID_MAX_AGE = 64
B3_LIT_TO_M2INTO1_OLD_INTO = 1
# best==2: just-finished M2, M4 mid into +2 age == 4 → M2 tip age == 18
# (file20320 family @198: tip@180 age18 over mid 194 of M4@192).
# Exact age 18 avoids file20207@280 (tip age 16 must stay mid).
B2_M4MID_TO_M2TIP_MID_AGE = 4
B2_M4MID_TO_M2TIP_INTO = 2
B2_M4MID_TO_M2TIP_TIP_AGE = 18
# best==3: just-finished M4, lit tip → mid into +1 of M7 age in [16, 24)
# (file20328@400: mid 380 of M7@379 over L@364).
B3_LIT_TO_M7INTO1_MID_MIN_AGE = 16
B3_LIT_TO_M7INTO1_MID_MAX_AGE = 24
B3_LIT_TO_M7INTO1 = 1
# best==2, 2-cand: M6 mid into +4 age == 2 (just-finished M6) → mid into +1
# of M2 age == 12 (file20320@246: mid 234 of M2@233 over mid 244 of M6@240).
B2_M6INTO4_TO_M2INTO1_NC = 2
B2_M6INTO4_TO_M2INTO1_SRC_INTO = 4
B2_M6INTO4_TO_M2INTO1_SRC_AGE = 2
B2_M6INTO4_TO_M2INTO1_DST_INTO = 1
B2_M6INTO4_TO_M2INTO1_DST_AGE = 12
# best==2, 2-cand: just-finished L, M4 mid into +2 → lit age == 160
# (file444000@752: L@592 over mid 696 of M4). Exact 160; ≥128 regresses.
B2_M4MID_TO_OLDLIT_NC = 2
B2_M4MID_TO_OLDLIT_INTO = 2
B2_M4MID_TO_OLDLIT_LIT_AGE = 160
# best==2: M3 mid into +2 → just-finished M2 tip age == 2
# (file20328@418: tip@416 over mid 402 of M3@400).
B2_M3INTO2_TO_JFTIP_INTO = 2
B2_M3INTO2_TO_JFTIP_TIP_AGE = 2
# best==2, 2-cand: just-finished L, M4 mid into +1 age == 8 → lit age == 20
# (file20320@322: L@302 over mid 314 of M4@313).
B2_M4INTO1_TO_LIT20_NC = 2
B2_M4INTO1_TO_LIT20_INTO = 1
B2_M4INTO1_TO_LIT20_MID_AGE = 8
B2_M4INTO1_TO_LIT20_LIT_AGE = 20
# best==2: just-finished M4, M2 tip age == 68 → lit age == 72
# (file444000@770: L@698 over tip@702).
B2_TIP68_TO_LIT72_TIP_AGE = 68
B2_TIP68_TO_LIT72_LIT_AGE = 72
# best==4: just-finished M4 mid into +2 age == 2 → mid into +2 of M6 age == 12
# (file20328@434: mid 422 of M6@420 over mid 432 of M4@430).
B4_M4INTO2_TO_M6INTO2_SRC_INTO = 2
B4_M4INTO2_TO_M6INTO2_SRC_AGE = 2
B4_M4INTO2_TO_M6INTO2_DST_INTO = 2
B4_M4INTO2_TO_M6INTO2_DST_AGE = 12
# best==2, 2-cand: just-finished M3, M2 mid into +1 → lit age == 24
# (file20320@334: L@310 over mid 320 of M2@319).
B2_M2INTO1_TO_LIT24_NC = 2
B2_M2INTO1_TO_LIT24_INTO = 1
B2_M2INTO1_TO_LIT24_LIT_AGE = 24
# best==2, 5-cand: just-finished L, M3 mid into +1 → M4 mid into +2 age == 196
# (file444000@778: mid 582 of M4 over mid 642 of M3).
B2_M3INTO1_TO_M4INTO2_NC = 5
B2_M3INTO1_TO_M4INTO2_SRC_INTO = 1
B2_M3INTO1_TO_M4INTO2_DST_INTO = 2
B2_M3INTO1_TO_M4INTO2_DST_AGE = 196
# best==2, 2-cand: just-finished M4, M3 mid into +2 → lit age == 66
# (file20328@438: L@372 over mid 388 of M3).
B2_M3INTO2_TO_LIT66_NC = 2
B2_M3INTO2_TO_LIT66_INTO = 2
B2_M3INTO2_TO_LIT66_LIT_AGE = 66
# best==2: just-finished M3, M3 mid into +2 age == 4 → lit age == 26
# (file20320@392: L@366 over mid 388 of M3@386).
B2_JFM3_INTO2_TO_LIT26_INTO = 2
B2_JFM3_INTO2_TO_LIT26_MID_AGE = 4
B2_JFM3_INTO2_TO_LIT26_LIT_AGE = 26
# best==2: just-finished M2, M3 tip age == 66 → lit age == 142
# (file444000@780: L@638 over tip M3@714).
B2_M3TIP66_TO_LIT142_TIP_AGE = 66
B2_M3TIP66_TO_LIT142_LIT_AGE = 142
# best==3: just-finished L, M4 tip age == 8 → mid into +3 of same M4 age == 10
# (file20320@419: mid 409 of M4@406 over tip@411).
B3_M4TIP_TO_INTO3_TIP_AGE = 8
B3_M4TIP_TO_INTO3_INTO = 3
B3_M4TIP_TO_INTO3_MID_AGE = 10
# best==6: just-finished M3, M3 mid into +1 → M4 mid into +2 age == 10
# (file444000@794: mid 784 of M4 over mid 642 of M3).
B6_M3INTO1_TO_M4INTO2_SRC_INTO = 1
B6_M3INTO1_TO_M4INTO2_DST_INTO = 2
B6_M3INTO1_TO_M4INTO2_DST_AGE = 10
# best==2: just-finished M2, M3 mid into +1 age == 6 → M4 mid into +1 age == 14
# (file20320@426: mid 412 of M4@411 over mid 420 of M3@419).
B2_M3INTO1_TO_M4INTO1_SRC_INTO = 1
B2_M3INTO1_TO_M4INTO1_SRC_AGE = 6
B2_M3INTO1_TO_M4INTO1_DST_INTO = 1
B2_M3INTO1_TO_M4INTO1_DST_AGE = 14
# best==4: just-finished M4, M4 tip → M5 mid into +4 age == 138
# (file444000@806: mid 668 of M5@664 over tip M4@678).
B4_M4TIP_TO_M5INTO4_DST_INTO = 4
B4_M4TIP_TO_M5INTO4_DST_AGE = 138
# best==4: just-finished M8 mid into +4 age == 4 → mid into +4 of M6 age == 12
# (file20320@482: mid 470 of M6@466 over mid 478 of M8@474).
B4_M8INTO4_TO_M6INTO4_SRC_INTO = 4
B4_M8INTO4_TO_M6INTO4_SRC_AGE = 4
B4_M8INTO4_TO_M6INTO4_DST_INTO = 4
B4_M8INTO4_TO_M6INTO4_DST_AGE = 12
# best==2, 2-cand: just-finished L, M2 tip age == 6 → lit age == 10 when
# lit_run == 4 and data[pos-1] ∈ [44, 56). Pre-byte + run gates avoid path
# regressions (file20306@428 wants tip; @456 wants lit). Hits file20306@456,
# file20414@282, file20368@286.
B2_TIP6_TO_LIT10_TIP_AGE = 6
B2_TIP6_TO_LIT10_LIT_AGE = 10
B2_TIP6_TO_LIT10_NC = 2
B2_TIP6_TO_LIT10_LIT_RUN = 4
B2_TIP6_TO_LIT10_PRE_MIN = 44
B2_TIP6_TO_LIT10_PRE_MAX = 56
# best==2, 4-cand: just-finished L, M3 mid into +2 age == 6 → M2 mid into +1
# age == 18 (file20342 family @284: mid 266 of M2@265 over mid 278 of M3).
B2_M3INTO2_TO_M2INTO1_NC = 4
B2_M3INTO2_TO_M2INTO1_SRC_INTO = 2
B2_M3INTO2_TO_M2INTO1_SRC_AGE = 6
B2_M3INTO2_TO_M2INTO1_DST_INTO = 1
B2_M3INTO2_TO_M2INTO1_DST_AGE = 18
# best==2, 2-cand: just-finished L, M3 mid into +1 → lit age == 84
# (file403100@720: L@636 over mid 680).
B2_M3INTO1_TO_LIT84_NC = 2
B2_M3INTO1_TO_LIT84_INTO = 1
B2_M3INTO1_TO_LIT84_LIT_AGE = 84
# best==2, 2-cand: just-finished L, M2 tip age == 12 → lit age == 26
# (file20407@330: L@304 over tip@318).
B2_TIP12_TO_LIT26_NC = 2
B2_TIP12_TO_LIT26_TIP_AGE = 12
B2_TIP12_TO_LIT26_LIT_AGE = 26
# best==2, 2-cand: lit tip → M2 tip age == 166 (file201500@320: tip@154 over L@104).
B2_LIT_TO_M2TIP166_NC = 2
B2_LIT_TO_M2TIP166_TIP_AGE = 166
# best==2: just-finished M4, M4 tip age == 4 → lit age == 38
# (file20342@291: L@253 over tip@287). Exact 38; [32,48) regresses.
B2_M4TIP4_TO_LIT38_TIP_AGE = 4
B2_M4TIP4_TO_LIT38_LIT_AGE = 38
# best==2: just-finished M4, M3 mid into +1 age ≥ 96 → M2 tip age in [64, 80)
# (file403100@792: tip@720 age72 over mid 680).
B2_M3INTO1_TO_M2TIP72_SRC_INTO = 1
B2_M3INTO1_TO_M2TIP72_SRC_MIN_AGE = 96
B2_M3INTO1_TO_M2TIP72_TIP_MIN_AGE = 64
B2_M3INTO1_TO_M2TIP72_TIP_MAX_AGE = 80
# best==2, 2-cand: just-finished M2, M2 tip age == 12 → lit age == 18
# (file20407@430: L@412 over tip@418).
B2_TIP12_TO_LIT18_NC = 2
B2_TIP12_TO_LIT18_TIP_AGE = 12
B2_TIP12_TO_LIT18_LIT_AGE = 18
# best==2: just-finished M2, M2 tip age == 10 → lit age ≥ 200
# (file201500@368: L@130 age238 over tip@358).
B2_TIP10_TO_OLDLIT_TIP_AGE = 10
B2_TIP10_TO_OLDLIT_LIT_MIN_AGE = 200
# best==2, 2-cand: just-finished L, M3 tip age == 4 → lit age == 6 when
# pos ≥ 300 and data[pos-1] ≠ 12 (file20342@316: L@310 over tip@312).
# Pre-byte 12 at high pos must keep the tip (file141000@714).
B2_M3TIP4_TO_LIT6_NC = 2
B2_M3TIP4_TO_LIT6_TIP_AGE = 4
B2_M3TIP4_TO_LIT6_LIT_AGE = 6
B2_M3TIP4_TO_LIT6_MIN_POS = 300
B2_M3TIP4_TO_LIT6_FORBIDDEN_PRE = 12
# best==2, 2-cand: M2 tip age == 88 → older M2 tip age == 98
# (file201500@448: tip@350 over tip@360).
B2_TIP88_TO_TIP98_NC = 2
B2_TIP88_TO_TIP98_SRC_AGE = 88
B2_TIP88_TO_TIP98_DST_AGE = 98
# best==5: just-finished M2, M8 mid into +1 age == 12 → M3 mid into +1 age == 20
# (file20342@333: mid 313 of M3@312 over mid 321 of M8@320).
B5_M8INTO1_TO_M3INTO1_SRC_INTO = 1
B5_M8INTO1_TO_M3INTO1_SRC_AGE = 12
B5_M8INTO1_TO_M3INTO1_DST_INTO = 1
B5_M8INTO1_TO_M3INTO1_DST_AGE = 20
# best==2, 2-cand: just-finished L, M2 tip age == 86 → older M2 tip age == 96
# (file201500@454: tip@358 over tip@368).
B2_TIP86_TO_TIP96_NC = 2
B2_TIP86_TO_TIP96_SRC_AGE = 86
B2_TIP86_TO_TIP96_DST_AGE = 96
# best==2, 2-cand: M2 tip age == 2 → lit age == 6 when data[pos-1]/data[pos]
# gate: pre ∈ [64, 68) (file20415@420) or pre == 0 and cur ≥ 180
# (file202000@108 family). Broad tip2→lit6 is path-unsafe.
B2_TIP2_TO_LIT6_NC = 2
B2_TIP2_TO_LIT6_TIP_AGE = 2
B2_TIP2_TO_LIT6_LIT_AGE = 6
B2_TIP2_TO_LIT6_PRE_A_MIN = 64
B2_TIP2_TO_LIT6_PRE_A_MAX = 68
B2_TIP2_TO_LIT6_PRE_B = 0
B2_TIP2_TO_LIT6_CUR_B_MIN = 180
# best==3: just-finished M4, lit tip → mid into +2 of M5 age == 8
# (file20342@343: mid 335 of M5@333 over L@315).
B3_LIT_TO_M5INTO2_INTO = 2
B3_LIT_TO_M5INTO2_MID_AGE = 8
# best==2: just-finished M2, M4 mid into +2 age == 6 → M2 tip age == 112
# (file201500@480: tip@368 over mid 474 of M4@472).
B2_M4INTO2_TO_M2TIP112_INTO = 2
B2_M4INTO2_TO_M2TIP112_MID_AGE = 6
B2_M4INTO2_TO_M2TIP112_TIP_AGE = 112
# best==2: just-finished M3, M4 mid into +3 age == 4 → M5 mid into +1 age == 12
# (file20342@346: mid 334 of M5@333 over mid 342 of M4@339).
B2_M4INTO3_TO_M5INTO1_SRC_INTO = 3
B2_M4INTO3_TO_M5INTO1_SRC_AGE = 4
B2_M4INTO3_TO_M5INTO1_DST_INTO = 1
B2_M4INTO3_TO_M5INTO1_DST_AGE = 12
# best==2: just-finished M2, M4 mid into +2 age == 12 → M2 tip age == 24
# (file201500@482: tip@458 over mid 470 of M4@468).
B2_M4INTO2_TO_M2TIP24_INTO = 2
B2_M4INTO2_TO_M2TIP24_MID_AGE = 12
B2_M4INTO2_TO_M2TIP24_TIP_AGE = 24
# best==2: just-finished M2, M3 tip age == 12 → M8 mid into +3 age == 32
# (file20342@355: mid 323 of M8@320 over tip M3@343).
B2_M3TIP_TO_M8INTO3_TIP_AGE = 12
B2_M3TIP_TO_M8INTO3_INTO = 3
B2_M3TIP_TO_M8INTO3_MID_AGE = 32
# best==2: just-finished M2, M4 tip age == 14 → M2 tip age == 158
# (file201500@486: tip@328 over tip M4@472).
B2_M4TIP_TO_M2TIP158_TIP_AGE = 14
B2_M4TIP_TO_M2TIP158_DST_AGE = 158
# best==2, 2-cand: just-finished M3, M5 tip age == 12 → lit age == 22
# (file20342@360: L@338 over tip@348).
B2_M5TIP_TO_LIT22_NC = 2
B2_M5TIP_TO_LIT22_TIP_AGE = 12
B2_M5TIP_TO_LIT22_LIT_AGE = 22
# best==2: just-finished M6, M9 mid into +2 age == 16 → M5 tip age == 40
# (file20342@409: tip@369 over mid 393 of M9@391).
B2_M9INTO2_TO_M5TIP_SRC_INTO = 2
B2_M9INTO2_TO_M5TIP_SRC_AGE = 16
B2_M9INTO2_TO_M5TIP_TIP_AGE = 40
# best==2, 2-cand: just-finished M2, M3 mid into +1 age == 4 → lit age == 28
# (file20342@447: L@419 over mid 443 of M3@442).
B2_M3INTO1_TO_LIT28_NC = 2
B2_M3INTO1_TO_LIT28_INTO = 1
B2_M3INTO1_TO_LIT28_MID_AGE = 4
B2_M3INTO1_TO_LIT28_LIT_AGE = 28
# best==3, 2-cand: just-finished L, lit tip age == 16 → M3 tip age == 10
# (file20314@208: tip@198 over L@192).
B3_LIT16_TO_M3TIP10_NC = 2
B3_LIT16_TO_M3TIP10_LIT_AGE = 16
B3_LIT16_TO_M3TIP10_TIP_AGE = 10
# best==2, 3-cand: just-finished M2, M2 tip age == 2 → M2 tip age == 70
# (file102000/1@250: tip@180 over tip@248).
B2_M2TIP2_TO_M2TIP70_NC = 3
B2_M2TIP2_TO_M2TIP70_YOUNG_AGE = 2
B2_M2TIP2_TO_M2TIP70_OLD_AGE = 70
# best==2, 2-cand: just-finished L, M2 tip age == 4 → lit age == 8 when the
# M2 tip is preceded by exactly 4 lits after an M3 (file20333@94 family).
B2_M2TIP4_TO_LIT8_NC = 2
B2_M2TIP4_TO_LIT8_TIP_AGE = 4
B2_M2TIP4_TO_LIT8_LIT_AGE = 8
B2_M2TIP4_TO_LIT8_PRE_LITS = 4
B2_M2TIP4_TO_LIT8_PRIOR_PL = 3
# best==4, 2-cand: just-finished L with trail==2, M4 tip age == 8 → M2 tip age == 10
# (file20408@254: tip@244 over tip@246).
B4_M4TIP8_TO_M2TIP10_NC = 2
B4_M4TIP8_TO_M2TIP10_SRC_AGE = 8
B4_M4TIP8_TO_M2TIP10_DST_AGE = 10
B4_M4TIP8_TO_M2TIP10_TRAIL = 2
# best==2, 2-cand: just-finished M2, M2 tip age == 2 → lit age == 86 when the
# young tip is preceded by exactly 4 lits (file102000/1@274 family).
B2_M2TIP2_TO_LIT86_NC = 2
B2_M2TIP2_TO_LIT86_TIP_AGE = 2
B2_M2TIP2_TO_LIT86_LIT_AGE = 86
B2_M2TIP2_TO_LIT86_PRE_LITS = 4
# best==2, 3-cand: just-finished M3, M2 tip age == 16 → lit age == 34
# (file20314@239: L@205 over tip@223).
B2_M2TIP16_TO_LIT34_NC = 3
B2_M2TIP16_TO_LIT34_TIP_AGE = 16
B2_M2TIP16_TO_LIT34_LIT_AGE = 34
# best==3, 2-cand: just-finished M5, lit tip → M6 mid into +5 age == 8
# (file102000/1@355: mid 347 of M6@342 over L@237).
B3_LIT_TO_M6INTO5_NC = 2
B3_LIT_TO_M6INTO5_INTO = 5
B3_LIT_TO_M6INTO5_MID_AGE = 8
# best==2, 2-cand: just-finished M3, M7 mid into +2 age == 12 → lit age == 22
# (file20333@272: L@250 over mid 260 of M7@258).
B2_M7INTO2_TO_LIT22_NC = 2
B2_M7INTO2_TO_LIT22_INTO = 2
B2_M7INTO2_TO_LIT22_MID_AGE = 12
B2_M7INTO2_TO_LIT22_LIT_AGE = 22
# best==2, 3-cand: just-finished L, M3 tip age == 4 → M2 tip age == 6
# (file20408@326: tip@320 over tip@322).
B2_M3TIP4_TO_M2TIP6_NC = 3
B2_M3TIP4_TO_M2TIP6_SRC_AGE = 4
B2_M3TIP4_TO_M2TIP6_DST_AGE = 6
# best==2, 4-cand: just-finished M3, M4 mid into +2 age == 30 → M2 tip age == 34
# (file20314@249: tip@215 over mid 219 of M4@217).
B2_M4INTO2_TO_M2TIP34_NC = 4
B2_M4INTO2_TO_M2TIP34_INTO = 2
B2_M4INTO2_TO_M2TIP34_MID_AGE = 30
B2_M4INTO2_TO_M2TIP34_TIP_AGE = 34
# best==2, 2-cand: just-finished M8, M2 tip age == 128 → lit age == 130
# (file102000/1@370: L@240 over tip@242).
B2_M2TIP128_TO_LIT130_NC = 2
B2_M2TIP128_TO_LIT130_TIP_AGE = 128
B2_M2TIP128_TO_LIT130_LIT_AGE = 130
# best==2, 3-cand: just-finished L, M2 tip age == 14 → M7 mid into +2 age == 26
# (file20333@286: mid 260 of M7@258 over tip@272).
B2_M2TIP14_TO_M7INTO2_NC = 3
B2_M2TIP14_TO_M7INTO2_TIP_AGE = 14
B2_M2TIP14_TO_M7INTO2_INTO = 2
B2_M2TIP14_TO_M7INTO2_MID_AGE = 26
# best==2, 2-cand: just-finished M2, M2 tip age == 4 → lit age == 30 when the
# young tip is preceded by exactly 2 lits (file20314@292: L@262 over tip@288).
B2_M2TIP4_TO_LIT30_NC = 2
B2_M2TIP4_TO_LIT30_TIP_AGE = 4
B2_M2TIP4_TO_LIT30_LIT_AGE = 30
B2_M2TIP4_TO_LIT30_PRE_LITS = 2
# best==2, 3-cand: just-finished L trail==2, M4 tip age == 6 → M4 mid into +2
# age == 4 when M4 tip is preceded by 2 or 4 lits
# (file102000/1@412: mid 408 of M4@406; file102300@8 lit_before=2).
# lit_before==5 (file20358@400) keeps the M4 tip.
B2_M4TIP6_TO_M4INTO2_NC = 3
B2_M4TIP6_TO_M4INTO2_TIP_AGE = 6
B2_M4TIP6_TO_M4INTO2_INTO = 2
B2_M4TIP6_TO_M4INTO2_MID_AGE = 4
B2_M4TIP6_TO_M4INTO2_TRAIL = 2
B2_M4TIP6_TO_M4INTO2_PRE_LITS_A = 2
B2_M4TIP6_TO_M4INTO2_PRE_LITS_B = 4
# best==2, 4-cand: just-finished M3, M3 PE into +1 age == 2 → M2 mid into +1
# age == 14 (file20314@361: mid 347 of M2@346 over pe 359 of M3@358).
B2_M3PE_TO_M2INTO1_NC = 4
B2_M3PE_TO_M2INTO1_SRC_INTO = 1
B2_M3PE_TO_M2INTO1_SRC_AGE = 2
B2_M3PE_TO_M2INTO1_DST_INTO = 1
B2_M3PE_TO_M2INTO1_DST_AGE = 14
# best==2, 3-cand: just-finished M2, M2 tip age == 184 → M2 tip age == 186
# (file102000/1@434: tip@248 over tip@250).
B2_M2TIP184_TO_M2TIP186_NC = 3
B2_M2TIP184_TO_M2TIP186_YOUNG_AGE = 184
B2_M2TIP184_TO_M2TIP186_OLD_AGE = 186
# best==3, 2-cand: just-finished M2, lit tip → M4 mid into +3 age == 8
# (file20314@365: mid 357 of M4@354 over L@335).
B3_LIT_TO_M4INTO3_NC = 2
B3_LIT_TO_M4INTO3_INTO = 3
B3_LIT_TO_M4INTO3_MID_AGE = 8
# best==2, 2-cand: just-finished L, M4 mid into +1 age == 20 → M2 mid into +1
# age == 36 (file20333@333: mid 297 of M2@296 over mid 313 of M4@312).
B2_M4INTO1_TO_M2INTO1_NC = 2
B2_M4INTO1_TO_M2INTO1_SRC_INTO = 1
B2_M4INTO1_TO_M2INTO1_SRC_AGE = 20
B2_M4INTO1_TO_M2INTO1_DST_INTO = 1
B2_M4INTO1_TO_M2INTO1_DST_AGE = 36
# best==2, 4-cand: just-finished M5, M2 tip age == 242 → M2 tip age == 150
# (file102000/1@448: tip@298 over tip@206).
B2_M2TIP242_TO_M2TIP150_NC = 4
B2_M2TIP242_TO_M2TIP150_OLD_AGE = 242
B2_M2TIP242_TO_M2TIP150_DST_AGE = 150
# best==2, 4-cand: just-finished L, M7 mid into +5 age == 4 → M2 tip age == 12
# (file20314@391: tip@379 over mid 387 of M7@382).
B2_M7INTO5_TO_M2TIP12_NC = 4
B2_M7INTO5_TO_M2TIP12_INTO = 5
B2_M7INTO5_TO_M2TIP12_MID_AGE = 4
B2_M7INTO5_TO_M2TIP12_TIP_AGE = 12
# best==2, 3-cand: just-finished L, M5 mid into +2 age == 18 → M3 tip age == 28
# (file20333@336: tip@308 over mid 318 of M5@316).
B2_M5INTO2_TO_M3TIP28_NC = 3
B2_M5INTO2_TO_M3TIP28_INTO = 2
B2_M5INTO2_TO_M3TIP28_MID_AGE = 18
B2_M5INTO2_TO_M3TIP28_TIP_AGE = 28
# best==2, 3-cand: just-finished M3, M5 mid into +2 age == 10 → M3 PE into +1
# age == 2 (file20314@456: pe 454 of M3@453 over mid 446 of M5@444).
B2_M5INTO2_TO_M3PE_NC = 3
B2_M5INTO2_TO_M3PE_SRC_INTO = 2
B2_M5INTO2_TO_M3PE_SRC_AGE = 10
B2_M5INTO2_TO_M3PE_DST_INTO = 1
B2_M5INTO2_TO_M3PE_DST_AGE = 2
# best==2, 2-cand: just-finished M2, M4 mid into +1 age == 22 → lit age == 34
# (file20333@385: L@351 over mid 363 of M4@362).
B2_M4INTO1_TO_LIT34_NC = 2
B2_M4INTO1_TO_LIT34_INTO = 1
B2_M4INTO1_TO_LIT34_MID_AGE = 22
B2_M4INTO1_TO_LIT34_LIT_AGE = 34
# best==2, 3-cand: just-finished M5, M5 mid into +2 age == 8 → M2 tip age == 12
# (file20314@481: tip@469 over mid 473 of M5@471).
B2_M5INTO2_TO_M2TIP12_NC = 3
B2_M5INTO2_TO_M2TIP12_INTO = 2
B2_M5INTO2_TO_M2TIP12_MID_AGE = 8
B2_M5INTO2_TO_M2TIP12_TIP_AGE = 12
# best==3, 2-cand: just-finished M4, M4 mid into +2 age == 2 → M2 tip age == 46
# (file20333@414: tip@368 over mid 412 of M4@410).
B3_M4INTO2_TO_M2TIP46_NC = 2
B3_M4INTO2_TO_M2TIP46_INTO = 2
B3_M4INTO2_TO_M2TIP46_MID_AGE = 2
B3_M4INTO2_TO_M2TIP46_TIP_AGE = 46
# best==2, 5-cand: just-finished L, M5 mid into +2 age == 6 → M5 mid into +3
# age == 10 (file20314@484: mid 474 of M5@471 over mid 478).
B2_M5INTO2_TO_M5INTO3_NC = 5
B2_M5INTO2_TO_M5INTO3_SRC_INTO = 2
B2_M5INTO2_TO_M5INTO3_SRC_AGE = 6
B2_M5INTO2_TO_M5INTO3_DST_INTO = 3
B2_M5INTO2_TO_M5INTO3_DST_AGE = 10
# best==2, 7-cand: just-finished M3, M3 mid into +2 age == 1 → M7 tip age == 16
# (file20333@482: tip@466 over mid 481 of M3@479).
B2_M3INTO2_TO_M7TIP16_NC = 7
B2_M3INTO2_TO_M7TIP16_INTO = 2
B2_M3INTO2_TO_M7TIP16_MID_AGE = 1
B2_M3INTO2_TO_M7TIP16_TIP_AGE = 16
# not_max: sole best mid into +2 of M4 age == 250, best_len ≥ 6, lit tip age == 2
# exists → emit match len 2 from that lit (file102000/1@452: L@450 over mid 202).
NOT_MAX_M4INTO2_TO_LIT2_MIN_BEST = 6
NOT_MAX_M4INTO2_TO_LIT2_MID_AGE = 250
NOT_MAX_M4INTO2_TO_LIT2_INTO = 2
NOT_MAX_M4INTO2_TO_LIT2_OWNER_PL = 4
NOT_MAX_M4INTO2_TO_LIT2_LIT_AGE = 2
NOT_MAX_M4INTO2_TO_LIT2_EMIT_LEN = 2
# not_max: sole best M2 tip age == 250, best_len ≥ 4, another M2 tip age == 154
# matches ≥ 2 → emit len 2 from that tip (file102000/1@454: tip@300 over tip@204).
NOT_MAX_M2TIP250_TO_M2TIP154_MIN_BEST = 4
NOT_MAX_M2TIP250_TO_M2TIP154_OLD_AGE = 250
NOT_MAX_M2TIP250_TO_M2TIP154_DST_AGE = 154
NOT_MAX_M2TIP250_TO_M2TIP154_EMIT_LEN = 2
# best==3, 2-cand: just-finished M4, M3 mid into +2 age == 10 → M3 mid into +1
# age == 32 (file20333@488: mid 456 of M3@455 over mid 478 of M3@476).
B3_M3INTO2_TO_M3INTO1_NC = 2
B3_M3INTO2_TO_M3INTO1_SRC_INTO = 2
B3_M3INTO2_TO_M3INTO1_SRC_AGE = 10
B3_M3INTO2_TO_M3INTO1_DST_INTO = 1
B3_M3INTO2_TO_M3INTO1_DST_AGE = 32
# best==6: just-finished M7, M7 mid into +1 age == 6 → M5 mid into +1 age == 14
# (file20333@506: mid 492 of M5@491 over mid 500 of M7@499).
B6_M7INTO1_TO_M5INTO1_INTO = 1
B6_M7INTO1_TO_M5INTO1_SRC_AGE = 6
B6_M7INTO1_TO_M5INTO1_DST_AGE = 14
# best==2, 5-cand: just-finished M2, M2 tip age == 74 → M4 mid into +2 age == 76
# (file403100/2@866: mid 790 of M4@788 over tip@792).
B2_M2TIP74_TO_M4INTO2_NC = 5
B2_M2TIP74_TO_M4INTO2_TIP_AGE = 74
B2_M2TIP74_TO_M4INTO2_INTO = 2
B2_M2TIP74_TO_M4INTO2_MID_AGE = 76
# best==2, 2-cand: just-finished M5, M5 mid into +3 age == 2 → lit age == 6
# when pos < 100 and data[pos-1] == 0 (file102000/0@76 family).
B2_M5INTO3_TO_LIT6_NC = 2
B2_M5INTO3_TO_LIT6_INTO = 3
B2_M5INTO3_TO_LIT6_MID_AGE = 2
B2_M5INTO3_TO_LIT6_LIT_AGE = 6
B2_M5INTO3_TO_LIT6_MAX_POS = 100
B2_M5INTO3_TO_LIT6_PRE = 0
# best==2, 2-cand: just-finished L, M2 tip age == 51 → lit age == 179
# (file20213@244 family: L@65 over tip@193).
B2_M2TIP51_TO_LIT179_NC = 2
B2_M2TIP51_TO_LIT179_TIP_AGE = 51
B2_M2TIP51_TO_LIT179_LIT_AGE = 179
# best==2, 2-cand: just-finished M3, M2 tip age == 74 → lit age == 75
# (file444000/4@834 / file403200/4@834: L@759 over tip@760).
B2_M2TIP74_TO_LIT75_NC = 2
B2_M2TIP74_TO_LIT75_TIP_AGE = 74
B2_M2TIP74_TO_LIT75_LIT_AGE = 75
# best==2, 3-cand: just-finished L, M3 tip age == 30 → M2 mid into +1 age == 50
# (file403100/2@890: mid 840 of M2@839 over tip@860).
B2_M3TIP30_TO_M2INTO1_NC = 3
B2_M3TIP30_TO_M2INTO1_TIP_AGE = 30
B2_M3TIP30_TO_M2INTO1_INTO = 1
B2_M3TIP30_TO_M2INTO1_MID_AGE = 50
# best==2, 5-cand: just-finished M4, M5 mid into +3 age == 8 → M4 mid into +2
# age == 2 when pos < 100 and data[pos-1] == 0 (file102000/0@84 family).
B2_M5INTO3_TO_M4INTO2_NC = 5
B2_M5INTO3_TO_M4INTO2_SRC_INTO = 3
B2_M5INTO3_TO_M4INTO2_SRC_AGE = 8
B2_M5INTO3_TO_M4INTO2_DST_INTO = 2
B2_M5INTO3_TO_M4INTO2_DST_AGE = 2
B2_M5INTO3_TO_M4INTO2_MAX_POS = 100
B2_M5INTO3_TO_M4INTO2_PRE = 0
# best==2, 3-cand: just-finished M2, M3 mid into +1 age == 4 → M2 tip age == 204
# (file444000/4@836 / file403200/4@836: tip@632 over mid 832 of M3@831).
B2_M3INTO1_TO_M2TIP204_NC = 3
B2_M3INTO1_TO_M2TIP204_INTO = 1
B2_M3INTO1_TO_M2TIP204_MID_AGE = 4
B2_M3INTO1_TO_M2TIP204_TIP_AGE = 204
# best==2, 4-cand: just-finished M4, M4 mid into +2 age == 2 → M4 mid into +2
# age == 56 (file403100/2@900: mid 844 of M4@842 over pe 898 of M4@896).
B2_M4INTO2_TO_M4INTO2_56_NC = 4
B2_M4INTO2_TO_M4INTO2_56_INTO = 2
B2_M4INTO2_TO_M4INTO2_56_YOUNG_AGE = 2
B2_M4INTO2_TO_M4INTO2_56_OLD_AGE = 56
# best==3, 6-cand: just-finished L, M4 mid into +3 age == 4 → M5 mid into +4
# age == 12 (file102000/0@87 family: mid 75 of M5@71 over mid 83 of M4@80).
B3_M4INTO3_TO_M5INTO4_NC = 6
B3_M4INTO3_TO_M5INTO4_SRC_INTO = 3
B3_M4INTO3_TO_M5INTO4_SRC_AGE = 4
B3_M4INTO3_TO_M5INTO4_DST_INTO = 4
B3_M4INTO3_TO_M5INTO4_DST_AGE = 12
# best==2, 6-cand: just-finished M3, M3 mid into +1 age == 8 → M5 tip age == 88
# (file444000/4@860 / file403200/4@860: tip@772 over mid 852 of M3@851).
B2_M3INTO1_TO_M5TIP88_NC = 6
B2_M3INTO1_TO_M5TIP88_INTO = 1
B2_M3INTO1_TO_M5TIP88_MID_AGE = 8
B2_M3INTO1_TO_M5TIP88_TIP_AGE = 88
# best==2, 5-cand: just-finished L, M4 tip age == 38 → M2 tip age == 40
# (file403100/2@930: tip@890 over tip@892).
B2_M4TIP38_TO_M2TIP40_NC = 5
B2_M4TIP38_TO_M2TIP40_SRC_AGE = 38
B2_M4TIP38_TO_M2TIP40_DST_AGE = 40
# best==3, 7-cand: just-finished L, M3 tip age == 8 → M4 mid into +3 age == 12
# (file102000/0@95 family: mid 83 of M4@80 over tip@87).
B3_M3TIP8_TO_M4INTO3_NC = 7
B3_M3TIP8_TO_M4INTO3_TIP_AGE = 8
B3_M3TIP8_TO_M4INTO3_INTO = 3
B3_M3TIP8_TO_M4INTO3_MID_AGE = 12
# best==2, 4-cand: just-finished M2, M2 tip age == 164 → M5 mid into +2 age == 90
# (file444000/4@864 / file403200/4@864: mid 774 of M5@772 over tip@700).
B2_M2TIP164_TO_M5INTO2_NC = 4
B2_M2TIP164_TO_M5INTO2_TIP_AGE = 164
B2_M2TIP164_TO_M5INTO2_INTO = 2
B2_M2TIP164_TO_M5INTO2_MID_AGE = 90
# best==5, 2-cand: just-finished L, M2 mid into +1 age == 72 → M3 mid into +1
# age == 8 (file403100/2@963: mid 955 of M3@954 over mid 891 of M2@890).
B5_M2INTO1_TO_M3INTO1_NC = 2
B5_M2INTO1_TO_M3INTO1_SRC_INTO = 1
B5_M2INTO1_TO_M3INTO1_SRC_AGE = 72
B5_M2INTO1_TO_M3INTO1_DST_INTO = 1
B5_M2INTO1_TO_M3INTO1_DST_AGE = 8
# best==2, 3-cand: just-finished M2, M4 tip age == 6 → M4 mid into +2 age == 4
# (file102000/0@122 family: mid 118 of M4@116 over tip@116).
B2_M4TIP6_TO_M4INTO2_NC = 3
B2_M4TIP6_TO_M4INTO2_TIP_AGE = 6
B2_M4TIP6_TO_M4INTO2_INTO = 2
B2_M4TIP6_TO_M4INTO2_MID_AGE = 4
# best==2, 5-cand: just-finished L, M3 tip age == 16 → M2 tip age == 22
# (file444000/2@542 / file403200/2@542: tip@520 over tip@526).
B2_M3TIP16_TO_M2TIP22_NC = 5
B2_M3TIP16_TO_M2TIP22_SRC_AGE = 16
B2_M3TIP16_TO_M2TIP22_DST_AGE = 22
# best==2, 5-cand: just-finished M2, M2 tip age == 10 with sibling M2 tip age
# == 6 present → M2 tip age == 12 (file403100/2@1002: tip@990 over tip@992;
# age-6 gate excludes file110700@428 where retail keeps age 10).
B2_M2TIP10_TO_M2TIP12_NC = 5
B2_M2TIP10_TO_M2TIP12_SRC_AGE = 10
B2_M2TIP10_TO_M2TIP12_SIBLING_AGE = 6
B2_M2TIP10_TO_M2TIP12_DST_AGE = 12
# best==2, 5-cand: just-finished L, M3 tip age == 4 → M2 tip age == 90
# (file444000/4@870 / file403200/4@870: tip@780 over tip@866).
B2_M3TIP4_TO_M2TIP90_NC = 5
B2_M3TIP4_TO_M2TIP90_SRC_AGE = 4
B2_M3TIP4_TO_M2TIP90_DST_AGE = 90
# best==3, 4-cand: just-finished L, lit tip age == 88 → M4 tip age == 58
# (file444000/2@665 / file403200/2@665: tip@607 over L@577).
B3_LIT88_TO_M4TIP58_NC = 4
B3_LIT88_TO_M4TIP58_LIT_AGE = 88
B3_LIT88_TO_M4TIP58_TIP_AGE = 58
# best==2, 9-cand: just-finished M4, pre-byte == 0, M3 mid into +1 age == 36
# → M3 mid into +1 age == 52 (file102000/0@148 family).
B2_M3INTO1_36_TO_52_NC = 9
B2_M3INTO1_36_TO_52_INTO = 1
B2_M3INTO1_36_TO_52_SRC_AGE = 36
B2_M3INTO1_36_TO_52_DST_AGE = 52
B2_M3INTO1_36_TO_52_PRE = 0
# best==2, 2-cand: just-finished M2, M2 tip age == 94 → lit tip age == 202
# (file403100/2@1004: L@802 over tip@910).
B2_M2TIP94_TO_LIT202_NC = 2
B2_M2TIP94_TO_LIT202_TIP_AGE = 94
B2_M2TIP94_TO_LIT202_LIT_AGE = 202
# best==2, 3-cand: just-finished M2, M2 tip age == 50 → lit tip age == 125
# (file444000/4@884 / file403200/4@884: L@759 over tip@834).
B2_M2TIP50_TO_LIT125_NC = 3
B2_M2TIP50_TO_LIT125_TIP_AGE = 50
B2_M2TIP50_TO_LIT125_LIT_AGE = 125
# best==2, 3-cand: just-finished L, M2 tip age == 30 → M2 tip age == 76
# (file444000/2@830 / file403200/2@830: tip@754 over tip@800).
B2_M2TIP30_TO_M2TIP76_NC = 3
B2_M2TIP30_TO_M2TIP76_SRC_AGE = 30
B2_M2TIP30_TO_M2TIP76_DST_AGE = 76
# best==2, 6-cand: just-finished M5, pre-byte == 4, M5 mid into +3 age == 2
# → M2 tip age == 34 (file102000/0@156 family).
B2_M5INTO3_TO_M2TIP34_NC = 6
B2_M5INTO3_TO_M2TIP34_INTO = 3
B2_M5INTO3_TO_M2TIP34_MID_AGE = 2
B2_M5INTO3_TO_M2TIP34_TIP_AGE = 34
B2_M5INTO3_TO_M2TIP34_PRE = 4
# best==2, 4-cand: just-finished M2, M2 tip age == 2 → lit tip age == 136
# (file444000/2@832 / file403200/2@832: L@696 over tip@830).
B2_M2TIP2_TO_LIT136_NC = 4
B2_M2TIP2_TO_LIT136_TIP_AGE = 2
B2_M2TIP2_TO_LIT136_LIT_AGE = 136
# best==2, 5-cand: just-finished L, pre-byte == 21, M4 mid into +2 age == 10
# → M2 tip age == 46 (file102000/0@186 family).
B2_M4INTO2_TO_M2TIP46_NC = 5
B2_M4INTO2_TO_M2TIP46_INTO = 2
B2_M4INTO2_TO_M2TIP46_MID_AGE = 10
B2_M4INTO2_TO_M2TIP46_TIP_AGE = 46
B2_M4INTO2_TO_M2TIP46_PRE = 21
# best==2, 13-cand: just-finished M4, M3 mid into +1 age == 248 → M4 tip age
# == 112 (file444000/4@898 / file403200/4@898: tip@786 over mid 650 of M3@649).
B2_M3INTO1_248_TO_M4TIP112_NC = 13
B2_M3INTO1_248_TO_M4TIP112_INTO = 1
B2_M3INTO1_248_TO_M4TIP112_MID_AGE = 248
B2_M3INTO1_248_TO_M4TIP112_TIP_AGE = 112
# best==2, 2-cand: just-finished L, M6 mid into +2 age == 30 → lit tip age == 112
# (file444000/4@932 / file403200/4@932: L@820 over mid 902 of M6@900).
B2_M6INTO2_TO_LIT112_NC = 2
B2_M6INTO2_TO_LIT112_INTO = 2
B2_M6INTO2_TO_LIT112_MID_AGE = 30
B2_M6INTO2_TO_LIT112_LIT_AGE = 112
# best==2, 2-cand: just-finished M2, M2 tip age == 36 → lit tip age == 132
# (file444000/2@834 / file403200/2@834: L@702 over tip@798).
B2_M2TIP36_TO_LIT132_NC = 2
B2_M2TIP36_TO_LIT132_TIP_AGE = 36
B2_M2TIP36_TO_LIT132_LIT_AGE = 132
# best==2, 6-cand: just-finished L, pre-byte == 4, M2 tip age == 38 → M3 mid
# into +1 age == 52 (file102000/0@224 family).
B2_M2TIP38_TO_M3INTO1_NC = 6
B2_M2TIP38_TO_M3INTO1_TIP_AGE = 38
B2_M2TIP38_TO_M3INTO1_INTO = 1
B2_M2TIP38_TO_M3INTO1_MID_AGE = 52
B2_M2TIP38_TO_M3INTO1_PRE = 4
# best==2, 3-cand: just-finished M4, M5 mid into +1 age == 22 → M2 tip age == 102
# (file444000/4@938 / file403200/4@938: tip@836 over mid 916 of M5@915).
B2_M5INTO1_TO_M2TIP102_NC = 3
B2_M5INTO1_TO_M2TIP102_INTO = 1
B2_M5INTO1_TO_M2TIP102_MID_AGE = 22
B2_M5INTO1_TO_M2TIP102_TIP_AGE = 102
# best==2, 5-cand: just-finished L, M2 tip age == 34 → M2 tip age == 36
# (file444000/2@866 / file403200/2@866: tip@830 over tip@832).
B2_M2TIP34_TO_M2TIP36_NC = 5
B2_M2TIP34_TO_M2TIP36_SRC_AGE = 34
B2_M2TIP34_TO_M2TIP36_DST_AGE = 36
# best==2, 5-cand: just-finished L, pre-byte == 165, M5 mid into +4 age == 84
# → M5 tip age == 88 (file102000/0@239 family).
B2_M5INTO4_TO_M5TIP88_NC = 5
B2_M5INTO4_TO_M5TIP88_INTO = 4
B2_M5INTO4_TO_M5TIP88_MID_AGE = 84
B2_M5INTO4_TO_M5TIP88_TIP_AGE = 88
B2_M5INTO4_TO_M5TIP88_PRE = 165
# best==2, 3-cand: just-finished L, M2 tip age == 18 → M6 mid into +2 age == 48
# (file444000/4@950 / file403200/4@950: mid 902 of M6@900 over tip@932).
B2_M2TIP18_TO_M6INTO2_NC = 3
B2_M2TIP18_TO_M6INTO2_TIP_AGE = 18
B2_M2TIP18_TO_M6INTO2_INTO = 2
B2_M2TIP18_TO_M6INTO2_MID_AGE = 48
# best==2, 4-cand: just-finished M4, M4 tip age == 4 → M2 tip age == 94
# (file444000/2@892 / file403200/2@892: tip@798 over tip@888).
B2_M4TIP4_TO_M2TIP94_NC = 4
B2_M4TIP4_TO_M2TIP94_SRC_AGE = 4
B2_M4TIP4_TO_M2TIP94_DST_AGE = 94
# best==2, 3-cand: just-finished M2, pre-byte == 16, M6 mid into +2 age == 8
# → M2 tip age == 16 (file102000/0@430 family).
B2_M6INTO2_TO_M2TIP16_NC = 3
B2_M6INTO2_TO_M2TIP16_INTO = 2
B2_M6INTO2_TO_M2TIP16_MID_AGE = 8
B2_M6INTO2_TO_M2TIP16_TIP_AGE = 16
B2_M6INTO2_TO_M2TIP16_PRE = 16
# best==3, 2-cand: just-finished L, M5 tip age == 40 → M3 tip age == 124
# (file444000/4@955 / file403200/4@955: tip@831 over tip@915).
B3_M5TIP40_TO_M3TIP124_NC = 2
B3_M5TIP40_TO_M3TIP124_SRC_AGE = 40
B3_M5TIP40_TO_M3TIP124_DST_AGE = 124
# best==3, 5-cand: just-finished L, lit tip age == 204 → M2 mid into +1 age == 18
# (file444000/2@905 / file403200/2@905: mid 887 of M2@886 over L@701).
B3_LIT204_TO_M2INTO1_NC = 5
B3_LIT204_TO_M2INTO1_LIT_AGE = 204
B3_LIT204_TO_M2INTO1_INTO = 1
B3_LIT204_TO_M2INTO1_MID_AGE = 18
# best==2, 4-cand: just-finished L, pre-byte == 24, M2 tip age == 20 → lit tip
# age == 24 (file102000/0@436 family).
B2_M2TIP20_TO_LIT24_NC = 4
B2_M2TIP20_TO_LIT24_TIP_AGE = 20
B2_M2TIP20_TO_LIT24_LIT_AGE = 24
B2_M2TIP20_TO_LIT24_PRE = 24
# best==5, 2-cand: just-finished L, lit tip age == 148 → M6 mid into +1 age == 66
# (file444000/4@967 / file403200/4@967: mid 901 of M6@900 over L@819).
B5_LIT148_TO_M6INTO1_NC = 2
B5_LIT148_TO_M6INTO1_LIT_AGE = 148
B5_LIT148_TO_M6INTO1_INTO = 1
B5_LIT148_TO_M6INTO1_MID_AGE = 66
# best==2, 3-cand: just-finished L, M3 mid into +1 age == 30 → M2 tip age == 54
# (file444000/2@940 / file403200/2@940: tip@886 over mid 910 of M3@909).
B2_M3INTO1_TO_M2TIP54_NC = 3
B2_M3INTO1_TO_M2TIP54_INTO = 1
B2_M3INTO1_TO_M2TIP54_MID_AGE = 30
B2_M3INTO1_TO_M2TIP54_TIP_AGE = 54
# best==2, 3-cand: just-finished L, pre-byte == 115, M4 tip age == 130 → lit tip
# age == 132 (file102000/0@508 family).
B2_M4TIP130_TO_LIT132_NC = 3
B2_M4TIP130_TO_LIT132_TIP_AGE = 130
B2_M4TIP130_TO_LIT132_LIT_AGE = 132
B2_M4TIP130_TO_LIT132_PRE = 115
# best==4, 2-cand: just-finished L, M6 tip age == 74 → lit tip age == 156
# (file444000/4@974 / file403200/4@974: L@818 over tip@900).
B4_M6TIP74_TO_LIT156_NC = 2
B4_M6TIP74_TO_LIT156_TIP_AGE = 74
B4_M6TIP74_TO_LIT156_LIT_AGE = 156
# best==2, 4-cand: just-finished L, M2 tip age == 6 → M3 mid into +1 age == 36
# (file444000/2@946 / file403200/2@946: mid 910 of M3@909 over tip@940).
B2_M2TIP6_TO_M3INTO1_NC = 4
B2_M2TIP6_TO_M3INTO1_TIP_AGE = 6
B2_M2TIP6_TO_M3INTO1_INTO = 1
B2_M2TIP6_TO_M3INTO1_MID_AGE = 36
# best==2, 2-cand: just-finished M3, pre-byte == 28, M2 tip age == 72 → lit tip
# age == 160 (file102000/0@516 family).
B2_M2TIP72_TO_LIT160_NC = 2
B2_M2TIP72_TO_LIT160_TIP_AGE = 72
B2_M2TIP72_TO_LIT160_LIT_AGE = 160
B2_M2TIP72_TO_LIT160_PRE = 28
# best==2, 5-cand: just-finished M2, M4 tip age == 6 → M4 tip age == 46
# (file444000/4@980 / file403200/4@980: tip@934 over tip@974).
B2_M4TIP6_TO_M4TIP46_NC = 5
B2_M4TIP6_TO_M4TIP46_SRC_AGE = 6
B2_M4TIP6_TO_M4TIP46_DST_AGE = 46
# best==2, 5-cand: just-finished L, M2 tip age == 12 → M2 tip age == 6
# (file444000/2@952 / file403200/2@952: tip@946 over tip@940).
B2_M2TIP12_TO_M2TIP6_NC = 5
B2_M2TIP12_TO_M2TIP6_SRC_AGE = 12
B2_M2TIP12_TO_M2TIP6_DST_AGE = 6
# best==2, 2-cand: just-finished M4, pre-byte == 28, pick M3 mid into +1 age
# == 46 → force lit (file102000/0@524 family).
FORCE_LIT_M3INTO1_NC = 2
FORCE_LIT_M3INTO1_BEST = 2
FORCE_LIT_M3INTO1_INTO = 1
FORCE_LIT_M3INTO1_MID_AGE = 46
FORCE_LIT_M3INTO1_PRE = 28
# best==3, 2-cand: just-finished L, M3 tip age == 24 → M2 mid into +1 age == 34
# (file444000/2@1005 / file403200/2@1005: mid 971 of M2@970 over tip@981).
B3_M3TIP24_TO_M2INTO1_NC = 2
B3_M3TIP24_TO_M2INTO1_TIP_AGE = 24
B3_M3TIP24_TO_M2INTO1_INTO = 1
B3_M3TIP24_TO_M2INTO1_MID_AGE = 34
# best==2, 6-cand: just-finished M5, M2 tip age == 8 → M2 tip age == 80
# (file444000/4@988 / file403200/4@988: tip@908 over tip@980).
B2_M2TIP8_TO_M2TIP80_NC = 6
B2_M2TIP8_TO_M2TIP80_SRC_AGE = 8
B2_M2TIP8_TO_M2TIP80_DST_AGE = 80
# sole L tip age == 52, best_len == 2, just-finished L, pre-byte == 49 → force
# lit (file102000/0@526 family; match from old lit is encodable but retail lits).
SOLE_LIT52_FORCE_AGE = 52
SOLE_LIT52_FORCE_BEST = 2
SOLE_LIT52_FORCE_PRE = 49
# best==2, 3-cand: just-finished M2, pre-byte == 0, lit tip age == 8 → M2 mid
# into +1 age == 1 (file444000/2@1022 / file403200/2@1022: mid 1021 of M2@1020
# over L@1014).
B2_LIT8_TO_M2INTO1_NC = 3
B2_LIT8_TO_M2INTO1_LIT_AGE = 8
B2_LIT8_TO_M2INTO1_INTO = 1
B2_LIT8_TO_M2INTO1_MID_AGE = 1
B2_LIT8_TO_M2INTO1_PRE = 0
# best==2, 7-cand: just-finished M2, M2 tip age == 10 → M2 tip age == 2
# (file444000/4@990 / file403200/4@990: tip@988 over tip@980).
B2_M2TIP10_TO_M2TIP2_NC = 7
B2_M2TIP10_TO_M2TIP2_SRC_AGE = 10
B2_M2TIP10_TO_M2TIP2_DST_AGE = 2
# best==2, 3-cand: just-finished L, pre-byte == 132, M4 mid into +2 age == 44
# → M2 tip age == 66 (file444000/3@162 / file403200/3@162).
B2_M4INTO2_TO_M2TIP66_NC = 3
B2_M4INTO2_TO_M2TIP66_INTO = 2
B2_M4INTO2_TO_M2TIP66_MID_AGE = 44
B2_M4INTO2_TO_M2TIP66_TIP_AGE = 66
B2_M4INTO2_TO_M2TIP66_PRE = 132
# best==2, 3-cand: just-finished L, pre-byte == 70, pick M2 tip age == 122 →
# force lit (file102000/0@532 family).
FORCE_LIT_M2TIP122_NC = 3
FORCE_LIT_M2TIP122_BEST = 2
FORCE_LIT_M2TIP122_TIP_AGE = 122
FORCE_LIT_M2TIP122_PRE = 70
# best==3, 3-cand: just-finished L, M2 mid into +1 age == 8 → M4 mid into +3
# age == 24 (file444000/4@1001 / file403200/4@1001).
B3_M2INTO1_TO_M4INTO3_NC = 3
B3_M2INTO1_TO_M4INTO3_SRC_INTO = 1
B3_M2INTO1_TO_M4INTO3_SRC_AGE = 8
B3_M2INTO1_TO_M4INTO3_DST_INTO = 3
B3_M2INTO1_TO_M4INTO3_DST_AGE = 24
# best==3, 2-cand: just-finished L, lit tip age == 120 → M4 mid into +1 age == 98
# (file444000/3@215 / file403200/3@215).
B3_LIT120_TO_M4INTO1_NC = 2
B3_LIT120_TO_M4INTO1_LIT_AGE = 120
B3_LIT120_TO_M4INTO1_INTO = 1
B3_LIT120_TO_M4INTO1_MID_AGE = 98
# best==3, 2-cand: just-finished L, M4 tip age == 18 → M3 mid into +1 age == 24
# (file102000/0@538 family).
B3_M4TIP18_TO_M3INTO1_NC = 2
B3_M4TIP18_TO_M3INTO1_TIP_AGE = 18
B3_M4TIP18_TO_M3INTO1_INTO = 1
B3_M4TIP18_TO_M3INTO1_MID_AGE = 24
# best==2, 5-cand: just-finished L, M3 mid into +1 age == 32 → lit tip age == 166
# (file444000/3@248 / file403200/3@248).
B2_M3INTO1_TO_LIT166_NC = 5
B2_M3INTO1_TO_LIT166_INTO = 1
B2_M3INTO1_TO_LIT166_MID_AGE = 32
B2_M3INTO1_TO_LIT166_LIT_AGE = 166
# not_max: sole M2 tip age == 140, best_len == 3, pre-byte == 70, just-finished
# M2 → emit len 2 from M3 mid into +1 age == 12 (file102000/0@546 family).
NOT_MAX_M2TIP140_BEST = 3
NOT_MAX_M2TIP140_SRC_AGE = 140
NOT_MAX_M2TIP140_PRE = 70
NOT_MAX_M2TIP140_DST_INTO = 1
NOT_MAX_M2TIP140_DST_OWNER_PL = 3
NOT_MAX_M2TIP140_DST_AGE = 12
NOT_MAX_M2TIP140_EMIT_LEN = 2
# best==2, 5-cand: just-finished L, M4 tip age == 36 → M2 tip age == 86
# (file444000/3@266 / file403200/3@266: tip@180 over tip@230).
B2_M4TIP36_TO_M2TIP86_NC = 5
B2_M4TIP36_TO_M2TIP86_SRC_AGE = 36
B2_M4TIP36_TO_M2TIP86_DST_AGE = 86
# best==2, 2-cand: just-finished L, pre-byte == 12, M2 tip age == 42 → M2 mid
# into +1 age == 43 (file102000/0@552 family).
B2_M2TIP42_TO_M2INTO1_NC = 2
B2_M2TIP42_TO_M2INTO1_TIP_AGE = 42
B2_M2TIP42_TO_M2INTO1_INTO = 1
B2_M2TIP42_TO_M2INTO1_MID_AGE = 43
B2_M2TIP42_TO_M2INTO1_PRE = 12
# best==2, 6-cand: just-finished L, M2 tip age == 44 → M4 tip age == 80
# (file444000/3@310 / file403200/3@310: tip@230 over tip@266).
B2_M2TIP44_TO_M4TIP80_NC = 6
B2_M2TIP44_TO_M4TIP80_SRC_AGE = 44
B2_M2TIP44_TO_M4TIP80_DST_AGE = 80
# best==2, 3-cand: just-finished M2, pre-byte == 0, pick M2 tip age == 128 →
# force lit (file102000/0@554 family).
FORCE_LIT_M2TIP128_NC = 3
FORCE_LIT_M2TIP128_BEST = 2
FORCE_LIT_M2TIP128_TIP_AGE = 128
FORCE_LIT_M2TIP128_PRE = 0
# best==2, 2-cand: just-finished M2, pre-byte == 148, lit tip age == 236 → M2
# tip age == 142 (file444000/3@392 / file403200/3@392).
B2_LIT236_TO_M2TIP142_NC = 2
B2_LIT236_TO_M2TIP142_LIT_AGE = 236
B2_LIT236_TO_M2TIP142_TIP_AGE = 142
B2_LIT236_TO_M2TIP142_PRE = 148
# best==3, 5-cand: just-finished M4, pre-byte == 0, M4 mid into +3 age == 1 →
# M2 mid into +1 age == 58 (file102000/0@567 family).
B3_M4INTO3_TO_M2INTO1_NC = 5
B3_M4INTO3_TO_M2INTO1_SRC_INTO = 3
B3_M4INTO3_TO_M2INTO1_SRC_AGE = 1
B3_M4INTO3_TO_M2INTO1_DST_INTO = 1
B3_M4INTO3_TO_M2INTO1_DST_AGE = 58
B3_M4INTO3_TO_M2INTO1_PRE = 0
# best==4, 2-cand: just-finished M4, pre-byte == 13, lit tip age == 46 → M4 tip
# age == 4 (file102000/0@642 family).
B4_LIT46_TO_M4TIP4_NC = 2
B4_LIT46_TO_M4TIP4_LIT_AGE = 46
B4_LIT46_TO_M4TIP4_TIP_AGE = 4
B4_LIT46_TO_M4TIP4_PRE = 13
# best==2, 5-cand: just-finished M4, pre-byte == 13, M4 mid into +2 age == 2
# → M2 tip age == 48 (file102000/0@646 family).
B2_M4INTO2_TO_M2TIP48_NC = 5
B2_M4INTO2_TO_M2TIP48_INTO = 2
B2_M4INTO2_TO_M2TIP48_MID_AGE = 2
B2_M4INTO2_TO_M2TIP48_TIP_AGE = 48
B2_M4INTO2_TO_M2TIP48_PRE = 13
# best==2, 3-cand: just-finished M2, pre-byte == 13, M6 mid into +1 age == 22
# → lit tip age == 30 (file102000/0@648 family).
B2_M6INTO1_TO_LIT30_NC = 3
B2_M6INTO1_TO_LIT30_INTO = 1
B2_M6INTO1_TO_LIT30_MID_AGE = 22
B2_M6INTO1_TO_LIT30_LIT_AGE = 30
B2_M6INTO1_TO_LIT30_PRE = 13
# best==2, 7-cand: just-finished L, pre-byte == 8, M4 tip age == 26 → M2 tip
# age == 42 (file102000/0@688 family).
B2_M4TIP26_TO_M2TIP42_NC = 7
B2_M4TIP26_TO_M2TIP42_SRC_AGE = 26
B2_M4TIP26_TO_M2TIP42_DST_AGE = 42
B2_M4TIP26_TO_M2TIP42_PRE = 8
# best==2, 2-cand: just-finished M2, pre-byte == 13, M2 tip age == 38 → lit tip
# age == 40 (file102000/0@736 family).
B2_M2TIP38_TO_LIT40_NC = 2
B2_M2TIP38_TO_LIT40_SRC_AGE = 38
B2_M2TIP38_TO_LIT40_LIT_AGE = 40
B2_M2TIP38_TO_LIT40_PRE = 13
# best==2, 2-cand: just-finished M3, pre-byte == 17, M3 mid into +1 age == 2
# → lit tip age == 32 (file102000/0@742 family).
B2_M3INTO1_TO_LIT32_NC = 2
B2_M3INTO1_TO_LIT32_INTO = 1
B2_M3INTO1_TO_LIT32_MID_AGE = 2
B2_M3INTO1_TO_LIT32_LIT_AGE = 32
B2_M3INTO1_TO_LIT32_PRE = 17
# best==2, 2-cand: just-finished M3, pre-byte == 8, M2 tip age == 12 → lit tip
# age == 86 (file102000/0@768 family).
B2_M2TIP12_TO_LIT86_NC = 2
B2_M2TIP12_TO_LIT86_SRC_AGE = 12
B2_M2TIP12_TO_LIT86_LIT_AGE = 86
B2_M2TIP12_TO_LIT86_PRE = 8
# best==2, 8-cand: just-finished L, pre-byte == 16, M3 mid into +1 age == 218
# → M3 mid into +1 age == 225 (file102000/0@786 family).
B2_M3INTO1_218_TO_225_NC = 8
B2_M3INTO1_218_TO_225_INTO = 1
B2_M3INTO1_218_TO_225_SRC_AGE = 218
B2_M3INTO1_218_TO_225_DST_AGE = 225
B2_M3INTO1_218_TO_225_PRE = 16
# best==2, 9-cand: just-finished L, pre-byte == 46, M2 tip age == 16 → M3 mid
# into +1 age == 234 (file102000/0@802 family).
B2_M2TIP16_TO_M3INTO1_234_NC = 9
B2_M2TIP16_TO_M3INTO1_234_SRC_AGE = 16
B2_M2TIP16_TO_M3INTO1_234_INTO = 1
B2_M2TIP16_TO_M3INTO1_234_MID_AGE = 234
B2_M2TIP16_TO_M3INTO1_234_PRE = 46
# best==2, 2-cand: just-finished L, pre-byte == 8, M2 tip age == 56 → M2 tip
# age == 72 (file102000/0@858 family).
B2_M2TIP56_TO_M2TIP72_NC = 2
B2_M2TIP56_TO_M2TIP72_SRC_AGE = 56
B2_M2TIP56_TO_M2TIP72_DST_AGE = 72
B2_M2TIP56_TO_M2TIP72_PRE = 8
# best==2, 5-cand: just-finished L, pre-byte == 4, M2 tip age == 8 → M2 tip
# age == 6 (file102000/0@866 family).
B2_M2TIP8_TO_M2TIP6_NC = 5
B2_M2TIP8_TO_M2TIP6_SRC_AGE = 8
B2_M2TIP8_TO_M2TIP6_DST_AGE = 6
B2_M2TIP8_TO_M2TIP6_PRE = 4
# best==2, 2-cand: just-finished M3, pre-byte == 24, M2 tip age == 100 → lit
# tip age == 108 (file102000/0@916 family).
B2_M2TIP100_TO_LIT108_NC = 2
B2_M2TIP100_TO_LIT108_SRC_AGE = 100
B2_M2TIP100_TO_LIT108_LIT_AGE = 108
B2_M2TIP100_TO_LIT108_PRE = 24
# best==2, 3-cand: just-finished M2, pre-byte == 28, M2 tip age == 2 → lit tip
# age == 106 (file102000/0@928 family).
B2_M2TIP2_TO_LIT106_NC = 3
B2_M2TIP2_TO_LIT106_SRC_AGE = 2
B2_M2TIP2_TO_LIT106_LIT_AGE = 106
B2_M2TIP2_TO_LIT106_PRE = 28
# best==3, 2-cand: just-finished L, pre-byte == 10, lit tip age == 10 → M4 mid
# into +1 age == 8 (file102000/0@931 family).
B3_LIT10_TO_M4INTO1_8_NC = 2
B3_LIT10_TO_M4INTO1_8_LIT_AGE = 10
B3_LIT10_TO_M4INTO1_8_INTO = 1
B3_LIT10_TO_M4INTO1_8_MID_AGE = 8
B3_LIT10_TO_M4INTO1_8_PRE = 10
# best==2, 3-cand: just-finished M2, pre-byte == 57, M4 tip age == 10 → lit tip
# age == 12 (file102000/0@970 family).
B2_M4TIP10_TO_LIT12_NC = 3
B2_M4TIP10_TO_LIT12_SRC_AGE = 10
B2_M4TIP10_TO_LIT12_LIT_AGE = 12
B2_M4TIP10_TO_LIT12_PRE = 57
# best==2, 6-cand: just-finished L, pre-byte == 127, M2 tip age == 156 → M2 tip
# age == 236 (file102000/0@1022 family).
B2_M2TIP156_TO_M2TIP236_NC = 6
B2_M2TIP156_TO_M2TIP236_SRC_AGE = 156
B2_M2TIP156_TO_M2TIP236_DST_AGE = 236
B2_M2TIP156_TO_M2TIP236_PRE = 127
# best==2, 6-cand: just-finished L, pre-byte == 91, M11 mid into +6 age == 6
# → M11 mid into +8 age == 4 (file20209@174).
B2_M11INTO6_TO_INTO8_NC = 6
B2_M11INTO6_TO_INTO8_SRC_INTO = 6
B2_M11INTO6_TO_INTO8_SRC_AGE = 6
B2_M11INTO6_TO_INTO8_DST_INTO = 8
B2_M11INTO6_TO_INTO8_DST_AGE = 4
B2_M11INTO6_TO_INTO8_PRE = 91
# best==3, 5-cand: just-finished L, pre-byte == 26, lit tip age == 16 → M11 mid
# into +7 age == 8 (file20209@177).
B3_LIT16_TO_M11INTO7_NC = 5
B3_LIT16_TO_M11INTO7_LIT_AGE = 16
B3_LIT16_TO_M11INTO7_INTO = 7
B3_LIT16_TO_M11INTO7_MID_AGE = 8
B3_LIT16_TO_M11INTO7_PRE = 26
# best==3, 4-cand: just-finished L, pre-byte == 58, M7 mid into +2 age == 8
# → M7 mid into +4 age == 6 (file20209@203).
B3_M7INTO2_TO_INTO4_NC = 4
B3_M7INTO2_TO_INTO4_SRC_INTO = 2
B3_M7INTO2_TO_INTO4_SRC_AGE = 8
B3_M7INTO2_TO_INTO4_DST_INTO = 4
B3_M7INTO2_TO_INTO4_DST_AGE = 6
B3_M7INTO2_TO_INTO4_PRE = 58
# best==5, 5-cand: just-finished L, pre-byte == 103, lit tip age == 16 → M13 mid
# into +7 age == 8 (file20209@328).
B5_LIT16_TO_M13INTO7_NC = 5
B5_LIT16_TO_M13INTO7_LIT_AGE = 16
B5_LIT16_TO_M13INTO7_INTO = 7
B5_LIT16_TO_M13INTO7_MID_AGE = 8
B5_LIT16_TO_M13INTO7_PRE = 103
# best==4, 2-cand: just-finished M2, pre-byte == 158, lit tip age == 22 → M6 tip
# age == 8 (file20209@399).
B4_LIT22_TO_M6TIP8_NC = 2
B4_LIT22_TO_M6TIP8_LIT_AGE = 22
B4_LIT22_TO_M6TIP8_TIP_AGE = 8
B4_LIT22_TO_M6TIP8_PRE = 158
# best==7, 3-cand: just-finished M8, pre-byte == 190, M8 mid into +4 age == 4
# → M8 mid into +6 age == 2 (file20209@417).
B7_M8INTO4_TO_INTO6_NC = 3
B7_M8INTO4_TO_INTO6_SRC_INTO = 4
B7_M8INTO4_TO_INTO6_SRC_AGE = 4
B7_M8INTO4_TO_INTO6_DST_INTO = 6
B7_M8INTO4_TO_INTO6_DST_AGE = 2
B7_M8INTO4_TO_INTO6_PRE = 190
# best==4, 2-cand: just-finished M8, pre-byte == 45, M8 tip age == 8 → lit tip
# age == 14 (file20121@226).
B4_M8TIP8_TO_LIT14_NC = 2
B4_M8TIP8_TO_LIT14_SRC_AGE = 8
B4_M8TIP8_TO_LIT14_LIT_AGE = 14
B4_M8TIP8_TO_LIT14_PRE = 45
# best==2, 3-cand: just-finished L, pre-byte == 148, M2 tip age == 18 → M2 tip
# age == 36 (file503500/4@558).
B2_M2TIP18_TO_M2TIP36_NC = 3
B2_M2TIP18_TO_M2TIP36_SRC_AGE = 18
B2_M2TIP18_TO_M2TIP36_DST_AGE = 36
B2_M2TIP18_TO_M2TIP36_PRE = 148
# best==3, 3-cand: just-finished M2, pre-byte == 58, lit tip age == 10 → M6 mid
# into +3 age == 6 (file20235@242).
B3_LIT10_TO_M6INTO3_NC = 3
B3_LIT10_TO_M6INTO3_LIT_AGE = 10
B3_LIT10_TO_M6INTO3_INTO = 3
B3_LIT10_TO_M6INTO3_MID_AGE = 6
B3_LIT10_TO_M6INTO3_PRE = 58
# best==2, 2-cand: just-finished L, pre-byte == 94, M2 tip age == 26 → lit tip
# age == 68 (file20702/20711@159).
B2_M2TIP26_TO_LIT68_NC = 2
B2_M2TIP26_TO_LIT68_SRC_AGE = 26
B2_M2TIP26_TO_LIT68_LIT_AGE = 68
B2_M2TIP26_TO_LIT68_PRE = 94
# best==4, 2-cand: just-finished M2, pre-byte == 140, M4 tip age == 14 → lit tip
# age == 202 (file104900/2 family).
B4_M4TIP14_TO_LIT202_NC = 2
B4_M4TIP14_TO_LIT202_SRC_AGE = 14
B4_M4TIP14_TO_LIT202_LIT_AGE = 202
B4_M4TIP14_TO_LIT202_PRE = 140
# best==2, 2-cand: just-finished L, pre-byte == 110, M3 tip age == 8 → M3 mid
# into +2 age == 10 (file20220@279).
B2_M3TIP8_TO_INTO2_NC = 2
B2_M3TIP8_TO_INTO2_SRC_AGE = 8
B2_M3TIP8_TO_INTO2_INTO = 2
B2_M3TIP8_TO_INTO2_MID_AGE = 10
B2_M3TIP8_TO_INTO2_PRE = 110
# best==2, 2-cand: just-finished L, pre-byte == 78, M2 tip age == 20 → lit tip
# age == 34 (file20212@222).
B2_M2TIP20_TO_LIT34_NC = 2
B2_M2TIP20_TO_LIT34_SRC_AGE = 20
B2_M2TIP20_TO_LIT34_LIT_AGE = 34
B2_M2TIP20_TO_LIT34_PRE = 78
# best==2, 3-cand: just-finished L, pre-byte == 33, M4 tip age == 6 → M2 tip
# age == 226 (file402200@278).
B2_M4TIP6_TO_M2TIP226_NC = 3
B2_M4TIP6_TO_M2TIP226_SRC_AGE = 6
B2_M4TIP6_TO_M2TIP226_DST_AGE = 226
B2_M4TIP6_TO_M2TIP226_PRE = 33
# 8cf177. 20702@189: best==2 nc3 pre126 jf('L',) ('Mtip', 2, 30)→('Mtip', 2, 56)
R177_NC = 3
R177_PRE = 126
R177_BEST = 2
# 8cf178. 104900@536: best==2 nc4 pre160 jf('L',) ('Mtip', 2, 8)→('Mmid', 4, 2, 18)
R178_NC = 4
R178_PRE = 160
R178_BEST = 2
# 8cf179. 104900@550: best==2 nc4 pre152 jf('M', 4) ('Mtip', 2, 26)→('Mtip', 2, 30)
R179_NC = 4
R179_PRE = 152
R179_BEST = 2
# 8cf180. 104900@572: best==2 nc5 pre148 jf('L',) ('Mtip', 2, 22)→('L', 236)
R180_NC = 5
R180_PRE = 148
R180_BEST = 2
# 8cf181. 104900@666: best==2 nc2 pre132 jf('L',) ('Mmid', 2, 1, 14)→('Mtip', 2, 4)
R181_NC = 2
R181_PRE = 132
R181_BEST = 2
# 8cf182. 104900@698: best==2 nc2 pre157 jf('L',) ('Mmid', 2, 1, 28)→('L', 38)
R182_NC = 2
R182_PRE = 157
R182_BEST = 2
# 8cf183. 104900@718: best==2 nc4 pre140 jf('M', 4) ('Mmid', 3, 1, 46)→('Mtip', 2, 52)
R183_NC = 4
R183_PRE = 140
R183_BEST = 2
# 8cf184. 104900@729: best==2 nc2 pre231 jf('M', 2) ('Mmid', 2, 1, 136)→('Mmid', 4, 3, 210)
R184_NC = 2
R184_PRE = 231
R184_BEST = 2
# 8cf185. 104900@816: best==2 nc2 pre144 jf('L',) ('Mtip', 2, 250)→('Mtip', 2, 18)
R185_NC = 2
R185_PRE = 144
R185_BEST = 2
# 8cf186. 503500@629: best==3 nc2 pre172 jf('L',) ('Mmid', 5, 2, 42)→('Mtip', 2, 100)
R186_NC = 2
R186_PRE = 172
R186_BEST = 3
# 8cf187. 503500@715: best==2 nc2 pre104 jf('L',) ('Mtip', 3, 104)→('L', 158)
R187_NC = 2
R187_PRE = 104
R187_BEST = 2
# 8cf188. 503500@801: best==2 nc3 pre7 jf('L',) ('Mmid', 3, 2, 4)→('L', 40)
R188_NC = 3
R188_PRE = 7
R188_BEST = 2
# 8cf189. 503500@886: best==2 nc2 pre188 jf('L',) ('Mtip', 3, 6)→('L', 10)
R189_NC = 2
R189_PRE = 188
R189_BEST = 2
# 8cf190. 503500@892: best==2 nc4 pre184 jf('L',) ('Mtip', 3, 4)→('Mtip', 3, 12)
R190_NC = 4
R190_PRE = 184
R190_BEST = 2
# 8cf191. 20121@263: best==2 nc3 pre41 jf('M', 2) ('L', 10)→('Mtip', 3, 9)
R191_NC = 3
R191_PRE = 41
R191_BEST = 2
# 8cf192. 20121@308: best==4 nc4 pre28 jf('M', 4) ('Mmid', 7, 2, 12)→('Mtip', 7, 14)
R192_NC = 4
R192_PRE = 28
R192_BEST = 4
# 8cf193. 20121@369: best==9 nc3 pre198 jf('M', 8) ('Mmid', 8, 4, 4)→('Mmid', 8, 6, 2)
R193_NC = 3
R193_PRE = 198
R193_BEST = 9
# 8cf194. 20121@430: best==6 nc3 pre12 jf('L',) ('Mmid', 8, 5, 10)→('Mmid', 8, 7, 8)
R194_NC = 3
R194_PRE = 12
R194_BEST = 6
# 8cf195. 20121@463: best==7 nc5 pre99 jf('M', 10) ('L', 18)→('Mmid', 10, 8, 2)
R195_NC = 5
R195_PRE = 99
R195_BEST = 7
# 8cf196. 20235@284: best==2 nc2 pre53 jf('L',) ('Mtip', 3, 4)→('L', 8)
R196_NC = 2
R196_PRE = 53
R196_BEST = 2
# 8cf197. 20235@311: best==2 nc2 pre141 jf('M', 2) ('Mtip', 2, 2)→('Mmid', 3, 2, 6)
R197_NC = 2
R197_PRE = 141
R197_BEST = 2
# 8cf198. 20235@339: best==2 nc3 pre107 jf('M', 3) ('Mmid', 3, 1, 2)→('Mtip', 2, 6)
R198_NC = 3
R198_PRE = 107
R198_BEST = 2
# 8cf199. 20235@462: best==2 nc2 pre12 jf('L',) ('L', 6)→('Mmid', 4, 1, 4)
R199_NC = 2
R199_PRE = 12
R199_BEST = 2
# 8cf200. 20235@481: best==2 nc2 pre66 jf('M', 3) ('Mtip', 3, 63)→('L', 67)
R200_NC = 2
R200_PRE = 66
R200_BEST = 2
# 8cf201. 20235@510: best==2 nc18 pre0 jf('M', 17) ('Mmid', 17, 15, 2)→('Mmid', 17, 16, 1)
R201_NC = 18
R201_PRE = 0
R201_BEST = 2
# 8cf202. 402200@346: best==2 nc3 pre8 jf('L',) ('Mtip', 2, 34)→('Mtip', 2, 36)
R202_NC = 3
R202_PRE = 8
R202_BEST = 2
# 8cf203. 402200@358: best==2 nc2 pre0 jf('M', 2) ('Mtip', 2, 6)→('Mtip', 2, 118)
R203_NC = 2
R203_PRE = 0
R203_BEST = 2
# 8cf204. 402200@400: best==2 nc4 pre16 jf('M', 2) ('Mtip', 2, 54)→('Mtip', 2, 90)
R204_NC = 4
R204_PRE = 16
R204_BEST = 2
# 8cf205. 402200@414: best==2 nc6 pre24 jf('M', 2) ('Mtip', 4, 10)→('L', 112)
R205_NC = 6
R205_PRE = 24
R205_BEST = 2
# 8cf206. 402200@434: best==2 nc3 pre41 jf('M', 2) ('Mtip', 2, 14)→('L', 116)
R206_NC = 3
R206_PRE = 41
R206_BEST = 2
# 8cf207. 402200@510: best==2 nc3 pre90 jf('L',) ('Mtip', 2, 142)→('Mtip', 2, 152)
R207_NC = 3
R207_PRE = 90
R207_BEST = 2
# 8cf208. 20220@308: best==2 nc2 pre45 jf('M', 2) ('Mmid', 3, 1, 4)→('L', 14)
R208_NC = 2
R208_PRE = 45
R208_BEST = 2
# 8cf209. 20220@344: best==2 nc6 pre45 jf('M', 4) ('Mmid', 5, 1, 8)→('Mmid', 5, 3, 14)
R209_NC = 6
R209_PRE = 45
R209_BEST = 2
# 8cf210. 20220@350: best==2 nc4 pre49 jf('M', 2) ('Mmid', 4, 2, 8)→('Mmid', 5, 3, 12)
R210_NC = 4
R210_PRE = 49
R210_BEST = 2
# 8cf211. 20220@353: best==3 nc4 pre145 jf('L',) ('Mmid', 4, 1, 12)→('Mmid', 5, 4, 22)
R211_NC = 4
R211_PRE = 145
R211_BEST = 3
# 8cf212. 20220@376: best==2 nc2 pre45 jf('M', 2) ('Mtip', 2, 2)→('L', 6)
R212_NC = 2
R212_PRE = 45
R212_BEST = 2
# 8cf213. 20220@429: best==3 nc2 pre210 jf('L',) ('L', 18)→('Mmid', 6, 1, 6)
R213_NC = 2
R213_PRE = 210
R213_BEST = 3
# 8cf214. 20220@466: best==2 nc2 pre62 jf('L',) ('Mtip', 2, 4)→('L', 18)
R214_NC = 2
R214_PRE = 62
R214_BEST = 2
# 8cf215. 20212@361: best==2 nc2 pre215 jf('L',) ('Mtip', 2, 24)→('L', 28)
R215_NC = 2
R215_PRE = 215
R215_BEST = 2
# 8cf216. 20212@400: best==2 nc3 pre91 jf('L',) ('Mtip', 3, 4)→('Mtip', 2, 14)
R216_NC = 3
R216_PRE = 91
R216_BEST = 2
# 8cf217. 20212@453: best==2 nc4 pre25 jf('L',) ('Mtip', 3, 12)→('Mtip', 2, 20)
R217_NC = 4
R217_PRE = 25
R217_BEST = 2
# 8cf218. 20212@498: best==2 nc3 pre107 jf('L',) ('Mtip', 7, 8)→('Mmid', 7, 4, 4)
R218_NC = 3
R218_PRE = 107
R218_BEST = 2
# pe2pkg multi_max cascade (all ≤4K misses are impure later, pure until first miss)
# 8cf219. best==2, 3-cand: pre-byte == 0, M2 tip age == 28 → M4 tip age == 20
# (file50146@124).
B2_M2TIP28_TO_M4TIP20_NC = 3
B2_M2TIP28_TO_M4TIP20_SRC_AGE = 28
B2_M2TIP28_TO_M4TIP20_DST_AGE = 20
B2_M2TIP28_TO_M4TIP20_PRE = 0
# 8cf220. best==3, 2-cand: pre-byte == 75, M3 tip age == 4 → M3 mid into +1
# age == 12 (file20600@14).
B3_M3TIP4_TO_INTO1_12_NC = 2
B3_M3TIP4_TO_INTO1_12_SRC_AGE = 4
B3_M3TIP4_TO_INTO1_12_INTO = 1
B3_M3TIP4_TO_INTO1_12_MID_AGE = 12
B3_M3TIP4_TO_INTO1_12_PRE = 75
# 8cf221. best==4, 3-cand: pre-byte == 142, M4 tip age == 16 → M3 tip age == 44
# (file50119/20/21@136).
B4_M4TIP16_TO_M3TIP44_NC = 3
B4_M4TIP16_TO_M3TIP44_SRC_AGE = 16
B4_M4TIP16_TO_M3TIP44_DST_AGE = 44
B4_M4TIP16_TO_M3TIP44_PRE = 142
# 8cf222. best==3, 33-cand: pre-byte == 137, M6 mid into +3 age == 24 → M3 tip
# age == 36 (file30406/07@77).
B3_M6INTO3_TO_M3TIP36_NC = 33
B3_M6INTO3_TO_M3TIP36_INTO = 3
B3_M6INTO3_TO_M3TIP36_MID_AGE = 24
B3_M6INTO3_TO_M3TIP36_TIP_AGE = 36
B3_M6INTO3_TO_M3TIP36_PRE = 137
# 8cf223. best==3, 33-cand: pre-byte == 117, M3 tip age == 24 → M7 mid into +4
# age == 16 (file30300/01@81).
B3_M3TIP24_TO_M7INTO4_NC = 33
B3_M3TIP24_TO_M7INTO4_SRC_AGE = 24
B3_M3TIP24_TO_M7INTO4_INTO = 4
B3_M3TIP24_TO_M7INTO4_MID_AGE = 16
B3_M3TIP24_TO_M7INTO4_PRE = 117
# 8cf224. pe2pkg 50146@136: best==4 nc3 pre142 jf('L',) ('Mtip', 4, 16)→('Mtip', 3, 64)
R224_NC = 3
R224_PRE = 142
R224_BEST = 4
# 8cf225. pe2pkg 50146@152: best==4 nc4 pre16 jf('M', 3) ('Mtip', 4, 16)→('Mtip', 4, 72)
R225_NC = 4
R225_PRE = 16
R225_BEST = 4
# 8cf226. pe2pkg 50146@161: best==2 nc3 pre30 jf('L',) ('Mmid', 3, 1, 32)→('Mtip', 2, 44)
R226_NC = 3
R226_PRE = 30
R226_BEST = 2
# 8cf227. pe2pkg 50146@168: best==4 nc5 pre8 jf('M', 3) ('Mtip', 4, 16)→('Mtip', 4, 48)
R227_NC = 5
R227_PRE = 8
R227_BEST = 4
# 8cf228. pe2pkg 30406@121: best==3 nc44 pre32 jf('L',) ('Mmid', 4, 1, 16)→('Mmid', 7, 4, 20)
R228_NC = 44
R228_PRE = 32
R228_BEST = 3
# 8cf229. pe2pkg 30406@125: best==3 nc45 pre40 jf('L',) ('Mtip', 7, 16)→('Mmid', 6, 3, 72)
R229_NC = 45
R229_PRE = 40
R229_BEST = 3
# 8cf230. pe2pkg 30406@164: best==3 nc2 pre0 jf('M', 2) ('L', 36)→('Mmid', 7, 3, 24)
R230_NC = 2
R230_PRE = 0
R230_BEST = 3
# 8cf231. pe2pkg 30406@174: best==2 nc3 pre0 jf('M', 5) ('Mtip', 4, 18)→('Mtip', 2, 42)
R231_NC = 3
R231_PRE = 0
R231_BEST = 2
# 8cf232. pe2pkg 30300@97: best==3 nc35 pre102 jf('L',) ('Mtip', 3, 16)→('Mtip', 3, 48)
R232_NC = 35
R232_PRE = 102
R232_BEST = 3
# 8cf233. pe2pkg 30300@121: best==3 nc38 pre72 jf('L',) ('Mtip', 3, 24)→('Mtip', 3, 40)
R233_NC = 38
R233_PRE = 72
R233_BEST = 3
# 8cf234. pe2pkg 30300@161: best==3 nc46 pre8 jf('L',) ('Mtip', 3, 12)→('Mtip', 3, 40)
R234_NC = 46
R234_PRE = 8
R234_BEST = 3
# 8cf235. pe2pkg 30300@180: best==4 nc3 pre0 jf('M', 2) ('Mtip', 4, 12)→('L', 36)
R235_NC = 3
R235_PRE = 0
R235_BEST = 4
# 8cf236. pe2pkg 20600@60: best==5 nc3 pre173 jf('L',) ('Mtip', 5, 20)→('Mtip', 5, 32)
R236_NC = 3
R236_PRE = 173
R236_BEST = 5
# 8cf237. pe2pkg 20600@70: best==3 nc6 pre82 jf('M', 2) ('Mmid', 5, 2, 8)→('Mmid', 5, 2, 28)
R237_NC = 6
R237_PRE = 82
R237_BEST = 3
# 8cf238. pe2pkg 20600@88: best==5 nc4 pre106 jf('L',) ('Mtip', 5, 28)→('Mtip', 5, 48)
R238_NC = 4
R238_PRE = 106
R238_BEST = 5
# 8cf239. pe2pkg 20600@120: best==2 nc2 pre125 jf('L',) ('Mtip', 3, 4)→('L', 98)
R239_NC = 2
R239_PRE = 125
R239_BEST = 2
# 8cf240. pe2pkg file50146@193: best==2 nc3 pre52 jf('L',) ('Mmid', 2, 1, 68)→('L', 144)
R240_NC = 3
R240_PRE = 52
R240_BEST = 2
# 8cf241. pe2pkg file50146@217: best==2 nc2 pre26 jf('L',) ('Mmid', 2, 1, 8)→('L', 84)
R241_NC = 2
R241_PRE = 26
R241_BEST = 2
# 8cf242. pe2pkg file50146@224: best==4 nc8 pre20 jf('L',) ('Mmid', 8, 4, 36)→('Mtip', 4, 72)
R242_NC = 8
R242_PRE = 20
R242_BEST = 4
# 8cf243. pe2pkg file50146@257: best==3 nc2 pre18 jf('L',) ('Mmid', 3, 2, 106)→('Mtip', 5, 78)
R243_NC = 2
R243_PRE = 18
R243_BEST = 3
# 8cf244. pe2pkg file50146@306: best==2 nc7 pre22 jf('L',) ('Mmid', 3, 1, 3)→('Mtip', 2, 62)
R244_NC = 7
R244_PRE = 22
R244_BEST = 2
# 8cf245. pe2pkg file50146@314: best==2 nc8 pre54 jf('L',) ('Mtip', 2, 8)→('Mtip', 2, 32)
R245_NC = 8
R245_PRE = 54
R245_BEST = 2
# 8cf246. pe2pkg file50146@380: best==3 nc2 pre174 jf('M', 4) ('Mtip', 3, 40)→('Mtip', 2, 56)
R246_NC = 2
R246_PRE = 174
R246_BEST = 3
# 8cf247. pe2pkg file50146@384: best==4 nc10 pre146 jf('L',) ('Mmid', 5, 1, 16)→('Mmid', 5, 1, 52)
R247_NC = 10
R247_PRE = 146
R247_BEST = 4
# 8cf248. pe2pkg file30406@181: best==3 nc4 pre254 jf('L',) ('L', 53)→('Mtip', 4, 5)
R248_NC = 4
R248_PRE = 254
R248_BEST = 3
# 8cf249. pe2pkg file30406@197: best==7 nc16 pre88 jf('L',) ('Mtip', 7, 88)→('Mtip', 11, 52)
R249_NC = 16
R249_PRE = 88
R249_BEST = 7
# 8cf250. pe2pkg file30406@289: best==3 nc8 pre166 jf('L',) ('Mtip', 3, 232)→('Mmid', 7, 4, 8)
R250_NC = 8
R250_PRE = 166
R250_BEST = 3
# 8cf251. pe2pkg file30406@305: best==3 nc64 pre90 jf('L',) ('Mmid', 11, 4, 156)→('Mtip', 3, 180)
R251_NC = 64
R251_PRE = 90
R251_BEST = 3
# 8cf252. pe2pkg file30406@308: best==2 nc2 pre0 jf('M', 3) ('Mtip', 4, 124)→('L', 128)
R252_NC = 2
R252_PRE = 0
R252_BEST = 2
# 8cf253. pe2pkg file30406@310: best==6 nc2 pre255 jf('M', 2) ('Mmid', 3, 1, 188)→('Mmid', 7, 1, 40)
R253_NC = 2
R253_PRE = 255
R253_BEST = 6
# 8cf254. pe2pkg file30406@328: best==4 nc3 pre0 jf('M', 4) ('Mtip', 4, 240)→('Mmid', 7, 3, 48)
R254_NC = 3
R254_PRE = 0
R254_BEST = 4
# 8cf255. pe2pkg file30406@348: best==2 nc2 pre0 jf('M', 4) ('L', 214)→('Mtip', 2, 188)
R255_NC = 2
R255_PRE = 0
R255_BEST = 2
# 8cf256. pe2pkg file30300@185: best==3 nc4 pre254 jf('L',) ('L', 41)→('Mtip', 4, 5)
R256_NC = 4
R256_PRE = 254
R256_BEST = 3
# 8cf257. pe2pkg file30300@209: best==3 nc55 pre1 jf('L',) ('Mmid', 4, 1, 96)→('Mmid', 4, 1, 12)
R257_NC = 55
R257_PRE = 1
R257_BEST = 3
# 8cf258. pe2pkg file30300@266: best==2 nc109 pre0 jf('M', 17) ('Mmid', 13, 11, 228)→('Mmid', 7, 5, 136)
R258_NC = 109
R258_PRE = 0
R258_BEST = 2
# 8cf259. pe2pkg file30300@270: best==2 nc109 pre0 jf('M', 2) ('Mmid', 4, 2, 72)→('Mmid', 3, 1, 172)
R259_NC = 109
R259_PRE = 0
R259_BEST = 2
# 8cf260. pe2pkg file30300@273: best==3 nc11 pre241 jf('L',) ('Mtip', 3, 220)→('Mmid', 4, 1, 164)
R260_NC = 11
R260_PRE = 241
R260_BEST = 3
# 8cf261. pe2pkg file30300@277: best==3 nc12 pre204 jf('L',) ('Mmid', 7, 4, 200)→('Mmid', 4, 3, 82)
R261_NC = 12
R261_PRE = 204
R261_BEST = 3
# 8cf262. pe2pkg file30300@281: best==3 nc67 pre35 jf('L',) ('Mmid', 2, 1, 12)→('Mmid', 4, 1, 168)
R262_NC = 67
R262_PRE = 35
R262_BEST = 3
# 8cf263. pe2pkg file30300@293: best==3 nc14 pre139 jf('L',) ('Mtip', 3, 240)→('Mtip', 3, 20)
R263_NC = 14
R263_PRE = 139
R263_BEST = 3
# 8cf264. pe2pkg file20600@140: best==11 nc2 pre97 jf('L',) ('Mtip', 15, 16)→('Mmid', 7, 2, 32)
R264_NC = 2
R264_PRE = 97
R264_BEST = 11
# 8cf265. pe2pkg file20600@180: best==7 nc4 pre0 jf('M', 4) ('Mmid', 7, 6, 68)→('Mmid', 11, 4, 36)
R265_NC = 4
R265_PRE = 0
R265_BEST = 7
# 8cf266. pe2pkg file20600@201: best==2 nc6 pre131 jf('M', 3) ('Mmid', 3, 1, 84)→('Mmid', 7, 5, 16)
R266_NC = 6
R266_PRE = 131
R266_BEST = 2
# 8cf267. pe2pkg file20600@208: best==5 nc9 pre104 jf('L',) ('Mtip', 5, 120)→('Mtip', 11, 68)
R267_NC = 9
R267_PRE = 104
R267_BEST = 5
# 8cf268. pe2pkg file20600@230: best==3 nc3 pre112 jf('M', 2) ('L', 156)→('Mmid', 12, 1, 136)
R268_NC = 3
R268_PRE = 112
R268_BEST = 3
# 8cf269. pe2pkg file20600@240: best==3 nc5 pre120 jf('L',) ('Mtip', 3, 164)→('Mmid', 2, 1, 6)
R269_NC = 5
R269_PRE = 120
R269_BEST = 3
# 8cf270. pe2pkg file20600@244: best==5 nc2 pre114 jf('L',) ('Mtip', 5, 52)→('Mtip', 4, 68)
R270_NC = 2
R270_PRE = 114
R270_BEST = 5
# 8cf271. pe2pkg file20600@256: best==3 nc2 pre104 jf('M', 3) ('Mmid', 3, 2, 24)→('Mmid', 5, 4, 212)
R271_NC = 2
R271_PRE = 104
R271_BEST = 3
# 8cf272. pe2pkg file50119@152: best==4 nc4 pre16 jf('M', 2) ('Mtip', 4, 16)→('Mtip', 4, 52)
R272_NC = 4
R272_PRE = 16
R272_BEST = 4
# 8cf273. pe2pkg file50119@169: best==3 nc14 pre0 jf('M', 2) ('Mmid', 4, 1, 32)→('Mmid', 4, 1, 48)
R273_NC = 14
R273_PRE = 0
R273_BEST = 3
# 8cf274. pe2pkg file50119@229: best==2 nc2 pre76 jf('L',) ('Mtip', 2, 52)→('L', 84)
R274_NC = 2
R274_PRE = 76
R274_BEST = 2
# 8cf275. pe2pkg file50119@236: best==2 nc3 pre38 jf('L',) ('Mtip', 2, 12)→('L', 132)
R275_NC = 3
R275_PRE = 38
R275_BEST = 2
# 8cf276. pe2pkg file50119@276: best==2 nc2 pre174 jf('M', 3) ('L', 236)→('Mtip', 2, 144)
R276_NC = 2
R276_PRE = 174
R276_BEST = 2
# 8cf277. pe2pkg file50119@306: best==2 nc25 pre72 jf('L',) ('Mmid', 2, 1, 34)→('Mmid', 8, 6, 108)
R277_NC = 25
R277_PRE = 72
R277_BEST = 2
# 8cf278. pe2pkg file50119@322: best==2 nc2 pre16 jf('M', 3) ('L', 178)→('Mtip', 2, 150)
R278_NC = 2
R278_PRE = 16
R278_BEST = 2
# 8cf279. pe2pkg file50119@356: best==2 nc24 pre0 jf('M', 4) ('Mmid', 4, 2, 202)→('Mmid', 5, 3, 194)
R279_NC = 24
R279_PRE = 0
R279_BEST = 2
# 8cf280. pe2pkg file30200@126: best==2 nc64 pre0 jf('M', 2) ('Mmid', 4, 2, 118)→('Mmid', 4, 2, 52)
R280_NC = 64
R280_PRE = 0
R280_BEST = 2
# 8cf281. pe2pkg file30200@185: best==3 nc73 pre12 jf('L',) ('Mmid', 17, 14, 23)→('Mmid', 8, 5, 124)
R281_NC = 73
R281_PRE = 12
R281_BEST = 3
# 8cf282. pe2pkg file30200@209: best==3 nc10 pre186 jf('L',) ('Mtip', 3, 144)→('Mtip', 7, 8)
R282_NC = 10
R282_PRE = 186
R282_BEST = 3
# 8cf283. pe2pkg file30200@229: best==3 nc75 pre55 jf('L',) ('Mmid', 5, 2, 138)→('Mmid', 4, 1, 8)
R283_NC = 75
R283_PRE = 55
R283_BEST = 3
# 8cf284. pe2pkg file30200@301: best==3 nc62 pre48 jf('L',) ('Mtip', 3, 252)→('Mtip', 3, 116)
R284_NC = 62
R284_PRE = 48
R284_BEST = 3
# 8cf285. pe2pkg file30200@304: best==3 nc7 pre0 jf('M', 3) ('Mtip', 3, 203)→('Mmid', 4, 1, 195)
R285_NC = 7
R285_PRE = 0
R285_BEST = 3
# 8cf286. pe2pkg file30200@346: best==2 nc2 pre0 jf('M', 2) ('Mmid', 4, 2, 8)→('L', 36)
R286_NC = 2
R286_PRE = 0
R286_BEST = 2
# 8cf287. pe2pkg file30200@352: best==4 nc2 pre0 jf('M', 4) ('L', 256)→('Mtip', 3, 12)
R287_NC = 2
R287_PRE = 0
R287_BEST = 4
# 8cf288. pe2pkg file50146@396: best==4 nc10 pre150 jf('M', 4) ('Mtip', 4, 12)→('Mmid', 8, 4, 208)
R288_NC = 10
R288_PRE = 150
R288_BEST = 4
# 8cf289. pe2pkg file50146@412: best==4 nc2 pre150 jf('M', 7) ('Mtip', 4, 136)→('Mtip', 2, 152)
R289_NC = 2
R289_PRE = 150
R289_BEST = 4
# 8cf290. pe2pkg file50146@436: best==4 nc3 pre39 jf('L',) ('Mtip', 4, 24)→('Mtip', 4, 160)
R290_NC = 3
R290_PRE = 39
R290_BEST = 4
# 8cf291. pe2pkg file50146@483: best==2 nc5 pre165 jf('L',) ('Mmid', 7, 3, 75)→('Mtip', 3, 143)
R291_NC = 5
R291_PRE = 165
R291_BEST = 2
# 8cf292. pe2pkg file50146@588: best==2 nc6 pre166 jf('L',) ('Mmid', 4, 2, 54)→('Mmid', 4, 2, 74)
R292_NC = 6
R292_PRE = 166
R292_BEST = 2
# 8cf293. pe2pkg file50146@598: best==2 nc2 pre6 jf('M', 2) ('Mmid', 2, 1, 16)→('Mtip', 2, 8)
R293_NC = 2
R293_PRE = 6
R293_BEST = 2
# 8cf294. pe2pkg file50146@605: best==2 nc14 pre42 jf('L',) ('Mmid', 8, 6, 31)→('Mmid', 5, 3, 235)
R294_NC = 14
R294_PRE = 42
R294_BEST = 2
# 8cf295. pe2pkg file50146@617: best==2 nc4 pre8 jf('L',) ('Mtip', 3, 24)→('Mmid', 8, 1, 48)
R295_NC = 4
R295_PRE = 8
R295_BEST = 2
# 8cf296. pe2pkg file50146@628: best==3 nc4 pre162 jf('L',) ('Mtip', 3, 44)→('Mtip', 4, 236)
R296_NC = 4
R296_PRE = 162
R296_BEST = 3
# 8cf297. pe2pkg file50146@632: best==4 nc4 pre134 jf('L',) ('Mmid', 5, 1, 20)→('Mmid', 8, 4, 60)
R297_NC = 4
R297_PRE = 134
R297_BEST = 4
# 8cf298. pe2pkg file30406@350: best==2 nc3 pre0 jf('M', 2) ('Mmid', 6, 2, 38)→('L', 226)
R298_NC = 3
R298_PRE = 0
R298_BEST = 2
# 8cf299. pe2pkg file30406@352: best==4 nc2 pre0 jf('M', 2) ('Mtip', 4, 176)→('Mtip', 3, 188)
R299_NC = 2
R299_PRE = 0
R299_BEST = 4
# 8cf300. pe2pkg file30406@356: best==4 nc2 pre40 jf('M', 4) ('Mmid', 6, 2, 92)→('Mtip', 4, 200)
R300_NC = 2
R300_PRE = 40
R300_BEST = 4
# 8cf301. pe2pkg file30406@469: best==3 nc79 pre227 jf('L',) ('Mmid', 10, 5, 132)→('Mmid', 11, 8, 64)
R301_NC = 79
R301_PRE = 227
R301_BEST = 3
# 8cf302. pe2pkg file30406@473: best==3 nc80 pre70 jf('L',) ('Mmid', 7, 4, 200)→('Mmid', 6, 3, 8)
R302_NC = 80
R302_PRE = 70
R302_BEST = 3
# 8cf303. pe2pkg file30406@489: best==3 nc7 pre206 jf('L',) ('Mmid', 4, 1, 160)→('Mmid', 4, 1, 192)
R303_NC = 7
R303_PRE = 206
R303_BEST = 3
# 8cf304. pe2pkg file30406@546: best==2 nc2 pre0 jf('M', 3) ('Mtip', 17, 184)→('Mtip', 4, 202)
R304_NC = 2
R304_PRE = 0
R304_BEST = 2
# 8cf305. pe2pkg file30406@552: best==3 nc9 pre0 jf('M', 4) ('Mmid', 17, 10, 163)→('Mtip', 4, 200)
R305_NC = 9
R305_PRE = 0
R305_BEST = 3
# 8cf306. pe2pkg file30406@582: best==2 nc103 pre0 jf('M', 4) ('Mmid', 4, 2, 68)→('Mmid', 4, 2, 236)
R306_NC = 103
R306_PRE = 0
R306_BEST = 2
# 8cf307. pe2pkg file30406@645: best==3 nc5 pre220 jf('L',) ('Mtip', 3, 232)→('Mtip', 7, 224)
R307_NC = 5
R307_PRE = 220
R307_BEST = 3
# 8cf308. pe2pkg file30300@410: best==2 nc102 pre0 jf('M', 17) ('Mtip', 7, 209)→('Mtip', 2, 144)
R308_NC = 102
R308_PRE = 0
R308_BEST = 2
# 8cf309. pe2pkg file30300@413: best==3 nc78 pre12 jf('L',) ('Mtip', 3, 252)→('Mmid', 2, 1, 144)
R309_NC = 78
R309_PRE = 12
R309_BEST = 3
# 8cf310. pe2pkg file30300@417: best==3 nc8 pre231 jf('L',) ('Mmid', 17, 15, 78)→('Mtip', 3, 144)
R310_NC = 8
R310_PRE = 231
R310_BEST = 3
# 8cf311. pe2pkg file30300@428: best==4 nc2 pre0 jf('M', 7) ('Mtip', 8, 144)→('L', 152)
R311_NC = 2
R311_PRE = 0
R311_BEST = 4
# 8cf312. pe2pkg file30300@477: best==3 nc73 pre27 jf('L',) ('Mmid', 10, 3, 128)→('Mmid', 4, 1, 4)
R312_NC = 73
R312_PRE = 27
R312_BEST = 3
# 8cf313. pe2pkg file30300@485: best==3 nc75 pre47 jf('L',) ('Mmid', 17, 15, 77)→('Mtip', 7, 28)
R313_NC = 75
R313_PRE = 47
R313_BEST = 3
# 8cf314. pe2pkg file30300@529: best==3 nc60 pre48 jf('L',) ('Mtip', 3, 248)→('Mtip', 3, 116)
R314_NC = 60
R314_PRE = 48
R314_BEST = 3
# 8cf315. pe2pkg file30300@532: best==3 nc7 pre0 jf('M', 3) ('Mmid', 17, 13, 195)→('Mtip', 17, 208)
R315_NC = 7
R315_PRE = 0
R315_BEST = 3
# 8cf316. pe2pkg file30300@580: best==4 nc2 pre0 jf('M', 4) ('Mtip', 17, 256)→('Mtip', 3, 12)
R316_NC = 2
R316_PRE = 0
R316_BEST = 4
# 8cf317. pe2pkg file30300@585: best==3 nc59 pre8 jf('L',) ('Mmid', 9, 4, 32)→('Mmid', 11, 8, 88)
R317_NC = 59
R317_PRE = 8
R317_BEST = 3
# 8cf318. pe2pkg file30200@357: best==3 nc59 pre8 jf('L',) ('Mtip', 3, 172)→('Mtip', 11, 108)
R318_NC = 59
R318_PRE = 8
R318_BEST = 3
# 8cf319. pe2pkg file20600@259: best==2 nc4 pre131 jf('M', 3) ('Mtip', 3, 6)→('Mmid', 3, 1, 54)
R319_NC = 4
R319_PRE = 131
R319_BEST = 2
# 8cf320. pe2pkg file20600@289: best==2 nc2 pre131 jf('M', 7) ('Mtip', 12, 196)→('L', 216)
R320_NC = 2
R320_PRE = 131
R320_BEST = 2
# 8cf321. pe2pkg file20600@314: best==3 nc6 pre104 jf('L',) ('Mmid', 3, 2, 56)→('L', 126)
R321_NC = 6
R321_PRE = 104
R321_BEST = 3
# 8cf322. pe2pkg file20600@388: best==2 nc3 pre0 jf('M', 8) ('Mmid', 5, 4, 46)→('Mmid', 3, 2, 86)
R322_NC = 3
R322_PRE = 0
R322_BEST = 2
# 8cf323. pe2pkg file20600@390: best==3 nc5 pre86 jf('M', 2) ('Mtip', 8, 10)→('Mtip', 3, 176)
R323_NC = 5
R323_PRE = 86
R323_BEST = 3
# 8cf324. pe2pkg file20600@402: best==5 nc2 pre91 jf('M', 3) ('Mmid', 7, 4, 116)→('Mmid', 9, 4, 28)
R324_NC = 2
R324_PRE = 91
R324_BEST = 5
# 8cf325. pe2pkg file20600@438: best==3 nc13 pre0 jf('M', 8) ('Mtip', 9, 20)→('Mmid', 5, 2, 88)
R325_NC = 13
R325_PRE = 0
R325_BEST = 3
# 8cf326. pe2pkg file20600@442: best==3 nc9 pre110 jf('L',) ('Mtip', 8, 12)→('Mmid', 5, 1, 28)
R326_NC = 9
R326_PRE = 110
R326_BEST = 3
# 8cf327. pe2pkg file20600@454: best==12 nc2 pre103 jf('M', 7) ('Mmid', 8, 6, 228)→('Mmid', 5, 2, 104)
R327_NC = 2
R327_PRE = 103
R327_BEST = 12
# 8cf328. pe2pkg file20600@486: best==3 nc13 pre104 jf('M', 4) ('Mtip', 11, 16)→('Mmid', 5, 2, 136)
R328_NC = 13
R328_PRE = 104
R328_BEST = 3
# 8cf329. pe2pkg file50119@360: best==4 nc6 pre132 jf('L',) ('Mmid', 8, 4, 164)→('Mmid', 5, 1, 200)
R329_NC = 6
R329_PRE = 132
R329_BEST = 4
# 8cf330. pe2pkg file50119@407: best==2 nc2 pre2 jf('M', 2) ('Mmid', 3, 2, 56)→('Mmid', 3, 2, 92)
R330_NC = 2
R330_PRE = 2
R330_BEST = 2
# 8cf331. pe2pkg file50119@418: best==2 nc3 pre20 jf('L',) ('Mtip', 2, 182)→('Mtip', 2, 194)
R331_NC = 3
R331_PRE = 20
R331_BEST = 2
# 8cf332. pe2pkg file50119@427: best==3 nc5 pre98 jf('L',) ('Mmid', 4, 1, 66)→('Mmid', 8, 5, 230)
R332_NC = 5
R332_PRE = 98
R332_BEST = 3
# 8cf333. pe2pkg file50119@452: best==2 nc4 pre132 jf('M', 3) ('Mmid', 6, 2, 10)→('Mtip', 2, 130)
R333_NC = 4
R333_PRE = 132
R333_BEST = 2
# 8cf334. pe2pkg file50119@457: best==2 nc3 pre42 jf('L',) ('Mmid', 6, 1, 16)→('Mmid', 3, 2, 136)
R334_NC = 3
R334_PRE = 42
R334_BEST = 2
# 8cf335. pe2pkg file50119@480: best==2 nc2 pre144 jf('L',) ('Mtip', 2, 68)→('Mmid', 2, 1, 72)
R335_NC = 2
R335_PRE = 144
R335_BEST = 2
# 8cf336. pe2pkg file50119@485: best==2 nc2 pre8 jf('L',) ('Mtip', 3, 36)→('Mmid', 2, 1, 128)
R336_NC = 2
R336_PRE = 8
R336_BEST = 2
# 8cf337. pe2pkg file50119@502: best==2 nc6 pre134 jf('L',) ('Mtip', 2, 180)→('Mtip', 2, 50)
R337_NC = 6
R337_PRE = 134
R337_BEST = 2
# 8cf338. pe2pkg file50119@524: best==2 nc3 pre36 jf('M', 2) ('Mtip', 4, 92)→('Mtip', 2, 184)
R338_NC = 3
R338_PRE = 36
R338_BEST = 2
# 8cf339. pe2pkg file30406/07@693: best==3 nc3 pre44
#         ('Mtip', 3, 204) → ('Mmid', 15, 10, 78)
R339_NC = 3
R339_PRE = 44
R339_BEST = 3
# 8cf340. pe2pkg file50146@644: best==2 nc4 pre20 jf('M', 3)
#         ('Mmid', 10, 6, 148) → ('Mtip', 5, 184)
R340_NC = 4
R340_PRE = 20
R340_BEST = 2
# 8cf341. pe2pkg file30406/07@697: best==3 nc82 pre20 jf('L',)
#         ('Mmid', 6, 3, 8) → ('Mmid', 4, 1, 148)
R341_NC = 82
R341_PRE = 20
R341_BEST = 3
# 8cf342. pe2pkg file20600@493: best==3 nc2 pre131 jf('M', 3)
#         ('L', 222) → ('Mmid', 7, 2, 44)
R342_NC = 2
R342_PRE = 131
R342_BEST = 3
# 8cf343. pe2pkg file50119/20/21@530: best==2 nc7 pre22 jf('L',)
#         ('Mtip', 2, 28) → ('Mmid', 4, 3, 31)
R343_NC = 7
R343_PRE = 22
R343_BEST = 2
# 8cf344. pe2pkg file50146@646: best==3 nc2 pre32 jf('M', 2)
#         ('Mmid', 2, 1, 57) → ('Mtip', 2, 65)
R344_NC = 2
R344_PRE = 32
R344_BEST = 3
# 8cf345. pe2pkg file30406/07@700: best==2 nc2 pre0 jf('M', 3)
#         ('Mmid', 15, 3, 92) → ('Mmid', 17, 16, 96)
R345_NC = 2
R345_PRE = 0
R345_BEST = 2
# 8cf346. pe2pkg file20600@526: best==17 nc2 pre81 jf('L',)
#         ('Mtip', 3, 40) → ('Mtip', 17, 20)
R346_NC = 2
R346_PRE = 81
R346_BEST = 17
# 8cf347. pe2pkg file50119/20/21@550: best==2 nc8 pre54 jf('L',)
#         ('Mtip', 2, 20) → ('Mtip', 2, 48)
R347_NC = 8
R347_PRE = 54
R347_BEST = 2
# 8cf348. pe2pkg file30406/07@702: best==2 nc115 pre255 jf('M', 2)
#         ('Mmid', 3, 1, 8) → ('Mmid', 6, 4, 12)
R348_NC = 115
R348_PRE = 255
R348_BEST = 2
# 8cf349. pe2pkg file30406/07@709: best==3 nc75 pre17 jf('L',)
#         ('Mtip', 3, 12) → ('Mmid', 6, 3, 20)
R349_NC = 75
R349_PRE = 17
R349_BEST = 3
# 8cf350. pe2pkg file50146@649: best==2 nc8 pre36 jf('M', 3)
#         ('Mtip', 3, 3) → ('Mmid', 3, 1, 64)
R350_NC = 8
R350_PRE = 36
R350_BEST = 2
# 8cf351. pe2pkg file20600@546: best==17 nc3 pre82 jf('L',)
#         ('Mtip', 17, 40) → ('Mtip', 17, 20)
R351_NC = 3
R351_PRE = 82
R351_BEST = 17
# 8cf352. pe2pkg file50119/20/21@556: best==4 nc2 pre12 jf('L',)
#         ('Mtip', 4, 60) → ('Mtip', 2, 76)
R352_NC = 2
R352_PRE = 12
R352_BEST = 4
# 8cf353. pe2pkg file30406/07@728: best==4 nc4 pre0 jf('M', 8)
#         ('Mtip', 4, 216) → ('Mtip', 4, 200)
R353_NC = 4
R353_PRE = 0
R353_BEST = 4
# 8cf354. pe2pkg file20600@563: best==2 nc3 pre130 jf('M', 17)
#         ('Mtip', 2, 40) → ('Mtip', 2, 20)
R354_NC = 3
R354_PRE = 130
R354_BEST = 2
# 8cf355. pe2pkg file50119/20/21@576: best==6 nc2 pre134 jf('L',)
#         ('Mtip', 6, 136) → ('L', 228)
R355_NC = 2
R355_PRE = 134
R355_BEST = 6
# 8cf356. pe2pkg file30406/07@736: best==4 nc3 pre0 jf('M', 4)
#         ('Mtip', 8, 24) → ('Mtip', 4, 32)
R356_NC = 3
R356_PRE = 0
R356_BEST = 4
# 8cf357. pe2pkg file50146@664: best==4 nc4 pre150 jf('L',)
#         ('Mmid', 5, 1, 12) → ('Mmid', 8, 4, 92)
R357_NC = 4
R357_PRE = 150
R357_BEST = 4
# 8cf358. pe2pkg file20600@566: best==17 nc4 pre83 jf('L',)
#         ('Mtip', 17, 40) → ('Mtip', 17, 20)
R358_NC = 4
R358_PRE = 83
R358_BEST = 17
# 8cf359. pe2pkg file50119/20/21@604: best==2 nc2 pre0 jf('M', 4)
#         ('Mtip', 2, 94) → ('Mtip', 2, 202)
R359_NC = 2
R359_PRE = 0
R359_BEST = 2
# 8cf360. pe2pkg file30406/07@837: best==3 nc5 pre228 jf('L',)
#         ('Mtip', 3, 4) → ('Mtip', 3, 200)
R360_NC = 5
R360_PRE = 228
R360_BEST = 3
# 8cf361. pe2pkg file30406/07@842: best==2 nc7 pre242 jf('L',)
#         ('Mmid', 3, 1, 8) → ('Mmid', 3, 1, 200)
R361_NC = 7
R361_PRE = 242
R361_BEST = 2
# 8cf362. pe2pkg file50146@680: best==4 nc3 pre150 jf('M', 4)
#         ('Mmid', 10, 6, 184) → ('Mtip', 5, 220)
R362_NC = 3
R362_PRE = 150
R362_BEST = 4
# 8cf363. pe2pkg file30300/01@604: best==4 nc3 pre0 jf('M', 2)
#         cands T4@12, T4@24, T3@36 — pick T4@24 → retail T3@36.
# Encode-validated: SAFE (pkg/clut exacts held; 30300/01 prefix 71.3→73.4%).
R363_NC = 3
R363_PRE = 0
R363_BEST = 4
R363_AGES = (12, 24, 36)
R363_PLS = (4, 4, 3)
# 8cf364. pe2pkg file50146@688: best==2 nc2 pre0 jf('M', 4)
#         mid into+1 of M3 age==160 → M2 tip age==180.
# Encode-validated: SAFE (50146 prefix 82.6→83.1%; broader tip→lit style
# without age gates regressed pe2clut −7).
R364_NC = 2
R364_PRE = 0
R364_BEST = 2
R364_SRC_INTO = 1
R364_SRC_OWNER_PL = 3
R364_SRC_AGE = 160
R364_DST_PL = 2
R364_DST_AGE = 180
# 8cf365. pe2pkg file30300/01@620 + file30200/01@392: best==4 nc31 pre255
#         jf('M', 4) M17 mid into+13 age==231 → M17 mid into+5 age==239.
# Encode-validated: SAFE (30300 73.4→74.0%; 30200 46.8→47.6%).
R365_NC = 31
R365_PRE = 255
R365_BEST = 4
R365_SRC_OWNER_PL = 17
R365_SRC_INTO = 13
R365_SRC_AGE = 231
R365_DST_OWNER_PL = 17
R365_DST_INTO = 5
R365_DST_AGE = 239
# 8cf366. pe2pkg file50119/20/21@632: best==2 nc3 pre150, M2 tips ages
#         28/122/230 — pick@28 → tip@122.
# Encode-validated: SAFE (50119 24.1→24.4%).
R366_NC = 3
R366_PRE = 150
R366_BEST = 2
R366_AGES = (28, 122, 230)
R366_SRC_AGE = 28
R366_DST_AGE = 122
R366_PL = 2
# 8cf367. pe2pkg file20600@598: best==3 nc13 pre109 jf('L',)
#         M17 mid into+4 age==28 → M3 tip age==156.
# Encode-validated: SAFE (20600 37.5→39.6%).
R367_NC = 13
R367_PRE = 109
R367_BEST = 3
R367_SRC_OWNER_PL = 17
R367_SRC_INTO = 4
R367_SRC_AGE = 28
R367_DST_PL = 3
R367_DST_AGE = 156
# 8cf368. pe2pkg file30300/01@625 + file30200/01@397: best==7 nc16 pre136
#         jf('L',) M17 mid into+10 age==222 → M9 tip age==112.
# Encode-validated: SAFE (30300 74.0→79.1%; 30200 47.6→48.4%).
R368_NC = 16
R368_PRE = 136
R368_BEST = 7
R368_SRC_OWNER_PL = 17
R368_SRC_INTO = 10
R368_SRC_AGE = 222
R368_DST_PL = 9
R368_DST_AGE = 112
# 8cf369. pe2pkg file20600@616: best==5 nc5 pre117 jf('L',)
#         M17 mid into+2 age==68 → M17 mid into+2 age==48.
# Encode-validated: SAFE (20600 39.6→40.2%).
R369_NC = 5
R369_PRE = 117
R369_BEST = 5
R369_SRC_OWNER_PL = 17
R369_SRC_INTO = 2
R369_SRC_AGE = 68
R369_DST_OWNER_PL = 17
R369_DST_INTO = 2
R369_DST_AGE = 48
# 8cf370. pe2pkg file50119/20/21@648: best==2 nc2 pre0 jf('M', 8)
#         M2 tip age==230 → M2 tip age==28.
# Encode-validated: SAFE (50119 24.4→25.4%).
R370_NC = 2
R370_PRE = 0
R370_BEST = 2
R370_AGES = (28, 230)
R370_SRC_AGE = 230
R370_DST_AGE = 28
R370_PL = 2
# Sole M4 tip age == 244, best_len == 4, pre == 134, just-finished L → force lit
# (pe2pkg file50146@696). Path-clean encode first-miss.
SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_AGE = 244
SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_BEST = 4
SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_PRE = 134
SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_PL = 4
# Sole M4 mid into +1 age == 244, best_len == 3, pre == 174, just-finished L
# → force lit (pe2pkg file50146@697).
SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_AGE = 244
SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_BEST = 3
SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_PRE = 174
SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_OWNER_PL = 4
SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_INTO = 1
# Sole M2 tip age == 244, best_len == 4, pre-byte == 0, just-finished M4
# → force lit (pe2pkg file30300/01@512). Path-clean on stage0 ≤4K.
SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_AGE = 244
SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_BEST = 4
SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_PRE = 0
SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_PL = 2
# Sole M10 tip age == 244, best_len == 10, pre-byte == 32, just-finished M7
# → force lit (pe2pkg file30300/01@548). Path-clean on stage0 ≤4K.
SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_AGE = 244
SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_BEST = 10
SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_PRE = 32
SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_PL = 10
# Sole M5 mid into +3 age == 252, best_len == 4, pre == 40, just-finished M4
# → force lit (pe2pkg file30300/01@596). Path-clean on stage0 ≤4K.
SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_AGE = 252
SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_BEST = 4
SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_PRE = 40
SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_OWNER_PL = 5
SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_INTO = 3
# best==2: young M3 mid (age < 8) → M2 tip age in [40, 48)
# (file444000@596 / file403200: tip@554 over mid 594 of M3@593).
B2_M3MID_TO_OLD_M2TIP_MID_MAX_AGE = 8
B2_M3MID_TO_OLD_M2TIP_TIP_MIN_AGE = 40
B2_M3MID_TO_OLD_M2TIP_TIP_MAX_AGE = 48
# best ≥ 14: old lit tip (age ≥ 32) → newest mid into +2 of a same-len long
# phrase (pl == best, age ≤ 64). M17 ladder re-anchor after RLE (file29@2626).
LIT_TO_SAMELEN_INTO2_MIN_BEST = 14
LIT_TO_SAMELEN_INTO2_LIT_MIN_AGE = 32
LIT_TO_SAMELEN_INTO2_MID_MAX_AGE = 64
LIT_TO_SAMELEN_INTO2 = 2
# best ≥ 3: deep/old long mid (into ≥ 6, age ≥ 96) → newest long mid into +2
# (file29@2745: mid into2 of young long over deep mid into8 of old long).
DEEP_LONG_MID_TO_INTO2_MIN_BEST = 3
DEEP_LONG_MID_TO_INTO2_MIN_INTO = 6
DEEP_LONG_MID_TO_INTO2_MIN_AGE = 96
DEEP_LONG_MID_TO_INTO2_TARGET_MAX_AGE = 64
# best ≥ 14: just-finished medium pl ∈ [8, 13] → its PE
# (file29@2754: pe of M9@2745; file20328@488: pe of M8@480 over into6).
JUST_FIN_MED_PE_MIN_BEST = 14
JUST_FIN_MED_PE_MIN_PL = 8
JUST_FIN_MED_PE_MAX_PL = 13
# best ≥ 14: same-len long MS tip age == 30 → newest mid of that phrase
# (file30101@1100: into1 1071 of M17@1070; @1130: into3 1103 of M17@1100).
# Age gate alone is exact on stage0 CLUT retail path (into1/into3 family).
LONG_TIP_TO_NEWEST_MID_MIN_BEST = 14
LONG_TIP_TO_NEWEST_MID_TIP_AGE = 30
# best in {7, 11}: just-finished long → newest long mid among multi-max
# (file30101@1147 best7: 1115 over 1083; @1239 best11: 1207 age32 over 1175).
JF_LONG_TO_NEWEST_LONG_MID_BESTS = (7, 11)
# best ≥ 14: just-finished M2 → its PE (file30101@1191: pe 1190 of M2@1189
# over mid 1135 of M17@1130). Complements 8bh (just-finished *long* PE).
JF_M2_PE_MIN_BEST = 14
JF_M2_PE_PL = 2
# best == 10: just-finished long → long mid age == 128 with into == best
# (file30101@1208: mid 1080 of M17@1070 over AP-32 2nd-newest 1144).
JF_LONG_INTO_BEST_MID_BEST = 10
JF_LONG_INTO_BEST_MID_AGE = 128
# best==2: young M2 tip age < 16 → PE of M3 age ≥ 8
# (file20208@391: pe 377 of M3@375 over tip@383).
B2_TIP_TO_M3PE_TIP_MAX_AGE = 16
B2_TIP_TO_M3PE_PE_MIN_AGE = 8
# best==2: young M2 tip age < 16 → PE of M2 age in [8, 20)
# (file20415@323: pe 311 of M2@310 over tip@313).
B2_TIP_TO_M2PE_PE_MIN_AGE = 8
B2_TIP_TO_M2PE_PE_MAX_AGE = 20
# best==2: young M2 tip age < 8 → newest older same tip age in [8, 16)
# (file20208@415: same@405 over young tip@411).
B2_YOUNG_TO_OLDER_SAME_TIP_MAX = 8
B2_YOUNG_TO_OLDER_SAME_OLD_MIN = 8
B2_YOUNG_TO_OLDER_SAME_OLD_MAX = 16


def _is_arithmetic_period(cands: list[int], *, min_period: int = 2) -> bool:
    """True if sorted unique cands form an AP with common difference ≥ min_period."""
    s = sorted(set(cands))
    if len(s) < 3:
        return False
    d = s[1] - s[0]
    if d < min_period:
        return False
    return all(s[i] - s[i - 1] == d for i in range(2, len(s)))


def _owner_phrase_end(owner: int, token_starts: dict[int, tuple]) -> int | None:
    """Last absolute index of the phrase that starts at ``owner``, if known."""
    info = token_starts.get(owner)
    if info is None:
        return None
    if info[0] == "L":
        return owner
    if info[0] == "M":
        return owner + info[1] - 1
    return None


def _pick_match_ref(
    cands: list[int],
    *,
    best_len: int,
    token_starts: dict[int, tuple],
    owners: dict[int, int],
    pos: int = 0,
    data: bytes | None = None,
) -> int:
    """Choose among equal-length match sources (retail-oriented heuristics).

    Order (see module docstring). ``owners`` maps each written absolute index to
    the start index of the token that emitted it (literal or match).
    ``pos`` is the absolute output index of the match being chosen (age checks).
    ``data`` is the uncompressed buffer (optional; used for byte-value gates).
    """
    if len(cands) == 1:
        return cands[0]

    chosen: int | None = None

    # 1. Long prior match-starts (0x10-ladder ``M(11,3)`` etc.).
    #    Skip when newest cand still lies inside that long phrase — that is
    #    ongoing RLE/continuation (prefer newest), not a ladder re-anchor.
    #    Prefer a *newer same-length* match-start over a distant long ladder
    #    (file30400 ``M(42,3)`` vs ``M(11,3)`` after an earlier ladder use).
    #    If that same-len tip is very young (age < 2×best), prefer a *medium*
    #    MS instead (best < prev_len ≤ 2×best+1, age ≤ 24) so we do not stick
    #    to the just-emitted phrase (file30400@85: M7@65 over M3@81).
    #    If the long anchor is old, prefer covers_pe (2nd-newest when several);
    #    if that pick is a mid and a tight same-len MS pair exists, take the
    #    2nd-newest of that pair (file30400@105: 81 over mid 89).
    if best_len >= 3:
        thr = max(2 * best_len, LONG_MS_ABS_FLOOR)
        long_ms: list[int] = []
        any_ms: list[int] = []
        for ref in cands:
            info = token_starts.get(ref)
            if info is not None and info[0] == "M":
                if info[1] >= thr:
                    long_ms.append(ref)
                if info[1] >= best_len:
                    any_ms.append(ref)

        def _old_mid_covers() -> list[int]:
            """Covers of medium owners (best < pl < thr) with age ≥ floor."""
            out: list[int] = []
            for ref in cands:
                o = owners.get(ref, ref)
                info = token_starts.get(o)
                if (
                    info is not None
                    and info[0] == "M"
                    and best_len < info[1] < thr
                    and ref + best_len - 1 == o + info[1] - 1
                    and (pos - ref) >= MID_COVER_MIN_AGE
                ):
                    out.append(ref)
            return out

        if long_ms:
            lm_ref = max(long_ms, key=lambda c: (token_starts[c][1], c))
            lm_info = token_starts.get(lm_ref)
            newest = max(cands)
            in_phrase = (
                lm_info is not None
                and lm_info[0] == "M"
                and lm_ref <= newest <= lm_ref + lm_info[1] - 1
            )
            if not in_phrase:
                if any_ms:
                    newest_ms = max(any_ms)
                    if (
                        newest_ms != lm_ref
                        and token_starts[newest_ms][1] == best_len
                    ):
                        # Distant long ladder + non-young same-len MS: prefer an
                        # old medium-owner cover (file30400@213: 73 over 153).
                        if (
                            (pos - lm_ref) >= OLD_COVER_MIN_AGE
                            and (pos - newest_ms) >= SAME_MS_MIN_AGE_FOR_MID
                        ):
                            mids = _old_mid_covers()
                            if mids:
                                chosen = max(mids)
                        if chosen is None and (pos - newest_ms) < 2 * best_len:
                            med_cap = 2 * best_len + 1
                            med = [
                                c
                                for c in any_ms
                                if best_len
                                < token_starts[c][1]
                                <= med_cap
                                and token_starts[c][1] < thr
                                and (pos - c) <= MEDIUM_MS_MAX_AGE
                            ]
                            if med:
                                chosen = max(
                                    med, key=lambda c: (token_starts[c][1], c)
                                )
                        if chosen is None:
                            chosen = newest_ms
                if chosen is None and (pos - lm_ref) > LONG_MS_COVERS_AGE:
                    cov_old: list[int] = []
                    for ref in cands:
                        o = owners.get(ref, ref)
                        info = token_starts.get(o)
                        if (
                            info is not None
                            and info[0] == "M"
                            and ref + best_len - 1 == o + info[1] - 1
                        ):
                            cov_old.append(ref)
                    if cov_old:
                        sc = sorted(cov_old, reverse=True)
                        pick_c = sc[1] if len(sc) >= 2 else sc[0]
                        # Mid cover + tight same-len MS pair → 2nd of pair.
                        pick_is_same = (
                            token_starts.get(pick_c) is not None
                            and token_starts[pick_c][0] == "M"
                            and token_starts[pick_c][1] == best_len
                        )
                        if not pick_is_same:
                            same_cov = [
                                c
                                for c in cov_old
                                if token_starts.get(c) is not None
                                and token_starts[c][0] == "M"
                                and token_starts[c][1] == best_len
                            ]
                            if len(same_cov) >= 2:
                                ss = sorted(same_cov, reverse=True)
                                if (
                                    (pos - ss[0]) <= MEDIUM_MS_MAX_AGE
                                    and (ss[0] - ss[1]) <= 8
                                ):
                                    pick_c = ss[1]
                        # Very young cover tip → re-anchor to newest *old* cover.
                        if (pos - pick_c) < YOUNG_COVER_MAX_AGE:
                            old_cov = [
                                c
                                for c in cov_old
                                if (pos - c) >= OLD_COVER_MIN_AGE
                            ]
                            if old_cov:
                                pick_c = max(old_cov)
                        chosen = pick_c
                if chosen is None:
                    chosen = lm_ref

    # 2. Phrase-end of an earlier match whose length equals this best_len.
    #    Recovers e.g. ref 9 = end of M(4) when best==4.
    if chosen is None and best_len >= 3:
        pe_eq: list[int] = []
        for ref in cands:
            o = owners.get(ref, ref)
            info = token_starts.get(o)
            if (
                info is not None
                and info[0] == "M"
                and info[1] == best_len
                and ref == o + best_len - 1
            ):
                pe_eq.append(ref)
        if pe_eq:
            chosen = max(pe_eq)

    # 3. Match that lands exactly on an owner phrase end (covers_pe).
    if chosen is None and best_len >= 6:
        cov: list[int] = []
        for ref in cands:
            o = owners.get(ref, ref)
            end = _owner_phrase_end(o, token_starts)
            if end is not None and ref + best_len - 1 == end:
                # Only match-owned phrases (not single-byte lits).
                info = token_starts.get(o)
                if info is not None and info[0] == "M":
                    cov.append(ref)
        if cov:
            chosen = max(cov)

    # 3b. Full-phrase match-start (pl == best) with other cands still inside
    #     that phrase → newest inside (file30402@292: 156 over MS 148).
    if (
        chosen is not None
        and best_len >= FULL_PHRASE_MIN_BEST
    ):
        info = token_starts.get(chosen)
        if info is not None and info[0] == "M" and info[1] == best_len:
            end = chosen + best_len - 1
            inside = [c for c in cands if chosen <= c <= end]
            if len(inside) >= 2:
                chosen = max(inside)

    # 4. Prefer a longer-than-best match-start over a literal tip when the
    #    overall newest cand is one of those (file30400@129: M4@124 over L@112).
    #    Gated so we do not steal a newer mid (CLUT file107600@1020).
    #    Same-len MS stay behind the med/lit gate so file30102@137 keeps
    #    L@129 over young M3@133; but an *old* same-len MS (age ≥ 96) beats
    #    the literal (file30402@273: M3@133 over L@129).
    if chosen is None and best_len >= 3:
        lit = [
            ref
            for ref in cands
            if token_starts.get(ref) is not None and token_starts[ref][0] == "L"
        ]
        med_ms = [
            ref
            for ref in cands
            if token_starts.get(ref) is not None
            and token_starts[ref][0] == "M"
            and token_starts[ref][1] > best_len
        ]
        same_ms = [
            ref
            for ref in cands
            if token_starts.get(ref) is not None
            and token_starts[ref][0] == "M"
            and token_starts[ref][1] == best_len
        ]
        newest = max(cands)
        # Just-emitted same-len tip:
        # - with old same-len + old mid cover → mid cover (file30402@277)
        # - with mid cover but no old same → 2nd-newest same if any, else the
        #   tip itself (file30402@421/@425: same chain over reusing mid cover)
        newest_young_same = (
            newest in same_ms and (pos - newest) < YOUNG_SAME_TIP_AGE
        )
        if newest_young_same:
            thr = max(2 * best_len, LONG_MS_ABS_FLOOR)
            mids: list[int] = []
            for ref in cands:
                o = owners.get(ref, ref)
                info = token_starts.get(o)
                if (
                    info is not None
                    and info[0] == "M"
                    and best_len < info[1] < thr
                    and ref + best_len - 1 == o + info[1] - 1
                    and (pos - ref) >= MID_COVER_MIN_AGE
                ):
                    mids.append(ref)
            old_same_tips = [
                c for c in same_ms if (pos - c) >= OLD_COVER_MIN_AGE
            ]
            if mids and old_same_tips:
                chosen = max(mids)
            elif mids and not old_same_tips:
                sc = sorted(same_ms, reverse=True)
                chosen = sc[1] if len(sc) >= 2 else sc[0]
        if chosen is None and med_ms and lit and (
            newest in lit or newest in med_ms
        ):
            # Very old med loses to lit (file30400@264: L@48 over M8@56).
            old_med = [c for c in med_ms if (pos - c) >= OLD_MED_MIN_AGE]
            if old_med and max(med_ms) in old_med and max(med_ms) > max(lit):
                chosen = max(lit)
            else:
                chosen = max(med_ms) if max(med_ms) > max(lit) else max(lit)
        elif chosen is None and lit:
            # Mid-age same-len (age in [12, 95]) newer than lit, no med_ms:
            # file30400@240 M4@220 over L@216; @360 age 12. Not age<12
            # (file30102@137 / file30400@244 young same → lit).
            mid_same = [
                c
                for c in same_ms
                if MID_SAME_MIN_AGE <= (pos - c) < OLD_COVER_MIN_AGE
                and c > max(lit)
            ]
            old_same = [c for c in same_ms if (pos - c) >= OLD_COVER_MIN_AGE]
            young_same = [
                c for c in same_ms if (pos - c) < YOUNG_SAME_BLOCKS_OLD
            ]
            if mid_same and not med_ms:
                chosen = max(mid_same)
            elif old_same and not young_same:
                # Oldest old same-len MS (file30402@273: 133 over 137).
                # Skip if a young same-len tip exists (file30400@244 → lit).
                chosen = min(old_same)
            else:
                chosen = max(lit)

        # Literal tip → medium-owner cover upgrades:
        # - old lit (age ≥ 160) + mid cover age 32..64, longest owner
        #   (file30400@276)
        # - any lit + mid cover age ≥ 128, longest owner (file30402@417)
        if chosen is not None and chosen in lit:
            thr = max(2 * best_len, LONG_MS_ABS_FLOOR)
            covs_near: list[int] = []
            covs_old: list[int] = []
            for ref in cands:
                o = owners.get(ref, ref)
                info = token_starts.get(o)
                age = pos - ref
                if not (
                    info is not None
                    and info[0] == "M"
                    and best_len < info[1] < thr
                    and ref + best_len - 1 == o + info[1] - 1
                ):
                    continue
                if OLD_LIT_COVER_MIN_AGE <= age <= OLD_LIT_COVER_MAX_AGE:
                    covs_near.append(ref)
                if age >= MIDCOV_OVER_LIT_MIN_AGE:
                    covs_old.append(ref)
            if (pos - chosen) >= OLD_LIT_MIN_AGE and covs_near:
                chosen = max(
                    covs_near,
                    key=lambda c: (token_starts[owners.get(c, c)][1], c),
                )
            elif covs_old:
                chosen = max(
                    covs_old,
                    key=lambda c: (token_starts[owners.get(c, c)][1], c),
                )

    # 5. Global arithmetic progression (ladder motifs like refs 4,12,20).
    #    Prefer 2nd-newest same-len MS when the plain AP 2nd is not same-len
    #    (file30400@408: same@372 over AP 2nd cover@384).
    if chosen is None and _is_arithmetic_period(cands):
        sc = sorted(cands, reverse=True)
        ap_pick = sc[1]
        tip = token_starts.get(ap_pick)
        if tip is not None and tip[0] == "M" and tip[1] == best_len:
            chosen = ap_pick
        else:
            same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
            ]
            if len(same) >= 2:
                chosen = sorted(same, reverse=True)[1]
            elif len(same) == 1:
                chosen = same[0]
            else:
                chosen = ap_pick

    # 6. Pure owner-local AP with period ≥ 4 (short zero-run ties, best ≤ 2).
    if chosen is None and best_len <= 2:
        by_owner: dict[int, list[int]] = defaultdict(list)
        for ref in cands:
            by_owner[owners.get(ref, ref)].append(ref)
        best_key = (-1, -1)  # (ap_len, owner_phrase_len)
        best_pick: int | None = None
        for o, group in by_owner.items():
            if not _is_arithmetic_period(group, min_period=OWNER_AP_MIN_PERIOD):
                continue
            s = sorted(set(group))
            info = token_starts.get(o)
            ol = info[1] if info is not None and info[0] == "M" else 0
            key = (len(s), ol)
            if key > best_key:
                best_key = key
                best_pick = sorted(s, reverse=True)[1]
        if best_pick is not None:
            chosen = best_pick

    # 6b. best==2: 2nd-newest cover among recent M3 owner phrases
    #     (file30402@282: 274 over 278).
    if chosen is None and best_len == 2:
        m3_cov: list[int] = []
        for ref in cands:
            o = owners.get(ref, ref)
            info = token_starts.get(o)
            if (
                info is not None
                and info[0] == "M"
                and info[1] == 3
                and ref + best_len - 1 == o + info[1] - 1
                and (pos - o) <= B2_M3_OWNER_MAX_AGE
            ):
                m3_cov.append(ref)
        if len(m3_cov) >= 2:
            chosen = sorted(m3_cov, reverse=True)[1]

    # 7. Short match: tip is a very young phrase-end of a long MS → cover of
    #    the most recent prior long MS whose length differs from the tip's
    #    (skip same-length ladder continuations like M17@176 before M17@193).
    #    file30400@210: cover of M13@27 (ref 38) over PE 209 of M17@193.
    if chosen is None and best_len <= 2:
        newest = max(cands)
        o = owners.get(newest, newest)
        oi = token_starts.get(o)
        if (
            (pos - newest) < PRIOR_LONG_TIP_AGE
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= PRIOR_LONG_MIN_PL
            and newest == o + oi[1] - 1
        ):
            tip_pl = oi[1]
            prior = [
                k
                for k, v in token_starts.items()
                if v[0] == "M"
                and v[1] >= PRIOR_LONG_MIN_PL
                and k < o
                and v[1] != tip_pl
            ]
            if not prior:
                prior = [
                    k
                    for k, v in token_starts.items()
                    if v[0] == "M" and v[1] >= PRIOR_LONG_MIN_PL and k < o
                ]
            if prior:
                pl = max(prior)
                want = pl + token_starts[pl][1] - best_len
                if want in cands:
                    chosen = want

    if chosen is None:
        chosen = max(cands)

    # 8b. best ≥ 7: among cands at +1 into an M4 phrase, prefer oldest
    #     (file30400@281: 237 over 265/257).
    if best_len >= M4_PLUS1_MIN_BEST:
        hits: list[int] = []
        for c in cands:
            o = owners.get(c, c)
            info = token_starts.get(o)
            if (
                info is not None
                and info[0] == "M"
                and info[1] == 4
                and c == o + 1
            ):
                hits.append(c)
        if len(hits) >= 2 and chosen in hits:
            chosen = min(hits)

    # 8c. Young cover tip yields to the newest older cand
    #     (file30400@317: 253 over cover 293 age 24).
    if best_len >= YOUNG_COVER_YIELD_MIN_BEST:
        o = owners.get(chosen, chosen)
        info = token_starts.get(o)
        is_cov = (
            info is not None
            and info[0] == "M"
            and chosen + best_len - 1 == o + info[1] - 1
        )
        if is_cov and (pos - chosen) < YOUNG_COVER_YIELD_AGE:
            older = [c for c in cands if c < chosen]
            if older:
                chosen = max(older)

    # 8d. Exactly two untyped cands, one cover / one not → non-cover
    #     (file30402@478: 334 over cover 342).
    if best_len >= PAIR_NONCOVER_MIN_BEST and len(cands) == 2:
        a, b = cands[0], cands[1]
        if token_starts.get(a) is None and token_starts.get(b) is None:

            def _is_cov(c: int) -> bool:
                o = owners.get(c, c)
                info = token_starts.get(o)
                return bool(
                    info is not None
                    and info[0] == "M"
                    and c + best_len - 1 == o + info[1] - 1
                )

            ca, cb = _is_cov(a), _is_cov(b)
            if ca and not cb:
                chosen = b
            elif cb and not ca:
                chosen = a

    # 8e. Prior-long picked a same-length long cover; prefer newest old
    #     medium MS start (file30503@326: M6@182 over M17 cover).
    if best_len <= 2:
        newest = max(cands)
        o = owners.get(newest, newest)
        oi = token_starts.get(o)
        if (
            (pos - newest) < PRIOR_LONG_TIP_AGE
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= PRIOR_LONG_MIN_PL
            and newest == o + oi[1] - 1
        ):
            tip_pl = oi[1]
            co = owners.get(chosen, chosen)
            ci = token_starts.get(co)
            if ci is not None and ci[0] == "M" and ci[1] == tip_pl:
                ms = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "M"
                    and PRIOR_SAME_PL_MS_MIN_PL
                    <= token_starts[c][1]
                    < PRIOR_LONG_MIN_PL
                    and (pos - c) >= PRIOR_SAME_PL_MS_MIN_AGE
                ]
                if ms:
                    chosen = max(ms)

    # 8f. Two-cand lit+same: same beats a very old literal
    #     (file30400@353 after force-lit: M3@341 over L@109).
    if best_len >= 3 and len(cands) == 2:
        lit = [
            c
            for c in cands
            if token_starts.get(c) is not None and token_starts[c][0] == "L"
        ]
        same = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] == best_len
        ]
        if (
            len(lit) == 1
            and len(same) == 1
            and chosen in lit
            and same[0] > lit[0]
            and (pos - lit[0]) >= OLD_COVER_MIN_AGE
        ):
            chosen = same[0]

    # 8g. best==5: oldest phrase-end of an M3 owner
    #     (file30400@383: 347 over 359).
    if best_len == OLDEST_PE3_BEST:
        pes: list[int] = []
        for c in cands:
            o = owners.get(c, c)
            info = token_starts.get(o)
            if (
                info is not None
                and info[0] == "M"
                and info[1] == 3
                and c == o + 2
            ):
                pes.append(c)
        if len(pes) >= 2 and chosen in pes:
            chosen = min(pes)

    # 8h. Young long-cover tip → 2nd-newest long cover owner + best//2
    #     (file30503@330: 295 over 320).
    if best_len >= SECOND_LONG_HALF_MIN_BEST:
        long_cov: list[tuple[int, int]] = []
        for ref in cands:
            o = owners.get(ref, ref)
            info = token_starts.get(o)
            if (
                info is not None
                and info[0] == "M"
                and info[1] >= SECOND_LONG_HALF_THR
                and ref + best_len - 1 == o + info[1] - 1
            ):
                long_cov.append((ref, o))
        if len(long_cov) >= 2:
            sc = sorted(long_cov, key=lambda x: x[0], reverse=True)
            if (
                chosen == sc[0][0]
                and (pos - sc[0][0]) <= SECOND_LONG_HALF_YOUNG
            ):
                want = sc[1][1] + best_len // 2
                if want in cands:
                    chosen = want

    # 8i. Mid inside a long owner → short-owner cover
    #     best==3 mid of pl≥8 → newest M4 cover age≥144 (file30400@389)
    #     best==2 mid of pl≥7 → newest M3 cover age≥128 (file30402@562)
    if best_len in (2, 3):
        o = owners.get(chosen, chosen)
        info = token_starts.get(o)
        long_pl = MID_LONG_TO_M4_PL if best_len == 3 else MID_LONG_TO_M3_PL
        short_pl = 4 if best_len == 3 else 3
        min_age = MID_LONG_TO_M4_AGE if best_len == 3 else MID_LONG_TO_M3_AGE
        if (
            info is not None
            and info[0] == "M"
            and info[1] >= long_pl
            and chosen + best_len - 1 != o + info[1] - 1
            and token_starts.get(chosen) is None
        ):
            cov = []
            for ref in cands:
                oo = owners.get(ref, ref)
                oi = token_starts.get(oo)
                if (
                    oi is not None
                    and oi[0] == "M"
                    and oi[1] == short_pl
                    and ref + best_len - 1 == oo + oi[1] - 1
                    and (pos - ref) >= min_age
                ):
                    cov.append(ref)
            if cov:
                chosen = max(cov)

    # 8j. same-len MS pick → cover of most recent (best+1)-length MS if younger
    #     (file30400@396: cover of M5@383 over same M4@372).
    if best_len >= YOUNG_PREV_COVER_MIN_BEST:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "M" and tip[1] == best_len:
            recent = [
                k
                for k, v in token_starts.items()
                if v[0] == "M"
                and v[1] == best_len + 1
                and (pos - k) <= YOUNG_PREV_MS_MAX_AGE
            ]
            if recent:
                prev = max(recent)
                want = prev + 1
                if (
                    want in cands
                    and (pos - want) <= YOUNG_PREV_COVER_MAX_AGE
                    and (pos - want) < (pos - chosen)
                ):
                    chosen = want

    # 8k. Two old untyped covers → older (file30503@336: 192 over 200).
    if best_len >= PAIR_OLDER_COVER_MIN_BEST and len(cands) == 2:
        a, b = cands[0], cands[1]
        if token_starts.get(a) is None and token_starts.get(b) is None:

            def _pair_is_cov(c: int) -> bool:
                o = owners.get(c, c)
                info = token_starts.get(o)
                return bool(
                    info is not None
                    and info[0] == "M"
                    and c + best_len - 1 == o + info[1] - 1
                )

            if (
                _pair_is_cov(a)
                and _pair_is_cov(b)
                and (pos - a) >= PAIR_OLDER_COVER_MIN_AGE
                and (pos - b) >= PAIR_OLDER_COVER_MIN_AGE
            ):
                chosen = min(a, b)

    # 8l. Two cands inside same long owner (best 5..8) → older
    #     (file30400@401: 182 over 190 of M17@176).
    if (
        SAME_OWNER_OLDER_MIN_BEST <= best_len <= SAME_OWNER_OLDER_MAX_BEST
        and len(cands) == 2
    ):
        o = owners.get(chosen, chosen)
        info = token_starts.get(o)
        a, b = cands[0], cands[1]
        if (
            info is not None
            and info[0] == "M"
            and info[1] >= SAME_OWNER_OLDER_MIN_PL
            and owners.get(a, a) == o
            and owners.get(b, b) == o
            and (pos - a) >= SAME_OWNER_OLDER_MIN_AGE
            and (pos - b) >= SAME_OWNER_OLDER_MIN_AGE
        ):
            chosen = min(a, b)

    # 8m. Very young phrase-end of a same-len owner → cover of recent longer MS
    #     (file30503@340: cover of M6@330 over pe of M4@336).
    if best_len >= YOUNG_PE_MID_COVER_MIN_BEST:
        o = owners.get(chosen, chosen)
        info = token_starts.get(o)
        if (
            info is not None
            and info[0] == "M"
            and info[1] == best_len
            and chosen == o + best_len - 1
            and (pos - chosen) < YOUNG_PE_MAX_AGE
        ):
            covs = []
            for ref in cands:
                oo = owners.get(ref, ref)
                oi = token_starts.get(oo)
                if (
                    oi is not None
                    and oi[0] == "M"
                    and oi[1] > best_len
                    and oi[1] >= YOUNG_PE_MID_PL_MIN
                    and ref + best_len - 1 == oo + oi[1] - 1
                    and (pos - ref) <= YOUNG_PE_COVER_MAX_AGE
                ):
                    covs.append(ref)
            if covs:
                chosen = max(covs)

    # 8n. Two cands: typed mid-MS vs untyped cover → cover
    #     (file30503@344: cover of M8@204 over mid M17@216).
    if best_len >= MID_VS_COVER_MIN_BEST and len(cands) == 2:
        a, b = cands[0], cands[1]

        def _mid_cov_kind(c: int) -> tuple[bool, bool]:
            tsc = token_starts.get(c)
            o = owners.get(c, c)
            oi = token_starts.get(o)
            is_mid = bool(
                tsc is not None and tsc[0] == "M" and tsc[1] > best_len
            )
            is_cov = bool(
                oi is not None
                and oi[0] == "M"
                and c + best_len - 1 == o + oi[1] - 1
                and tsc is None
            )
            return is_mid, is_cov

        am, ac = _mid_cov_kind(a)
        bm, bc = _mid_cov_kind(b)
        if am and bc:
            chosen = b
        elif bm and ac:
            chosen = a

    # 8o. best==3: among mid-MS of the same pl as the pick, prefer newest
    #     (file30400@413: M4@408 over M4@396).
    if best_len == NEWER_SAME_MID_BEST:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "M" and tip[1] > best_len:
            pl = tip[1]
            same_mid = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == pl
            ]
            if len(same_mid) >= 2:
                newest_mid = max(same_mid)
                if newest_mid > chosen:
                    chosen = newest_mid

    thr = max(2 * best_len, LONG_MS_ABS_FLOOR)

    # 8p. Untyped mid of medium owner → oldest long cover age ≥ 128
    #     (file30400@424: 189 over mid 332 of M10).
    if best_len >= MID_MED_TO_LONG_COV_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        is_mid_med = (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and best_len < oi[1] < thr
            and chosen + best_len - 1 != o + oi[1] - 1
        )
        if is_mid_med:
            long_cov = [
                ref
                for ref in cands
                if token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] >= thr
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and token_starts.get(ref) is None
                and (pos - ref) >= MID_MED_TO_LONG_COV_MIN_AGE
            ]
            if long_cov:
                chosen = min(long_cov)

    # 8q. Old long cover → newest mid of medium owner
    #     (file30400@429: mid 329 of M10 over cover 203).
    if best_len >= LONG_COV_TO_MID_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        is_long_cov = (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= thr
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) >= LONG_COV_TO_MID_MIN_AGE
        )
        if is_long_cov:
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and best_len
                < token_starts[owners.get(ref, ref)][1]
                < thr
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
            ]
            if mids:
                chosen = max(mids)

    # 8r. Mid-age same-len pe → mid-owner cover
    #     best ≥ 4: newest cover age in [24, 64) (file30503@372)
    #     else newest cover age ≥ 128 (file30503@413); best==3 young fallback
    if best_len >= PE_MID_COVER_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        pe_age = pos - chosen
        if (
            oi is not None
            and oi[0] == "M"
            and oi[1] == best_len
            and chosen == o + best_len - 1
            and PE_MID_COVER_PE_MIN_AGE <= pe_age < PE_MID_COVER_PE_MAX_AGE
        ):

            def _mid_covers(lo: int, hi: int | None = None) -> list[int]:
                out: list[int] = []
                for ref in cands:
                    oo = owners.get(ref, ref)
                    oi2 = token_starts.get(oo)
                    age = pos - ref
                    if not (
                        oi2 is not None
                        and oi2[0] == "M"
                        and best_len < oi2[1] < thr
                        and ref + best_len - 1 == oo + oi2[1] - 1
                    ):
                        continue
                    if age >= lo and (hi is None or age < hi):
                        out.append(ref)
                return out

            if best_len >= 4:
                young = _mid_covers(
                    PE_MID_COVER_YOUNG_MIN, PE_MID_COVER_YOUNG_MAX
                )
                if young:
                    chosen = max(young)
                else:
                    old = _mid_covers(PE_MID_COVER_OLD_MIN)
                    if old:
                        chosen = max(old)
            else:
                old = _mid_covers(PE_MID_COVER_OLD_MIN)
                if old:
                    chosen = max(old)
                else:
                    young = _mid_covers(
                        PE_MID_COVER_YOUNG_MIN, PE_MID_COVER_YOUNG_MAX
                    )
                    if young:
                        chosen = max(young)

    # 8s. Young cover of longer owner → mid of medium owner pl ≥ 8, age ≥ 96
    #     (file30400@437: mid 333 of M10 over cover 433 of M7).
    if best_len >= YOUNG_COV_TO_MID8_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] > best_len
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) < YOUNG_COV_TO_MID8_MAX_AGE
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1]
                >= YOUNG_COV_TO_MID8_OWNER_PL
                and token_starts[owners.get(ref, ref)][1] < thr
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and (pos - ref) >= YOUNG_COV_TO_MID8_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8t. Long cover → pe of short owner (pl ≤ 2) age in [32, 96)
    #     (file30503@385: pe 329 of M2 over cover 319).
    if best_len >= LONG_COV_TO_SHORT_PE_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= thr
            and chosen + best_len - 1 == o + oi[1] - 1
        ):
            pes = [
                ref
                for ref in cands
                if token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1]
                <= LONG_COV_TO_SHORT_PE_MAX_PL
                and ref
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and LONG_COV_TO_SHORT_PE_MIN_AGE
                <= (pos - ref)
                < LONG_COV_TO_SHORT_PE_MAX_AGE
            ]
            if pes:
                chosen = max(pes)

    # 8u. Two cands: pe of short loses to mid of long owner age ≥ 128
    #     (file30503@407: mid 231 of M17 over pe 327 of M2).
    if best_len >= SHORT_PE_VS_LONG_MID_MIN_BEST and len(cands) == 2:
        a, b = cands[0], cands[1]

        def _short_pe_long_mid(c: int) -> tuple[bool, bool]:
            o = owners.get(c, c)
            oi = token_starts.get(o)
            tip = token_starts.get(c)
            pe_short = bool(
                oi is not None
                and oi[0] == "M"
                and oi[1] <= LONG_COV_TO_SHORT_PE_MAX_PL
                and c == o + oi[1] - 1
            )
            mid_long = bool(
                tip is None
                and oi is not None
                and oi[0] == "M"
                and oi[1] >= thr
                and c + best_len - 1 != o + oi[1] - 1
                and (pos - c) >= SHORT_PE_VS_LONG_MID_MIN_AGE
            )
            return pe_short, mid_long

        pa, ma = _short_pe_long_mid(a)
        pb, mb = _short_pe_long_mid(b)
        if pa and mb:
            chosen = b
        elif pb and ma:
            chosen = a

    # 8v. Mid-age same-len pe (age in [48, 64)) → cover of most recent MS
    #     with pl ≥ best+4 (file30503@421: cover of M7@385 over pe 371).
    if best_len >= PE_RECENT_LONG_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        pe_age = pos - chosen
        if (
            oi is not None
            and oi[0] == "M"
            and oi[1] == best_len
            and chosen == o + best_len - 1
            and PE_RECENT_LONG_PE_MIN_AGE
            <= pe_age
            < PE_RECENT_LONG_PE_MAX_AGE
        ):
            recent = [
                k
                for k, v in token_starts.items()
                if v[0] == "M"
                and best_len + PE_RECENT_LONG_PL_ADD <= v[1] < thr
                and (pos - k) < pe_age
            ]
            if recent:
                prev = max(recent)
                want = prev + token_starts[prev][1] - best_len
                if (
                    want in cands
                    and PE_RECENT_LONG_COV_MIN_AGE
                    <= (pos - want)
                    < PE_RECENT_LONG_COV_MAX_AGE
                ):
                    chosen = want

    # 8w. best ≤ 2: very young pe of M4 → newest same-pl cover age ≥ 64
    #     (file30503@446: cover 382 of M4@380 over pe 445).
    if best_len <= YOUNG_PE4_TO_COV_MAX_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            oi is not None
            and oi[0] == "M"
            and oi[1] == YOUNG_PE4_OWNER_PL
            and chosen == o + oi[1] - 1
            and (pos - chosen) < YOUNG_PE4_MAX_AGE
        ):
            covs = [
                ref
                for ref in cands
                if token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == YOUNG_PE4_OWNER_PL
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and token_starts.get(ref) is None
                and (pos - ref) >= YOUNG_PE4_COV_MIN_AGE
            ]
            if covs:
                chosen = max(covs)

    # 8x. best ≤ 2: long MS tip → oldest untyped mid of a long owner
    #     (file30503@460: mid 234 of M17@233 over tip 309).
    if best_len <= LONG_MS_TO_OLD_MID_MAX_BEST:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "M" and tip[1] >= thr:
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] >= thr
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
            ]
            if mids:
                chosen = min(mids)

    # 8y. best ≤ 2: young cover of pl > best+1 → cover of pl == best+1
    #     (file30503@462: cover 408 of M3 over cover 444 of M4).
    if best_len <= YOUNG_COV_TO_SHORT_COV_MAX_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] > best_len + 1
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) < YOUNG_COV_TO_SHORT_COV_MAX_AGE
        ):
            want_pl = best_len + 1
            covs = [
                ref
                for ref in cands
                if token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == want_pl
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and token_starts.get(ref) is None
                and (pos - ref) >= YOUNG_COV_TO_SHORT_COV_MIN_AGE
            ]
            if covs:
                chosen = max(covs)

    # 8z. Old same-len pe → newest cover of pl == best+1 with age ≥ pe_age
    #     (file30503@481: cover 365 of M4@364 over pe 371).
    if best_len >= OLD_PE_TO_NEXT_COV_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        pe_age = pos - chosen
        if (
            oi is not None
            and oi[0] == "M"
            and oi[1] == best_len
            and chosen == o + best_len - 1
            and pe_age >= OLD_PE_TO_NEXT_COV_MIN_AGE
        ):
            want_pl = best_len + 1
            if want_pl < thr:
                covs = [
                    ref
                    for ref in cands
                    if token_starts.get(owners.get(ref, ref)) is not None
                    and token_starts[owners.get(ref, ref)][0] == "M"
                    and token_starts[owners.get(ref, ref)][1] == want_pl
                    and ref + best_len - 1
                    == owners.get(ref, ref)
                    + token_starts[owners.get(ref, ref)][1]
                    - 1
                    and token_starts.get(ref) is None
                    and (pos - ref) >= pe_age
                ]
                if covs:
                    chosen = max(covs)

    # 8aa. best==3: very old M4 cover (age ≥ 176) with a long mid in cands
    #      → oldest same-len MS age ≥ 96 (file30503@557: same@349).
    #      Age floor 176 preserves file30400@389 (M4 cover age 144).
    if best_len == M4COV_TO_OLD_SAME_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) >= M4COV_TO_OLD_SAME_MIN_AGE
        ):
            long_mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] >= thr
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
            ]
            if long_mids:
                same = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "M"
                    and token_starts[c][1] == best_len
                    and (pos - c) >= M4COV_TO_OLD_SAME_MS_AGE
                ]
                if same:
                    chosen = min(same)

    # 8ab. Mid of pl ∈ [8, thr) → newest same-len MS age < 48
    #      (file30503@585: same@557 over mid of M8 from young-cover→mid8).
    if best_len >= MID8_TO_SAME_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and MID8_TO_SAME_OWNER_PL <= oi[1] < thr
            and chosen + best_len - 1 != o + oi[1] - 1
        ):
            same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
                and (pos - c) < MID8_TO_SAME_MAX_AGE
            ]
            if same:
                chosen = max(same)

    # 8ac. best ≤ 2: young cover of pl == best+1 → longer medium cover age ≥ 96
    #      (file30503@590: cover of M11@485 over cover of M3@585).
    if best_len <= YOUNG_SHORT_COV_TO_MED_MAX_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == best_len + 1
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) < YOUNG_SHORT_COV_MAX_AGE
        ):
            covs = [
                ref
                for ref in cands
                if token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and oi[1]
                < token_starts[owners.get(ref, ref)][1]
                < thr
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and token_starts.get(ref) is None
                and (pos - ref) >= YOUNG_SHORT_COV_MED_MIN_AGE
            ]
            if covs:
                chosen = max(covs)

    # 8ad. best==2, lit+same pair: old lit beats moderately old same
    #      (file30503@632: L@410; same-age gate preserves @646 same@450).
    if best_len == 2 and len(cands) == 2:
        lit = [
            c
            for c in cands
            if token_starts.get(c) is not None and token_starts[c][0] == "L"
        ]
        same = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] == best_len
        ]
        if (
            len(lit) == 1
            and len(same) == 1
            and (pos - lit[0]) >= B2_LIT_OVER_SAME_LIT_MIN_AGE
            and (pos - same[0]) < B2_LIT_OVER_SAME_SAME_MAX_AGE
        ):
            chosen = lit[0]

    # 8ae. best ≤ 2: young pe of M2 → newest M4 cover age ≥ 68
    #      (file30503@634: cover 566 of M4@564 over pe 633).
    if best_len <= YOUNG_PE2_TO_M4_MAX_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen == o + oi[1] - 1
            and (pos - chosen) < YOUNG_PE2_MAX_AGE
        ):
            covs = [
                ref
                for ref in cands
                if token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == 4
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and token_starts.get(ref) is None
                and (pos - ref) >= YOUNG_PE2_COV_MIN_AGE
            ]
            if covs:
                chosen = max(covs)

    # 8af. best==2: mid of medium → newest same-len MS age ≥ 160
    #      (file30503@644: same M2@462 over mid of M6@550).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and best_len < oi[1] < thr
            and chosen + best_len - 1 != o + oi[1] - 1
        ):
            same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
                and (pos - c) >= B2_MID_TO_OLD_SAME_MIN_AGE
            ]
            if same:
                chosen = max(same)

    # 8ag. best==2, two same-len MS (young + very old) → oldest old
    #      (file30503@674: M2@448 over M2@632).
    if best_len == 2 and len(cands) == 2:
        same = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] == best_len
        ]
        if len(same) == 2:
            old = [c for c in same if (pos - c) >= B2_SAME_PAIR_OLD_MIN]
            young = [c for c in same if (pos - c) < B2_SAME_PAIR_YOUNG_MAX]
            if old and young:
                chosen = min(old)

    # 8ah. best==3: mid of M8 after young cover → M7 cover age ≥ 96
    #      (file30503@781: 669 over mid 637 of M8).
    if best_len == MID8_YOUNG_COV_TO_M7_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        is_mid8 = (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen + best_len - 1 != o + oi[1] - 1
            and (pos - chosen) >= MID8_YOUNG_COV_MID_MIN_AGE
        )
        if is_mid8:
            young_cov = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] > best_len
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and (pos - ref) < MID8_YOUNG_COV_TIP_MAX_AGE
            ]
            if young_cov:
                covs = [
                    ref
                    for ref in cands
                    if token_starts.get(owners.get(ref, ref)) is not None
                    and token_starts[owners.get(ref, ref)][0] == "M"
                    and token_starts[owners.get(ref, ref)][1] == 7
                    and ref + best_len - 1
                    == owners.get(ref, ref)
                    + token_starts[owners.get(ref, ref)][1]
                    - 1
                    and token_starts.get(ref) is None
                    and (pos - ref) >= MID8_YOUNG_COV_M7_MIN_AGE
                ]
                if covs:
                    chosen = max(covs)

    # 8ai. Young same-len MS → cover of pl == 2×best age < 16
    #      (file30503@785: cover of M6@774 over same@781).
    if best_len >= YOUNG_SAME_TO_DOUBLE_COV_MIN_BEST:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == best_len
            and (pos - chosen) < YOUNG_SAME_TO_DOUBLE_COV_MAX_AGE
        ):
            want_pl = best_len * 2
            if want_pl < thr:
                covs = [
                    ref
                    for ref in cands
                    if token_starts.get(ref) is None
                    and token_starts.get(owners.get(ref, ref)) is not None
                    and token_starts[owners.get(ref, ref)][0] == "M"
                    and token_starts[owners.get(ref, ref)][1] == want_pl
                    and ref + best_len - 1
                    == owners.get(ref, ref)
                    + token_starts[owners.get(ref, ref)][1]
                    - 1
                    and (pos - ref) < YOUNG_SAME_TO_DOUBLE_COV_COVER_MAX
                ]
                if covs:
                    chosen = max(covs)

    # 8aj. best ≥ 6: mid of short owner → young same-len MS tip
    #      (file30503@813: same@789 over mid of M2 after cover-yield).
    if best_len >= MID_SHORT_TO_SAME_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] <= MID_SHORT_OWNER_MAX_PL
            and chosen + best_len - 1 != o + oi[1] - 1
        ):
            same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
                and (pos - c) < MID_SHORT_TO_SAME_MAX_AGE
            ]
            if same:
                chosen = max(same)

    # 8ak. Young med MS → mid-age lit (file30503@840: L@800 over M8@832).
    if best_len >= YOUNG_MED_TO_LIT_MIN_BEST:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] > best_len
            and (pos - chosen) < YOUNG_MED_TO_LIT_MED_MAX_AGE
        ):
            lit = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and YOUNG_MED_TO_LIT_LIT_MIN_AGE
                <= (pos - c)
                < YOUNG_MED_TO_LIT_LIT_MAX_AGE
            ]
            if lit:
                chosen = max(lit)

    # 8al. best ≤ 2: mid of medium age < 64 → old med MS age ≥ 200
    #      (file30503@856: M8@636 over mid of M7@813).
    if best_len <= 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and best_len < oi[1] < thr
            and chosen + best_len - 1 != o + oi[1] - 1
            and (pos - chosen) < B2_MID_TO_OLD_MED_MAX_AGE
        ):
            old_med = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] > best_len
                and (pos - c) >= B2_MID_TO_OLD_MED_MIN_AGE
            ]
            if old_med:
                chosen = max(old_med)

    # 8am. best ≥ 4, two cands: mid inside long MS + tip → tip (age ≥ 160)
    #      (file30503@864: M17@676 over mid 688).
    if best_len >= MID_LONG_TO_TIP_MIN_BEST and len(cands) == 2:
        tips = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] >= thr
        ]
        if len(tips) == 1:
            tip_ref = tips[0]
            other = cands[0] if cands[1] == tip_ref else cands[1]
            o = owners.get(other, other)
            oi = token_starts.get(o)
            if (
                o == tip_ref
                and oi is not None
                and oi[0] == "M"
                and tip_ref <= other <= tip_ref + oi[1] - 1
                and (pos - tip_ref) >= MID_LONG_TO_TIP_MIN_AGE
            ):
                chosen = tip_ref

    # 8an. best ≥ 4, two untyped covers: older ≥ 160 beats younger < 96
    #      (file30503@868: cover of M7@665 over cover of M6@774).
    if best_len >= PAIR_OLDER_COVER_LOOSE_MIN_BEST and len(cands) == 2:
        a, b = cands[0], cands[1]
        if token_starts.get(a) is None and token_starts.get(b) is None:

            def _loose_cov(c: int) -> bool:
                o = owners.get(c, c)
                info = token_starts.get(o)
                return bool(
                    info is not None
                    and info[0] == "M"
                    and c + best_len - 1 == o + info[1] - 1
                )

            if _loose_cov(a) and _loose_cov(b):
                older, newer = (a, b) if a < b else (b, a)
                if (
                    (pos - older) >= PAIR_OLDER_COVER_LOOSE_OLD_AGE
                    and (pos - newer) < PAIR_OLDER_COVER_LOOSE_YOUNG_AGE
                ):
                    chosen = older

    # 8ao. best==2, AP multi-max, sole same MS age ≥ 144 → newest M3 cover
    #      (file30503@922/@1066).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == best_len
            and (pos - chosen) >= AP_SAME_TO_M3_MIN_AGE
            and _is_arithmetic_period(cands)
        ):
            covs = [
                ref
                for ref in cands
                if token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == 3
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and token_starts.get(ref) is None
            ]
            if covs:
                chosen = max(covs)

    # 8ap. Young pe of long re-anchored to different long cover → old med MS
    #      (file30503@974: M6@774 over cover of M15@893).
    if best_len <= 2:
        newest = max(cands)
        o = owners.get(newest, newest)
        oi = token_starts.get(o)
        if (
            (pos - newest) < PRIOR_LONG_TIP_AGE
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= PRIOR_LONG_MIN_PL
            and newest == o + oi[1] - 1
        ):
            co = owners.get(chosen, chosen)
            ci = token_starts.get(co)
            is_long_cov = (
                token_starts.get(chosen) is None
                and ci is not None
                and ci[0] == "M"
                and ci[1] >= PRIOR_LONG_MIN_PL
                and chosen + best_len - 1 == co + ci[1] - 1
            )
            if is_long_cov:
                med = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "M"
                    and PRIOR_LONG_DIFF_MED_PL_MIN
                    <= token_starts[c][1]
                    < PRIOR_LONG_MIN_PL
                    and (pos - c) >= PRIOR_LONG_DIFF_TO_MED_MIN_AGE
                ]
                if med:
                    chosen = max(med)

    # 8aq. best==3: M4 cover age ≥ 148 + long mid → M7 MS age ≥ 176
    #      (file30503@977: M7@801).
    if best_len == 3:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) >= M4COV_TO_M7_MS_MIN_AGE
        ):
            long_mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] >= thr
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
            ]
            if long_mids:
                med = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "M"
                    and token_starts[c][1] == 7
                    and (pos - c) >= M4COV_TO_M7_MS_TIP_AGE
                ]
                if med:
                    chosen = max(med)

    # 8ar. Young-same branch M4 cover → M7 cover age ≥ 160
    #      (file30503@981: cover of M7@813).
    if best_len == 3:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) >= YOUNG_SAME_M4_TO_M7_MIN_AGE
        ):
            young_same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
                and (pos - c) < YOUNG_SAME_TIP_AGE
            ]
            if young_same:
                covs = [
                    ref
                    for ref in cands
                    if token_starts.get(ref) is None
                    and token_starts.get(owners.get(ref, ref)) is not None
                    and token_starts[owners.get(ref, ref)][0] == "M"
                    and token_starts[owners.get(ref, ref)][1] == 7
                    and ref + best_len - 1
                    == owners.get(ref, ref)
                    + token_starts[owners.get(ref, ref)][1]
                    - 1
                    and (pos - ref) >= YOUNG_SAME_M7_COV_MIN_AGE
                ]
                if covs:
                    chosen = max(covs)

    # 8as. mid_same age in [12, 24) → young mid of long pl ≥ 11
    #      (file30503@1001: mid of M11@989 over same@985).
    if best_len == 3:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == best_len
            and MID_SAME_TO_LONG_MID_MIN
            <= (pos - chosen)
            < MID_SAME_TO_LONG_MID_MAX
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1]
                >= MID_SAME_TO_LONG_MID_PL
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and (pos - ref) < MID_SAME_TO_LONG_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8at. best ≥ 4: 2+ untyped mids of same long owner → oldest age ≥ 64
    #      (file30503@1026: mid 941 of M17@940).
    if best_len >= SAME_LONG_MIDS_MIN_BEST:
        by_o: dict[int, list[int]] = {}
        for ref in cands:
            if token_starts.get(ref) is not None:
                continue
            o = owners.get(ref, ref)
            oi = token_starts.get(o)
            if (
                oi is not None
                and oi[0] == "M"
                and oi[1] >= thr
                and ref + best_len - 1 != o + oi[1] - 1
            ):
                by_o.setdefault(o, []).append(ref)
        for group in by_o.values():
            if len(group) >= 2 and chosen in group:
                old = [c for c in group if (pos - c) >= SAME_LONG_MIDS_MIN_AGE]
                if old:
                    chosen = min(old)
                    break

    # 8au. Late stream: lit + young same → same (file30503@1073).
    if (
        best_len >= 3
        and len(cands) == 2
        and pos >= LATE_LIT_TO_SAME_MIN_POS
    ):
        lit = [
            c
            for c in cands
            if token_starts.get(c) is not None and token_starts[c][0] == "L"
        ]
        same = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] == best_len
        ]
        if (
            len(lit) == 1
            and len(same) == 1
            and (pos - same[0]) < LATE_LIT_TO_SAME_SAME_MAX
            and (pos - lit[0]) < LATE_LIT_TO_SAME_LIT_MAX
        ):
            chosen = same[0]

    # 8av. Late stream: multiple old same → newest (file30503@1077).
    if best_len >= 3 and pos >= LATE_OLD_SAME_NEWEST_MIN_POS:
        same = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] == best_len
            and (pos - c) >= OLD_COVER_MIN_AGE
        ]
        if len(same) >= 2 and chosen == min(same):
            chosen = max(same)

    # 8aw. 2-cand lit + young same: prefer same when data[same-1] ≤ 36
    #      (file30400@449 / file30402@569); else keep lit (file30102@137).
    if (
        data is not None
        and best_len >= 3
        and len(cands) == 2
    ):
        lit = [
            c
            for c in cands
            if token_starts.get(c) is not None and token_starts[c][0] == "L"
        ]
        same = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] == best_len
        ]
        if (
            len(lit) == 1
            and len(same) == 1
            and (pos - same[0]) < LIT_SAME_SAME_MAX_AGE
            and (pos - lit[0]) < LIT_SAME_LIT_MAX_AGE
            and same[0] > 0
            and data[same[0] - 1] <= LIT_SAME_PREBYTE_SAME_MAX
        ):
            chosen = same[0]

    # 8ax. AP: AP-2nd is med MS tip + ≥3 same-len MS → newest same
    #      (file30402@573: same@425 over 2nd-same@421).
    if best_len >= AP_MED_TIP_NEWEST_SAME_MIN and _is_arithmetic_period(cands):
        sc = sorted(cands, reverse=True)
        if len(sc) >= 2:
            tip = token_starts.get(sc[1])
            if tip is not None and tip[0] == "M" and tip[1] > best_len:
                same = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "M"
                    and token_starts[c][1] == best_len
                ]
                if len(same) >= 3:
                    chosen = max(same)

    # 8ay. lit + ≥2 old same + M4 cover → newest old same
    #      (file30402@705: same@569 over min@565).
    if best_len >= 3:
        lit = [
            c
            for c in cands
            if token_starts.get(c) is not None and token_starts[c][0] == "L"
        ]
        same_old = [
            c
            for c in cands
            if token_starts.get(c) is not None
            and token_starts[c][0] == "M"
            and token_starts[c][1] == best_len
            and (pos - c) >= OLD_COVER_MIN_AGE
        ]
        m4c = [
            c
            for c in cands
            if token_starts.get(c) is None
            and token_starts.get(owners.get(c, c)) is not None
            and token_starts[owners.get(c, c)][0] == "M"
            and token_starts[owners.get(c, c)][1] == 4
            and c + best_len - 1 == owners.get(c, c) + 3
        ]
        if (
            lit
            and len(same_old) >= LIT_OLD_SAME_M4_MIN_SAME
            and m4c
            and chosen in same_old
        ):
            chosen = max(same_old)

    # 8az. M4 cover from young-same+old-same → young same tip
    #      (one young → tip; two+ age < 12 → 2nd-newest; file30402@709/@713).
    if best_len >= 3:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        is_m4_cov = (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) >= MID_COVER_MIN_AGE
        )
        if is_m4_cov:
            young = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
                and (pos - c) < M4COV_YOUNG_SAME_MAX_AGE
            ]
            old = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
                and (pos - c) >= OLD_COVER_MIN_AGE
            ]
            if young and old:
                if len(young) >= 2:
                    chosen = sorted(young, reverse=True)[1]
                else:
                    chosen = max(young)

    # 8ba. Young PE of long MS → older long mid just-before-PE (off_into=pl-2)
    #      age ≥ 64, newest (file30402@761: mid 667 of M17@652 over PE 757).
    if best_len >= 3:
        thr_pe = max(2 * best_len, LONG_MS_ABS_FLOOR)
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= thr_pe
            and chosen == o + oi[1] - 1
            and (pos - chosen) < YOUNG_LONG_PE_MAX_AGE
        ):
            want_into = oi[1] - 2
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and owners.get(ref, ref) < o
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] >= thr_pe
                and ref
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and ref - owners.get(ref, ref) == want_into
                and (pos - ref) >= YOUNG_LONG_PE_MID_MIN_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8bb. Sole young same-len tip + ≥2 old mid covers → newest mid cover
    #      (file30402@765: cover 633 of M4@632 over same@761).
    if best_len >= 3:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == best_len
            and (pos - chosen) < YOUNG_SAME_TIP_AGE
        ):
            same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
            ]
            if len(same) == 1 and same[0] == chosen:
                thr_loc = max(2 * best_len, LONG_MS_ABS_FLOOR)
                mids = [
                    ref
                    for ref in cands
                    if token_starts.get(owners.get(ref, ref)) is not None
                    and token_starts[owners.get(ref, ref)][0] == "M"
                    and best_len
                    < token_starts[owners.get(ref, ref)][1]
                    < thr_loc
                    and ref + best_len - 1
                    == owners.get(ref, ref)
                    + token_starts[owners.get(ref, ref)][1]
                    - 1
                    and (pos - ref) >= MID_COVER_MIN_AGE
                ]
                if len(mids) >= SOLE_YOUNG_SAME_MULTI_MID_MIN:
                    chosen = max(mids)

    # 8bc. Same-len MS tip age ≤ 8 → older long mid just-before-PE age ≥ 64
    #      (file30402@773: mid 701 of M17@686 over same@765).
    if best_len >= 3:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == best_len
            and (pos - chosen) <= SAME_TIP_TO_LONG_MID_MAX_AGE
        ):
            thr_mid = max(2 * best_len, LONG_MS_ABS_FLOOR)
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] >= thr_mid
                and ref
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and ref + best_len - 1
                != owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and ref - owners.get(ref, ref)
                == token_starts[owners.get(ref, ref)][1] - 2
                and (pos - ref) >= YOUNG_LONG_PE_MID_MIN_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8bd. Young M4 cover + ≥2 same-len MS tips → oldest same
    #      (file30402@801: same@761 over cover 797 of M4@796).
    if best_len >= 3:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) < YOUNG_M4COV_TO_OLD_SAME_MAX_AGE
        ):
            same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
            ]
            if len(same) >= YOUNG_M4COV_TO_OLD_SAME_MIN_SAME:
                chosen = min(same)

    # 8be. best ≥ 4: two untyped cands with index gap ≥ 32 → older
    #      (file30402@826: mid 762 of M3 over mid 794 of M4).
    if (
        best_len >= PAIR_UNTYPED_OLDER_MIN_BEST
        and len(cands) == 2
        and all(token_starts.get(c) is None for c in cands)
        and (max(cands) - min(cands)) >= PAIR_UNTYPED_OLDER_MIN_GAP
    ):
        chosen = min(cands)

    # 8bf. best ≤ 2: young short cover → old long mid age ≥ 128
    #      (file30402@842: mid 664 of M17@652 over cover 824 of M3@823).
    if best_len <= SHORT_COV_TO_LONG_MID_MAX_BEST and len(cands) == 2:
        thr_sc = max(2 * best_len, LONG_MS_ABS_FLOOR)
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] <= SHORT_COV_TO_LONG_MID_MAX_PL
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) < SHORT_COV_TO_LONG_MID_MAX_AGE
        ):
            other = cands[0] if cands[1] == chosen else cands[1]
            oo = owners.get(other, other)
            ooi = token_starts.get(oo)
            if (
                token_starts.get(other) is None
                and ooi is not None
                and ooi[0] == "M"
                and ooi[1] >= thr_sc
                and other != oo + ooi[1] - 1
                and other + best_len - 1 != oo + ooi[1] - 1
                and (pos - other) >= SHORT_COV_TO_LONG_MID_MIN_AGE
            ):
                chosen = other

    # 8bg. Old same-len MS tip → newest same-pl pe age in [24, 64)
    #      (file30402@861: pe 825 of M3@823 over same@761).
    if best_len >= 3:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == best_len
            and (pos - chosen) >= OLD_SAME_TO_PE_MIN_AGE
        ):
            pes = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == best_len
                and ref
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and OLD_SAME_TO_PE_PE_MIN_AGE
                <= (pos - ref)
                < OLD_SAME_TO_PE_PE_MAX_AGE
            ]
            if pes:
                chosen = max(pes)

    # 8bh. Just-finished long phrase (ends at pos-1) → PE when best ≥ 4
    #      (file20215@506; file20362@507; file107600@1020; file20601@496).
    if best_len >= JUST_FINISHED_LONG_PE_MIN_BEST:
        for o, info in token_starts.items():
            if info[0] != "M" or info[1] < JUST_FINISHED_LONG_MIN_PL:
                continue
            if o + info[1] != pos:
                continue
            pe = o + info[1] - 1
            if pe in cands:
                chosen = pe
                break

    # 8bi. Lit tip + just-finished long PE (best ≥ 3) → PE
    #      (file20507@509: pe 508 over L@440).
    if best_len >= 3:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            for o, info in token_starts.items():
                if info[0] != "M" or info[1] < JUST_FINISHED_LONG_MIN_PL:
                    continue
                if o + info[1] != pos:
                    continue
                pe = o + info[1] - 1
                if pe in cands:
                    chosen = pe
                    break

    # 8bj. Just-finished long tip (age == pl) with best ≥ pl → mid into pl-2
    #      (file304600@768: mid 766 over tip 751; full-length PE often absent).
    if best_len >= JUST_FINISHED_LONG_MIN_PL:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] >= JUST_FINISHED_LONG_MIN_PL
            and (pos - chosen) == tip[1]
            and best_len >= tip[1]
        ):
            mid = chosen + tip[1] - 2
            pe = chosen + tip[1] - 1
            if mid in cands:
                chosen = mid
            elif pe in cands:
                chosen = pe

    # 8bk. Long tip age == pl+1 (intervening lit) → PE
    #      (file100500@768: pe 766 of M17@750 over tip after L@767).
    if best_len >= JUST_FINISHED_LONG_MIN_PL - 1:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] >= JUST_FINISHED_LONG_MIN_PL
            and (pos - chosen) == tip[1] + 1
        ):
            pe = chosen + tip[1] - 1
            if pe in cands:
                chosen = pe

    # 8bl. Just-finished long with PE absent from cands → mid into pl-2
    #      (file20102@247; file635400@505; best==3 CLUT RLE tails).
    #      best ≥ 3 (not only ≥ 4) so short continuations after M17 re-anchor.
    if best_len >= 3:
        for o, info in token_starts.items():
            if info[0] != "M" or info[1] < JUST_FINISHED_LONG_MIN_PL:
                continue
            if o + info[1] != pos:
                continue
            pe = o + info[1] - 1
            mid = o + info[1] - 2
            if pe not in cands and mid in cands:
                chosen = mid
                break

    # 8bm. best==2: lit tip + same-len M2 age < 100 → newest same
    #      (file20604@325 same@249 over L@142; file107600@959 same@945).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            same = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == best_len
                and (pos - c) < B2_LIT_TO_SAME_MAX_AGE
            ]
            if same:
                chosen = max(same)

    # 8bn. best ≤ 2: just-finished pl ≥ 7, chosen inside → mid into pl-2
    #      (file20211@490 mid 488 of M17@473 over mid 486).
    if best_len <= 2:
        for o, info in token_starts.items():
            if info[0] != "M" or info[1] < B2_JUST_FIN_MID_MIN_PL:
                continue
            if o + info[1] != pos:
                continue
            pe = o + info[1] - 1
            mid = o + info[1] - 2
            if o <= chosen <= pe and mid in cands:
                chosen = mid
                break

    # 8bo. best ≥ long thr: just-finished medium pl ∈ [6, 13], chosen inside
    #      → mid into pl-2 (file110300@996 mid 994 of M7@989 over mid 992).
    if best_len >= JUST_FINISHED_LONG_MIN_PL:
        for o, info in token_starts.items():
            if info[0] != "M":
                continue
            pl = info[1]
            if pl < JUST_FIN_MED_MIN_PL or pl > JUST_FIN_MED_MAX_PL:
                continue
            if o + pl != pos:
                continue
            mid = o + pl - 2
            pe = o + pl - 1
            if o <= chosen <= pe and mid in cands:
                chosen = mid
                break

    # 8bp. best 4..8: young mid of just-finished long → newest med cover
    #      pl 5..8 age ≥ 24 (file110300@1013 cover 983 of M6 over mid 1011).
    if (
        YOUNG_LONG_MID_TO_MED_COV_MIN_BEST
        <= best_len
        <= YOUNG_LONG_MID_TO_MED_COV_MAX_BEST
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= JUST_FINISHED_LONG_MIN_PL
            and o + oi[1] == pos
            and (pos - chosen) < YOUNG_LONG_MID_TO_MED_COV_MAX_AGE
        ):
            covs = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and YOUNG_LONG_MID_TO_MED_COV_PL_LO
                <= token_starts[owners.get(ref, ref)][1]
                <= YOUNG_LONG_MID_TO_MED_COV_PL_HI
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and (pos - ref) >= YOUNG_LONG_MID_TO_MED_COV_MIN_AGE
            ]
            if covs:
                chosen = max(covs)

    # 8bq. best 4..8: mid-age long MS tip → newest med cover pl 5..8 age ≥ 32
    #      (file110300@1020 cover 984 of M6@982 over tip M17@996).
    if (
        YOUNG_LONG_MID_TO_MED_COV_MIN_BEST
        <= best_len
        <= YOUNG_LONG_MID_TO_MED_COV_MAX_BEST
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] >= JUST_FINISHED_LONG_MIN_PL
            and MID_LONG_TIP_TO_MED_COV_MIN_AGE
            <= (pos - chosen)
            < MID_LONG_TIP_TO_MED_COV_MAX_AGE
        ):
            covs = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and YOUNG_LONG_MID_TO_MED_COV_PL_LO
                <= token_starts[owners.get(ref, ref)][1]
                <= YOUNG_LONG_MID_TO_MED_COV_PL_HI
                and ref + best_len - 1
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and (pos - ref) >= MID_LONG_TIP_TO_MED_COV_MIN_AGE_COV
            ]
            if covs:
                chosen = max(covs)

    # 8br. best ≥ 4: lit tip + PE of short owner (pl ≤ 2) age < 12 → PE
    #      (file20513@360: pe 354 of M2@353 over L@352).
    if best_len >= LIT_TO_SHORT_PE_MIN_BEST:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) <= LIT_TO_SHORT_PE_LIT_MAX_AGE
        ):
            pes = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1]
                <= LIT_TO_SHORT_PE_MAX_PL
                and ref
                == owners.get(ref, ref)
                + token_starts[owners.get(ref, ref)][1]
                - 1
                and (pos - ref) < LIT_TO_SHORT_PE_PE_MAX_AGE
            ]
            if pes:
                chosen = max(pes)

    # 8bs. best==2: young M4 cover → newest M2/M3 tip age < 16
    #      (file302400@749 tip M3@745 over cover 741 of M4@739).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen + best_len - 1 == o + oi[1] - 1
            and (pos - chosen) < B2_M4COV_TO_SHORT_TIP_MAX_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] in (2, 3)
                and (pos - c) < B2_M4COV_TO_SHORT_TIP_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8bt. best ≥ 4, 2 cands: young med MS tip → mid-age lit
    #      (file20513@374: L@352 over M7@366).
    if best_len >= MED_TIP_TO_LIT_MIN_BEST and len(cands) == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] > best_len
            and (pos - chosen) < MED_TIP_TO_LIT_TIP_MAX_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and MED_TIP_TO_LIT_LIT_MIN_AGE
                <= (pos - c)
                < MED_TIP_TO_LIT_LIT_MAX_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8bu. best==3: lit tip + long mids age < 16 → newest mid
    #      (file20513@434: mid 422 of M17@409 over L@408).
    if best_len == 3:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            thr = max(2 * best_len, LONG_MS_ABS_FLOOR)
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] >= thr
                and (pos - c) < B3_LIT_TO_LONG_MID_MAX_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8bv. best==2, 2-cand: just-finished M2 tip + lit age in [14, 16) → lit
    #      (file20306@413: L@399 over tip@411; floor/cap keep file302400@641,
    #      file20400@476, file105300@456, file20359@324 on tip).
    if best_len == 2 and len(cands) == 2:
        prev_start = pos - 2
        if (
            token_starts.get(prev_start) is not None
            and token_starts[prev_start][0] == "M"
            and token_starts[prev_start][1] == 2
            and chosen == prev_start
        ):
            other = cands[0] if cands[1] == chosen else cands[1]
            ot = token_starts.get(other)
            if (
                ot is not None
                and ot[0] == "L"
                and B2_IMM_LIT_MIN_AGE
                <= (pos - other)
                < B2_IMM_LIT_MAX_AGE
            ):
                chosen = other

    # 8bw. best==2: young M2 tip → PE of M3 age ≥ 8
    #      (file20208@391: pe 377 of M3@375 over tip@383).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) < B2_TIP_TO_M3PE_TIP_MAX_AGE
        ):
            pes = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == 3
                and ref == owners.get(ref, ref) + 2
                and (pos - ref) >= B2_TIP_TO_M3PE_PE_MIN_AGE
            ]
            if pes:
                chosen = max(pes)

    # 8bx. best==2: young M2 tip → PE of M2 age in [8, 20)
    #      (file20415@323: pe 311 of M2@310 over tip@313).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) < B2_TIP_TO_M3PE_TIP_MAX_AGE
        ):
            pes = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == 2
                and ref == owners.get(ref, ref) + 1
                and B2_TIP_TO_M2PE_PE_MIN_AGE
                <= (pos - ref)
                < B2_TIP_TO_M2PE_PE_MAX_AGE
            ]
            if pes:
                chosen = max(pes)

    # 8by. best==2: young M2 tip age < 8 → newest older same tip age [8, 16)
    #      (file20208@415: same@405 over young tip@411).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) < B2_YOUNG_TO_OLDER_SAME_TIP_MAX
        ):
            olds = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and B2_YOUNG_TO_OLDER_SAME_OLD_MIN
                <= (pos - c)
                < B2_YOUNG_TO_OLDER_SAME_OLD_MAX
            ]
            if olds:
                chosen = max(olds)

    # 8bz. best==2: young M2 PE → lit age < 12
    #      (file20208@430: L@424 over pe 426 of M2@425).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen == o + 1
            and (pos - chosen) < B2_M2PE_TO_LIT_PE_MAX_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) < B2_M2PE_TO_LIT_LIT_MAX_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8ca. best==2: young M4 mid → M3 tip age in [16, 32)
    #      (file20359@302: tip M3@280 over mid 296 of M4@295).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and (pos - chosen) < B2_M4MID_TO_M3TIP_MID_MAX_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and B2_M4MID_TO_M3TIP_TIP_MIN_AGE
                <= (pos - c)
                < B2_M4MID_TO_M3TIP_TIP_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cb. best==2: M5 tip age < 12 → mid into +2 of same phrase
    #      (file20208@448: mid 444 over tip M5@442).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 5
            and (pos - chosen) < B2_M5TIP_TO_MID2_MAX_AGE
        ):
            mid = chosen + 2
            if mid in cands and owners.get(mid) == chosen:
                chosen = mid

    # 8cc. best==2: young M5 mid → M4 mid into +3 age in [8, 20)
    #      (file20359@366: mid 354 of M4@351 over mid 360 of M5@358).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and (pos - chosen) < B2_M5MID_TO_M4MID_MID_MAX_AGE
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B2_M5MID_TO_M4MID_INTO
                and B2_M5MID_TO_M4MID_AGE_MIN
                <= (pos - c)
                < B2_M5MID_TO_M4MID_AGE_MAX
            ]
            if mids:
                chosen = max(mids)

    # 8cd. best==2: young M4 mid age < 8 → lit age < 8
    #      (file20208@486: L@480 over mid 482 of M4@481).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and (pos - chosen) < B2_M4MID_TO_LIT_MAX_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) < B2_M4MID_TO_LIT_MAX_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8ce. best==3: young mid of pl ≥ 7 → M5 tip age in [12, 24)
    #      (file20359@461: tip M5@447 over mid 455 of M7@454).
    if best_len == 3:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= B3_MEDMID_TO_M5TIP_OWNER_MIN_PL
            and (pos - chosen) < B3_MEDMID_TO_M5TIP_MID_MAX_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and B3_MEDMID_TO_M5TIP_TIP_MIN_AGE
                <= (pos - c)
                < B3_MEDMID_TO_M5TIP_TIP_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf. best==2, 2-cand: M2 tip age == 30 + lit age ≥ 48 → lit
    #      (file20703@229 / file20712: L@181 over tip@199).
    if best_len == 2 and len(cands) == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_OLD_TIP_TO_LIT_TIP_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) >= B2_OLD_TIP_TO_LIT_LIT_MIN_AGE
                and (pos - c) > (pos - chosen)
            ]
            if lits:
                chosen = max(lits)

    # 8cf2. best==2: M2 tip age == 12 → lit age == 32
    #       (file20328@242 family).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP12_TO_LIT32_TIP_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_TIP12_TO_LIT32_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf3. best==2: M2 tip age == 2 at ring pos 0x22 → lit age == 24
    #       (file120400@34 family).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP2_TO_LIT24_TIP_AGE
            and (pos & 0xFF) == B2_TIP2_TO_LIT24_POS_MOD
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_TIP2_TO_LIT24_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf4. best==2: M2 tip age == 6 → lit age == 8 after ≥ 1 leading lit.
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP6_TO_LIT8_TIP_AGE
        ):
            lit_run = 0
            j = pos - 1
            while j >= 0 and token_starts.get(j) == ("L",):
                lit_run += 1
                j -= 1
            if lit_run >= B2_TIP6_TO_LIT8_MIN_LIT_RUN:
                lits = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "L"
                    and (pos - c) == B2_TIP6_TO_LIT8_LIT_AGE
                ]
                if lits:
                    chosen = max(lits)

    # 8cf5. best==2: M3 tip age == 4 → mid into +2 of M4 age in [48, 64)
    #       (file102000@110: mid 56 of M4@54 over tip M3@106).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP_TO_M4INTO2_TIP_AGE
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 4)
                and ref - owners.get(ref, ref) == B2_M4INTO2
                and B2_M4INTO2_MID_MIN_AGE
                <= (pos - ref)
                < B2_M4INTO2_MID_MAX_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf6. best==2, 2-cand: M2 tip age == 8 → lit age == 48.
    if best_len == 2 and len(cands) == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP8_TO_LIT48_TIP_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_TIP8_TO_LIT48_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf7. best==2: M3 tip age in [16, 40) → newest M2 tip age in [80, 96)
    #       (file102000@226: tip M2 age88 over tip M3 age32).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and B2_M3TIP_TO_M2TIP_M3_MIN_AGE
            <= (pos - chosen)
            < B2_M3TIP_TO_M2TIP_M3_MAX_AGE
        ):
            m2s = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and B2_M3TIP_TO_M2TIP_M2_MIN_AGE
                <= (pos - c)
                < B2_M3TIP_TO_M2TIP_M2_MAX_AGE
            ]
            if m2s:
                chosen = max(m2s)

    # 8cf8. best==2, 2-cand: just-finished M3, mid into +1 age == 2 → lit 12
    #       (file20328@316: L@304 over mid 314 of M3@313).
    if best_len == 2 and len(cands) == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_JF_M3MID_TO_LIT_INTO
            and (pos - chosen) == B2_JF_M3MID_TO_LIT_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_JF_M3MID_TO_LIT_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf9. best==2, 4-cand: M3 mid into +1 age == 8 → lit age in [48, 64)
    #       (file444000@622: L@564 over mid 614).
    if best_len == 2 and len(cands) == B2_M3MID8_TO_LIT_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == B2_M3MID8_TO_LIT_MID_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and B2_M3MID8_TO_LIT_LIT_MIN_AGE
                <= (pos - c)
                < B2_M3MID8_TO_LIT_LIT_MAX_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf10. best==3: just-finished M2, lit tip → mid into +1 of M2
    #        age in [12, 16) (file20328@326: mid 312 over L@302).
    if best_len == 3:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            if any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            ):
                mids = [
                    ref
                    for ref in cands
                    if token_starts.get(ref) is None
                    and token_starts.get(owners.get(ref, ref)) == ("M", 2)
                    and ref - owners.get(ref, ref) == B3_LIT_TO_M2INTO1
                    and B3_LIT_TO_M2INTO1_MID_MIN_AGE
                    <= (pos - ref)
                    < B3_LIT_TO_M2INTO1_MID_MAX_AGE
                ]
                if mids:
                    chosen = max(mids)

    # 8cf11. best==2, 4-cand: just-finished M2, M3 mid → M2 tip age [80, 96)
    #        (file444000@646: tip@562 over mid 610).
    if best_len == 2 and len(cands) == B2_M3MID_TO_M2TIP_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and B2_M3MID_TO_M2TIP_TIP_MIN_AGE
                <= (pos - c)
                < B2_M3MID_TO_M2TIP_TIP_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf12. best==2, 2-cand: M2 tip age == 2 → lit age == 16 when pos ≥ 300
    #        (file20328@346: L@330 over tip@344).
    if best_len == 2 and len(cands) == B2_TIP2_TO_LIT16_NC and pos >= B2_TIP2_TO_LIT16_MIN_POS:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP2_TO_LIT16_TIP_AGE
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_TIP2_TO_LIT16_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf13. best==2, 5-cand: M2 tip age == 2 → older M2 tip age in [64, 80)
    #        (file444000@712: tip@644 age68 over tip@710).
    if best_len == 2 and len(cands) == B2_TIP2_TO_OLD_M2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP2_TO_OLD_M2_TIP_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and B2_TIP2_TO_OLD_M2_TARGET_MIN_AGE
                <= (pos - c)
                < B2_TIP2_TO_OLD_M2_TARGET_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf14. best==2, 4-cand: just-finished L, M3 mid into +1 → M2 tip
    #        age in [40, 48) (file20328@353: tip@311 over mid 327).
    if best_len == 2 and len(cands) == B2_M3MID_TO_M2TIP42_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3MID_TO_M2TIP42_INTO
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and B2_M3MID_TO_M2TIP42_TIP_MIN_AGE
                <= (pos - c)
                < B2_M3MID_TO_M2TIP42_TIP_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf15. best==3: just-finished L, lit tip → mid into +1 of M2 age
    #        in [48, 64) (file444000@745: mid 685 over L@625).
    if best_len == 3:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            if any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            ):
                mids = [
                    ref
                    for ref in cands
                    if token_starts.get(ref) is None
                    and token_starts.get(owners.get(ref, ref)) == ("M", 2)
                    and ref - owners.get(ref, ref) == B3_LIT_TO_M2INTO1_OLD_INTO
                    and B3_LIT_TO_M2INTO1_OLD_MID_MIN_AGE
                    <= (pos - ref)
                    < B3_LIT_TO_M2INTO1_OLD_MID_MAX_AGE
                ]
                if mids:
                    chosen = max(mids)

    # 8cf16. best==2: just-finished M2, M4 mid into +2 age == 4 → M2 tip
    #        age == 18 (file20320@198: tip@180 over mid 194).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4MID_TO_M2TIP_INTO
            and (pos - chosen) == B2_M4MID_TO_M2TIP_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4MID_TO_M2TIP_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf17. best==3: just-finished M4, lit tip → mid into +1 of M7 age
    #        in [16, 24) (file20328@400: mid 380 over L@364).
    if best_len == 3:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            if any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            ):
                mids = [
                    ref
                    for ref in cands
                    if token_starts.get(ref) is None
                    and token_starts.get(owners.get(ref, ref)) == ("M", 7)
                    and ref - owners.get(ref, ref) == B3_LIT_TO_M7INTO1
                    and B3_LIT_TO_M7INTO1_MID_MIN_AGE
                    <= (pos - ref)
                    < B3_LIT_TO_M7INTO1_MID_MAX_AGE
                ]
                if mids:
                    chosen = max(mids)

    # 8cf18. best==2, 2-cand: M6 mid into +4 age == 2 → M2 mid into +1 age == 12
    #        (file20320@246: mid 234 over mid 244 of M6@240).
    if best_len == 2 and len(cands) == B2_M6INTO4_TO_M2INTO1_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == B2_M6INTO4_TO_M2INTO1_SRC_INTO
            and (pos - chosen) == B2_M6INTO4_TO_M2INTO1_SRC_AGE
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 2)
                and ref - owners.get(ref, ref) == B2_M6INTO4_TO_M2INTO1_DST_INTO
                and (pos - ref) == B2_M6INTO4_TO_M2INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf19. best==2, 2-cand: just-finished L, M4 mid into +2 → lit age == 160
    #        (file444000@752: L@592 over mid 696).
    if best_len == 2 and len(cands) == B2_M4MID_TO_OLDLIT_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4MID_TO_OLDLIT_INTO
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M4MID_TO_OLDLIT_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf20. best==2: M3 mid into +2 → just-finished M2 tip age == 2
    #        (file20328@418: tip@416 over mid 402).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO2_TO_JFTIP_INTO
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M3INTO2_TO_JFTIP_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf21. best==2, 2-cand: just-finished L, M4 mid into +1 age == 8 → lit
    #        age == 20 (file20320@322: L@302 over mid 314).
    if best_len == 2 and len(cands) == B2_M4INTO1_TO_LIT20_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO1_TO_LIT20_INTO
            and (pos - chosen) == B2_M4INTO1_TO_LIT20_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M4INTO1_TO_LIT20_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf22. best==2: just-finished M4, M2 tip age == 68 → lit age == 72
    #        (file444000@770: L@698 over tip@702).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP68_TO_LIT72_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_TIP68_TO_LIT72_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf23. best==4: just-finished M4 mid into +2 age == 2 → M6 mid into +2
    #        age == 12 (file20328@434: mid 422 over mid 432).
    if best_len == 4:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B4_M4INTO2_TO_M6INTO2_SRC_INTO
            and (pos - chosen) == B4_M4INTO2_TO_M6INTO2_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 6)
                and ref - owners.get(ref, ref) == B4_M4INTO2_TO_M6INTO2_DST_INTO
                and (pos - ref) == B4_M4INTO2_TO_M6INTO2_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf24. best==2, 2-cand: just-finished M3, M2 mid into +1 → lit age == 24
    #        (file20320@334: L@310 over mid 320).
    if best_len == 2 and len(cands) == B2_M2INTO1_TO_LIT24_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == B2_M2INTO1_TO_LIT24_INTO
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2INTO1_TO_LIT24_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf25. best==2, 5-cand: just-finished L, M3 mid into +1 → M4 mid into +2
    #        age == 196 (file444000@778: mid 582 over mid 642).
    if best_len == 2 and len(cands) == B2_M3INTO1_TO_M4INTO2_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_M4INTO2_SRC_INTO
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 4)
                and ref - owners.get(ref, ref) == B2_M3INTO1_TO_M4INTO2_DST_INTO
                and (pos - ref) == B2_M3INTO1_TO_M4INTO2_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf26. best==2, 2-cand: just-finished M4, M3 mid into +2 → lit age == 66
    #        (file20328@438: L@372 over mid 388).
    if best_len == 2 and len(cands) == B2_M3INTO2_TO_LIT66_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO2_TO_LIT66_INTO
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M3INTO2_TO_LIT66_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf27. best==2: just-finished M3, M3 mid into +2 age == 4 → lit age == 26
    #        (file20320@392: L@366 over mid 388).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_JFM3_INTO2_TO_LIT26_INTO
            and (pos - chosen) == B2_JFM3_INTO2_TO_LIT26_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_JFM3_INTO2_TO_LIT26_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf28. best==2: just-finished M2, M3 tip age == 66 → lit age == 142
    #        (file444000@780: L@638 over tip@714).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP66_TO_LIT142_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M3TIP66_TO_LIT142_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf29. best==3: just-finished L, M4 tip age == 8 → mid into +3 of M4
    #        age == 10 (file20320@419: mid 409 of M4@406 over tip@411).
    if best_len == 3:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B3_M4TIP_TO_INTO3_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 4)
                and ref - owners.get(ref, ref) == B3_M4TIP_TO_INTO3_INTO
                and (pos - ref) == B3_M4TIP_TO_INTO3_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf30. best==6: just-finished M3, M3 mid into +1 → M4 mid into +2 age == 10
    #        (file444000@794: mid 784 over mid 642).
    if best_len == 6:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B6_M3INTO1_TO_M4INTO2_SRC_INTO
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 4)
                and ref - owners.get(ref, ref) == B6_M3INTO1_TO_M4INTO2_DST_INTO
                and (pos - ref) == B6_M3INTO1_TO_M4INTO2_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf31. best==2: just-finished M2, M3 mid into +1 age == 6 → M4 mid into +1
    #        age == 14 (file20320@426: mid 412 over mid 420).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_M4INTO1_SRC_INTO
            and (pos - chosen) == B2_M3INTO1_TO_M4INTO1_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 4)
                and ref - owners.get(ref, ref) == B2_M3INTO1_TO_M4INTO1_DST_INTO
                and (pos - ref) == B2_M3INTO1_TO_M4INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf32. best==4: just-finished M4, M4 tip → M5 mid into +4 age == 138
    #        (file444000@806: mid 668 over tip@678).
    if best_len == 4:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 5)
                and ref - owners.get(ref, ref) == B4_M4TIP_TO_M5INTO4_DST_INTO
                and (pos - ref) == B4_M4TIP_TO_M5INTO4_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf33. best==4: just-finished M8 mid into +4 age == 4 → M6 mid into +4
    #        age == 12 (file20320@482: mid 470 over mid 478).
    if best_len == 4:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == B4_M8INTO4_TO_M6INTO4_SRC_INTO
            and (pos - chosen) == B4_M8INTO4_TO_M6INTO4_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 6)
                and ref - owners.get(ref, ref) == B4_M8INTO4_TO_M6INTO4_DST_INTO
                and (pos - ref) == B4_M8INTO4_TO_M6INTO4_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf34. best==2, 2-cand: just-finished L, M2 tip age == 6 → lit age == 10
    #        when lit_run == 4 and data[pos-1] ∈ [44, 56)
    #        (file20306@456: L@446 over tip@450).
    if (
        best_len == 2
        and len(cands) == B2_TIP6_TO_LIT10_NC
        and data is not None
        and 0 < pos <= len(data)
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP6_TO_LIT10_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lit_run = 0
            j = pos - 1
            while j >= 0 and token_starts.get(j) == ("L",):
                lit_run += 1
                j -= 1
            pre = data[pos - 1]
            if (
                lit_run == B2_TIP6_TO_LIT10_LIT_RUN
                and B2_TIP6_TO_LIT10_PRE_MIN
                <= pre
                < B2_TIP6_TO_LIT10_PRE_MAX
            ):
                lits = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "L"
                    and (pos - c) == B2_TIP6_TO_LIT10_LIT_AGE
                ]
                if lits:
                    chosen = max(lits)

    # 8cf35. best==2, 4-cand: just-finished L, M3 mid into +2 age == 6 → M2
    #        mid into +1 age == 18 (file20342@284: mid 266 over mid 278).
    if best_len == 2 and len(cands) == B2_M3INTO2_TO_M2INTO1_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO2_TO_M2INTO1_SRC_INTO
            and (pos - chosen) == B2_M3INTO2_TO_M2INTO1_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 2)
                and ref - owners.get(ref, ref) == B2_M3INTO2_TO_M2INTO1_DST_INTO
                and (pos - ref) == B2_M3INTO2_TO_M2INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf36. best==2, 2-cand: just-finished L, M3 mid into +1 → lit age == 84
    #        (file403100@720: L@636 over mid 680).
    if best_len == 2 and len(cands) == B2_M3INTO1_TO_LIT84_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_LIT84_INTO
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M3INTO1_TO_LIT84_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf37. best==2, 2-cand: just-finished L, M2 tip age == 12 → lit age == 26
    #        (file20407@330: L@304 over tip@318).
    if best_len == 2 and len(cands) == B2_TIP12_TO_LIT26_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP12_TO_LIT26_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_TIP12_TO_LIT26_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf38. best==2, 2-cand: lit tip → M2 tip age == 166
    #        (file201500@320: tip@154 over L@104).
    if best_len == 2 and len(cands) == B2_LIT_TO_M2TIP166_NC:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_LIT_TO_M2TIP166_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf39. best==2: just-finished M4, M4 tip age == 4 → lit age == 38
    #        (file20342@291: L@253 over tip@287).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP4_TO_LIT38_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M4TIP4_TO_LIT38_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf40. best==2: just-finished M4, M3 mid into +1 age ≥ 96 → M2 tip
    #        age in [64, 80) (file403100@792: tip@720 over mid 680).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_M2TIP72_SRC_INTO
            and (pos - chosen) >= B2_M3INTO1_TO_M2TIP72_SRC_MIN_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and B2_M3INTO1_TO_M2TIP72_TIP_MIN_AGE
                <= (pos - c)
                < B2_M3INTO1_TO_M2TIP72_TIP_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf41. best==2, 2-cand: just-finished M2, M2 tip age == 12 → lit age == 18
    #        (file20407@430: L@412 over tip@418).
    if best_len == 2 and len(cands) == B2_TIP12_TO_LIT18_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP12_TO_LIT18_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_TIP12_TO_LIT18_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf42. best==2: just-finished M2, M2 tip age == 10 → lit age ≥ 200
    #        (file201500@368: L@130 over tip@358).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP10_TO_OLDLIT_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) >= B2_TIP10_TO_OLDLIT_LIT_MIN_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf43. best==2, 2-cand: just-finished L, M3 tip age == 4 → lit age == 6
    #        when pos ≥ 300 and data[pos-1] ≠ 12 (file20342@316: L@310).
    if (
        best_len == 2
        and len(cands) == B2_M3TIP4_TO_LIT6_NC
        and pos >= B2_M3TIP4_TO_LIT6_MIN_POS
        and data is not None
        and 0 < pos <= len(data)
        and data[pos - 1] != B2_M3TIP4_TO_LIT6_FORBIDDEN_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP4_TO_LIT6_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M3TIP4_TO_LIT6_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf44. best==2, 2-cand: M2 tip age == 88 → older M2 tip age == 98
    #        (file201500@448: tip@350 over tip@360).
    if best_len == 2 and len(cands) == B2_TIP88_TO_TIP98_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP88_TO_TIP98_SRC_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_TIP88_TO_TIP98_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf45. best==5: just-finished M2, M8 mid into +1 age == 12 → M3 mid into +1
    #        age == 20 (file20342@333: mid 313 over mid 321).
    if best_len == 5:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == B5_M8INTO1_TO_M3INTO1_SRC_INTO
            and (pos - chosen) == B5_M8INTO1_TO_M3INTO1_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 3)
                and ref - owners.get(ref, ref) == B5_M8INTO1_TO_M3INTO1_DST_INTO
                and (pos - ref) == B5_M8INTO1_TO_M3INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf46. best==2, 2-cand: just-finished L, M2 tip age == 86 → older M2 tip
    #        age == 96 (file201500@454: tip@358 over tip@368).
    if best_len == 2 and len(cands) == B2_TIP86_TO_TIP96_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP86_TO_TIP96_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_TIP86_TO_TIP96_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf47. best==2, 2-cand: M2 tip age == 2 → lit age == 6 under pre/cur byte
    #        gates (file20415@420 pre66; file202000@108 pre0 cur197).
    if (
        best_len == 2
        and len(cands) == B2_TIP2_TO_LIT6_NC
        and data is not None
        and 0 < pos < len(data)
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_TIP2_TO_LIT6_TIP_AGE
        ):
            pre = data[pos - 1]
            cur = data[pos]
            gate_a = (
                B2_TIP2_TO_LIT6_PRE_A_MIN
                <= pre
                < B2_TIP2_TO_LIT6_PRE_A_MAX
            )
            gate_b = (
                pre == B2_TIP2_TO_LIT6_PRE_B
                and cur >= B2_TIP2_TO_LIT6_CUR_B_MIN
            )
            if gate_a or gate_b:
                lits = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "L"
                    and (pos - c) == B2_TIP2_TO_LIT6_LIT_AGE
                ]
                if lits:
                    chosen = max(lits)

    # 8cf48. best==3: just-finished M4, lit tip → mid into +2 of M5 age == 8
    #        (file20342@343: mid 335 over L@315).
    if best_len == 3:
        tip = token_starts.get(chosen)
        if tip is not None and tip[0] == "L":
            if any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            ):
                mids = [
                    ref
                    for ref in cands
                    if token_starts.get(ref) is None
                    and token_starts.get(owners.get(ref, ref)) == ("M", 5)
                    and ref - owners.get(ref, ref) == B3_LIT_TO_M5INTO2_INTO
                    and (pos - ref) == B3_LIT_TO_M5INTO2_MID_AGE
                ]
                if mids:
                    chosen = max(mids)

    # 8cf49. best==2: just-finished M2, M4 mid into +2 age == 6 → M2 tip age == 112
    #        (file201500@480: tip@368 over mid 474).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO2_TO_M2TIP112_INTO
            and (pos - chosen) == B2_M4INTO2_TO_M2TIP112_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4INTO2_TO_M2TIP112_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf50. best==2: just-finished M3, M4 mid into +3 age == 4 → M5 mid into +1
    #        age == 12 (file20342@346: mid 334 over mid 342).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO3_TO_M5INTO1_SRC_INTO
            and (pos - chosen) == B2_M4INTO3_TO_M5INTO1_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 5)
                and ref - owners.get(ref, ref) == B2_M4INTO3_TO_M5INTO1_DST_INTO
                and (pos - ref) == B2_M4INTO3_TO_M5INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf51. best==2: just-finished M2, M4 mid into +2 age == 12 → M2 tip age == 24
    #        (file201500@482: tip@458 over mid 470).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO2_TO_M2TIP24_INTO
            and (pos - chosen) == B2_M4INTO2_TO_M2TIP24_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4INTO2_TO_M2TIP24_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf52. best==2: just-finished M2, M3 tip age == 12 → M8 mid into +3 age == 32
    #        (file20342@355: mid 323 over tip@343).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP_TO_M8INTO3_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) == ("M", 8)
                and ref - owners.get(ref, ref) == B2_M3TIP_TO_M8INTO3_INTO
                and (pos - ref) == B2_M3TIP_TO_M8INTO3_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf53. best==2: just-finished M2, M4 tip age == 14 → M2 tip age == 158
    #        (file201500@486: tip@328 over tip@472).
    if best_len == 2:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP_TO_M2TIP158_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4TIP_TO_M2TIP158_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf54. best==2, 2-cand: just-finished M3, M5 tip age == 12 → lit age == 22
    #        (file20342@360: L@338 over tip@348).
    if best_len == 2 and len(cands) == B2_M5TIP_TO_LIT22_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 5
            and (pos - chosen) == B2_M5TIP_TO_LIT22_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M5TIP_TO_LIT22_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf55. best==2: just-finished M6, M9 mid into +2 age == 16 → M5 tip age == 40
    #        (file20342@409: tip@369 over mid 393).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 9
            and chosen - o == B2_M9INTO2_TO_M5TIP_SRC_INTO
            and (pos - chosen) == B2_M9INTO2_TO_M5TIP_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 6
                and own + 6 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == B2_M9INTO2_TO_M5TIP_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf56. best==2, 2-cand: just-finished M2, M3 mid into +1 age == 4 → lit age == 28
    #        (file20342@447: L@419 over mid 443 of M3@442).
    if best_len == 2 and len(cands) == B2_M3INTO1_TO_LIT28_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_LIT28_INTO
            and (pos - chosen) == B2_M3INTO1_TO_LIT28_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M3INTO1_TO_LIT28_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf57. best==3, 2-cand: just-finished L, lit tip age == 16 → M3 tip age == 10
    #        (file20314@208: tip@198 over L@192).
    if best_len == 3 and len(cands) == B3_LIT16_TO_M3TIP10_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B3_LIT16_TO_M3TIP10_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == B3_LIT16_TO_M3TIP10_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf58. best==2, 3-cand: just-finished M2, M2 tip age == 2 → M2 tip age == 70
    #        (file102000/1@250: tip@180 over tip@248).
    if best_len == 2 and len(cands) == B2_M2TIP2_TO_M2TIP70_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP2_TO_M2TIP70_YOUNG_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            old = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP2_TO_M2TIP70_OLD_AGE
            ]
            if old:
                chosen = max(old)

    # 8cf59. best==2, 2-cand: just-finished L, M2 tip age == 4 → lit age == 8
    #        when M2 tip is preceded by exactly 4 lits after an M3
    #        (file20333@94 family: L@86 over tip@90).
    if best_len == 2 and len(cands) == B2_M2TIP4_TO_LIT8_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP4_TO_LIT8_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lit_before = 0
            q = chosen - 1
            while (
                q >= 0
                and token_starts.get(q) is not None
                and token_starts[q][0] == "L"
            ):
                lit_before += 1
                q -= 1
            prior = (
                token_starts.get(owners.get(q, q)) if q >= 0 else None
            )
            if (
                lit_before == B2_M2TIP4_TO_LIT8_PRE_LITS
                and prior is not None
                and prior[0] == "M"
                and prior[1] == B2_M2TIP4_TO_LIT8_PRIOR_PL
            ):
                lits = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "L"
                    and (pos - c) == B2_M2TIP4_TO_LIT8_LIT_AGE
                ]
                if lits:
                    chosen = max(lits)

    # 8cf60. best==4, 2-cand: just-finished L trail==2, M4 tip age == 8 → M2 tip
    #        age == 10 (file20408@254: tip@244 over tip@246).
    if best_len == 4 and len(cands) == B4_M4TIP8_TO_M2TIP10_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B4_M4TIP8_TO_M2TIP10_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            trail = 0
            q = pos - 1
            while (
                q >= 0
                and token_starts.get(q) is not None
                and token_starts[q][0] == "L"
            ):
                trail += 1
                q -= 1
            if trail == B4_M4TIP8_TO_M2TIP10_TRAIL:
                tips = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "M"
                    and token_starts[c][1] == 2
                    and (pos - c) == B4_M4TIP8_TO_M2TIP10_DST_AGE
                ]
                if tips:
                    chosen = max(tips)

    # 8cf61. best==2, 2-cand: just-finished M2, M2 tip age == 2 → lit age == 86
    #        when young tip is preceded by exactly 4 lits
    #        (file102000/1@274: L@188 over tip@272).
    if best_len == 2 and len(cands) == B2_M2TIP2_TO_LIT86_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP2_TO_LIT86_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lit_before = 0
            q = chosen - 1
            while (
                q >= 0
                and token_starts.get(q) is not None
                and token_starts[q][0] == "L"
            ):
                lit_before += 1
                q -= 1
            if lit_before == B2_M2TIP2_TO_LIT86_PRE_LITS:
                lits = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "L"
                    and (pos - c) == B2_M2TIP2_TO_LIT86_LIT_AGE
                ]
                if lits:
                    chosen = max(lits)

    # 8cf62. best==2, 3-cand: just-finished M3, M2 tip age == 16 → lit age == 34
    #        (file20314@239: L@205 over tip@223).
    if best_len == 2 and len(cands) == B2_M2TIP16_TO_LIT34_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP16_TO_LIT34_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP16_TO_LIT34_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf63. best==3, 2-cand: just-finished M5, lit tip → M6 mid into +5 age == 8
    #        (file102000/1@355: mid 347 of M6@342 over L@237).
    if best_len == 3 and len(cands) == B3_LIT_TO_M6INTO5_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and any(
                info[0] == "M"
                and info[1] == 5
                and own + 5 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == B3_LIT_TO_M6INTO5_INTO
                and (pos - c) == B3_LIT_TO_M6INTO5_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf64. best==2, 2-cand: just-finished M3, M7 mid into +2 age == 12 → lit age == 22
    #        (file20333@272: L@250 over mid 260 of M7@258).
    if best_len == 2 and len(cands) == B2_M7INTO2_TO_LIT22_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == B2_M7INTO2_TO_LIT22_INTO
            and (pos - chosen) == B2_M7INTO2_TO_LIT22_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M7INTO2_TO_LIT22_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf65. best==2, 3-cand: just-finished L, M3 tip age == 4 → M2 tip age == 6
    #        (file20408@326: tip@320 over tip@322).
    if best_len == 2 and len(cands) == B2_M3TIP4_TO_M2TIP6_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP4_TO_M2TIP6_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M3TIP4_TO_M2TIP6_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf66. best==2, 4-cand: just-finished M3, M4 mid into +2 age == 30 → M2 tip age == 34
    #        (file20314@249: tip@215 over mid 219 of M4@217).
    if best_len == 2 and len(cands) == B2_M4INTO2_TO_M2TIP34_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO2_TO_M2TIP34_INTO
            and (pos - chosen) == B2_M4INTO2_TO_M2TIP34_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4INTO2_TO_M2TIP34_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf67. best==2, 2-cand: just-finished M8, M2 tip age == 128 → lit age == 130
    #        (file102000/1@370: L@240 over tip@242).
    if best_len == 2 and len(cands) == B2_M2TIP128_TO_LIT130_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP128_TO_LIT130_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP128_TO_LIT130_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf68. best==2, 3-cand: just-finished L, M2 tip age == 14 → M7 mid into +2
    #        age == 26 (file20333@286: mid 260 of M7@258 over tip@272).
    if best_len == 2 and len(cands) == B2_M2TIP14_TO_M7INTO2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP14_TO_M7INTO2_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == B2_M2TIP14_TO_M7INTO2_INTO
                and (pos - c) == B2_M2TIP14_TO_M7INTO2_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf69. best==2, 2-cand: just-finished M2, M2 tip age == 4 → lit age == 30
    #        when young tip is preceded by exactly 2 lits
    #        (file20314@292: L@262 over tip@288).
    if best_len == 2 and len(cands) == B2_M2TIP4_TO_LIT30_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP4_TO_LIT30_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lit_before = 0
            q = chosen - 1
            while (
                q >= 0
                and token_starts.get(q) is not None
                and token_starts[q][0] == "L"
            ):
                lit_before += 1
                q -= 1
            if lit_before == B2_M2TIP4_TO_LIT30_PRE_LITS:
                lits = [
                    c
                    for c in cands
                    if token_starts.get(c) is not None
                    and token_starts[c][0] == "L"
                    and (pos - c) == B2_M2TIP4_TO_LIT30_LIT_AGE
                ]
                if lits:
                    chosen = max(lits)

    # 8cf70. best==2, 3-cand: just-finished L trail==2, M4 tip age == 6 → M4 mid
    #        into +2 age == 4 when M4 tip is preceded by 2 or 4 lits
    #        (file102000/1@412; file102300@8). lit_before==5 keeps tip.
    if best_len == 2 and len(cands) == B2_M4TIP6_TO_M4INTO2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP6_TO_M4INTO2_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            trail = 0
            q = pos - 1
            while (
                q >= 0
                and token_starts.get(q) is not None
                and token_starts[q][0] == "L"
            ):
                trail += 1
                q -= 1
            lit_before = 0
            q = chosen - 1
            while (
                q >= 0
                and token_starts.get(q) is not None
                and token_starts[q][0] == "L"
            ):
                lit_before += 1
                q -= 1
            if trail == B2_M4TIP6_TO_M4INTO2_TRAIL and lit_before in (
                B2_M4TIP6_TO_M4INTO2_PRE_LITS_A,
                B2_M4TIP6_TO_M4INTO2_PRE_LITS_B,
            ):
                mids = [
                    c
                    for c in cands
                    if token_starts.get(c) is None
                    and owners.get(c, c) == chosen
                    and c - chosen == B2_M4TIP6_TO_M4INTO2_INTO
                    and (pos - c) == B2_M4TIP6_TO_M4INTO2_MID_AGE
                ]
                if mids:
                    chosen = max(mids)

    # 8cf71. best==2, 4-cand: just-finished M3, M3 PE into +1 age == 2 → M2 mid
    #        into +1 age == 14 (file20314@361: mid 347 over pe 359).
    if best_len == 2 and len(cands) == B2_M3PE_TO_M2INTO1_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3PE_TO_M2INTO1_SRC_INTO
            and (pos - chosen) == B2_M3PE_TO_M2INTO1_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B2_M3PE_TO_M2INTO1_DST_INTO
                and (pos - c) == B2_M3PE_TO_M2INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf72. best==2, 3-cand: just-finished M2, M2 tip age == 184 → M2 tip age == 186
    #        (file102000/1@434: tip@248 over tip@250).
    if best_len == 2 and len(cands) == B2_M2TIP184_TO_M2TIP186_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP184_TO_M2TIP186_YOUNG_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            older = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP184_TO_M2TIP186_OLD_AGE
            ]
            if older:
                chosen = max(older)

    # 8cf73. best==3, 2-cand: just-finished M2, lit tip → M4 mid into +3 age == 8
    #        (file20314@365: mid 357 of M4@354 over L@335).
    if best_len == 3 and len(cands) == B3_LIT_TO_M4INTO3_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B3_LIT_TO_M4INTO3_INTO
                and (pos - c) == B3_LIT_TO_M4INTO3_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf74. best==2, 2-cand: just-finished L, M4 mid into +1 age == 20 → M2 mid
    #        into +1 age == 36 (file20333@333: mid 297 over mid 313).
    if best_len == 2 and len(cands) == B2_M4INTO1_TO_M2INTO1_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO1_TO_M2INTO1_SRC_INTO
            and (pos - chosen) == B2_M4INTO1_TO_M2INTO1_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B2_M4INTO1_TO_M2INTO1_DST_INTO
                and (pos - c) == B2_M4INTO1_TO_M2INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf75. best==2, 4-cand: just-finished M5, M2 tip age == 242 → M2 tip age == 150
    #        (file102000/1@448: tip@298 over tip@206).
    if best_len == 2 and len(cands) == B2_M2TIP242_TO_M2TIP150_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP242_TO_M2TIP150_OLD_AGE
            and any(
                info[0] == "M"
                and info[1] == 5
                and own + 5 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP242_TO_M2TIP150_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf76. best==2, 4-cand: just-finished L, M7 mid into +5 age == 4 → M2 tip
    #        age == 12 (file20314@391: tip@379 over mid 387 of M7@382).
    if best_len == 2 and len(cands) == B2_M7INTO5_TO_M2TIP12_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == B2_M7INTO5_TO_M2TIP12_INTO
            and (pos - chosen) == B2_M7INTO5_TO_M2TIP12_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M7INTO5_TO_M2TIP12_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf77. best==2, 3-cand: just-finished L, M5 mid into +2 age == 18 → M3 tip
    #        age == 28 (file20333@336: tip@308 over mid 318 of M5@316).
    if best_len == 2 and len(cands) == B2_M5INTO2_TO_M3TIP28_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO2_TO_M3TIP28_INTO
            and (pos - chosen) == B2_M5INTO2_TO_M3TIP28_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == B2_M5INTO2_TO_M3TIP28_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf78. best==2, 3-cand: just-finished M3, M5 mid into +2 age == 10 → M3 PE
    #        into +1 age == 2 (file20314@456: pe 454 over mid 446).
    if best_len == 2 and len(cands) == B2_M5INTO2_TO_M3PE_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO2_TO_M3PE_SRC_INTO
            and (pos - chosen) == B2_M5INTO2_TO_M3PE_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            pes = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B2_M5INTO2_TO_M3PE_DST_INTO
                and (pos - c) == B2_M5INTO2_TO_M3PE_DST_AGE
            ]
            if pes:
                chosen = max(pes)

    # 8cf79. best==2, 2-cand: just-finished M2, M4 mid into +1 age == 22 → lit
    #        age == 34 (file20333@385: L@351 over mid 363 of M4@362).
    if best_len == 2 and len(cands) == B2_M4INTO1_TO_LIT34_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO1_TO_LIT34_INTO
            and (pos - chosen) == B2_M4INTO1_TO_LIT34_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M4INTO1_TO_LIT34_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf80. best==2, 3-cand: just-finished M5, M5 mid into +2 age == 8 → M2 tip
    #        age == 12 (file20314@481: tip@469 over mid 473 of M5@471).
    if best_len == 2 and len(cands) == B2_M5INTO2_TO_M2TIP12_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO2_TO_M2TIP12_INTO
            and (pos - chosen) == B2_M5INTO2_TO_M2TIP12_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 5
                and own + 5 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M5INTO2_TO_M2TIP12_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf81. best==3, 2-cand: just-finished M4, M4 mid into +2 age == 2 → M2 tip
    #        age == 46 (file20333@414: tip@368 over mid 412 of M4@410).
    if best_len == 3 and len(cands) == B3_M4INTO2_TO_M2TIP46_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B3_M4INTO2_TO_M2TIP46_INTO
            and (pos - chosen) == B3_M4INTO2_TO_M2TIP46_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B3_M4INTO2_TO_M2TIP46_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf82. best==2, 5-cand: just-finished L, M5 mid into +2 age == 6 → M5 mid
    #        into +3 age == 10 (file20314@484: mid 474 over mid 478).
    if best_len == 2 and len(cands) == B2_M5INTO2_TO_M5INTO3_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO2_TO_M5INTO3_SRC_INTO
            and (pos - chosen) == B2_M5INTO2_TO_M5INTO3_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == B2_M5INTO2_TO_M5INTO3_DST_INTO
                and (pos - c) == B2_M5INTO2_TO_M5INTO3_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf83. best==2, 7-cand: just-finished M3, M3 mid into +2 age == 1 → M7 tip
    #        age == 16 (file20333@482: tip@466 over mid 481 of M3@479).
    if best_len == 2 and len(cands) == B2_M3INTO2_TO_M7TIP16_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO2_TO_M7TIP16_INTO
            and (pos - chosen) == B2_M3INTO2_TO_M7TIP16_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 7
                and (pos - c) == B2_M3INTO2_TO_M7TIP16_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf84. best==3, 2-cand: just-finished M4, M3 mid into +2 age == 10 → M3 mid
    #        into +1 age == 32 (file20333@488: mid 456 over mid 478).
    if best_len == 3 and len(cands) == B3_M3INTO2_TO_M3INTO1_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B3_M3INTO2_TO_M3INTO1_SRC_INTO
            and (pos - chosen) == B3_M3INTO2_TO_M3INTO1_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B3_M3INTO2_TO_M3INTO1_DST_INTO
                and (pos - c) == B3_M3INTO2_TO_M3INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf85. best==6: just-finished M7, M7 mid into +1 age == 6 → M5 mid into +1
    #        age == 14 (file20333@506: mid 492 of M5@491 over mid 500 of M7@499).
    if best_len == 6:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == B6_M7INTO1_TO_M5INTO1_INTO
            and (pos - chosen) == B6_M7INTO1_TO_M5INTO1_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 7
                and own + 7 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == B6_M7INTO1_TO_M5INTO1_INTO
                and (pos - c) == B6_M7INTO1_TO_M5INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf86. best==2, 5-cand: just-finished M2, M2 tip age == 74 → M4 mid into +2
    #        age == 76 (file403100/2@866: mid 790 over tip@792).
    if best_len == 2 and len(cands) == B2_M2TIP74_TO_M4INTO2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP74_TO_M4INTO2_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B2_M2TIP74_TO_M4INTO2_INTO
                and (pos - c) == B2_M2TIP74_TO_M4INTO2_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf87. best==2, 2-cand: just-finished M5, M5 mid into +3 age == 2 → lit
    #        age == 6 when pos < 100 and data[pos-1] == 0
    #        (file102000/0@76 family).
    if (
        best_len == 2
        and len(cands) == B2_M5INTO3_TO_LIT6_NC
        and pos < B2_M5INTO3_TO_LIT6_MAX_POS
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M5INTO3_TO_LIT6_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO3_TO_LIT6_INTO
            and (pos - chosen) == B2_M5INTO3_TO_LIT6_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 5
                and own + 5 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M5INTO3_TO_LIT6_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf88. best==2, 2-cand: just-finished L, M2 tip age == 51 → lit age == 179
    #        (file20213@244 family: L@65 over tip@193).
    if best_len == 2 and len(cands) == B2_M2TIP51_TO_LIT179_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP51_TO_LIT179_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP51_TO_LIT179_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf89. best==2, 2-cand: just-finished M3, M2 tip age == 74 → lit age == 75
    #        (file444000/4@834: L@759 over tip@760).
    if best_len == 2 and len(cands) == B2_M2TIP74_TO_LIT75_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP74_TO_LIT75_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP74_TO_LIT75_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf90. best==2, 3-cand: just-finished L, M3 tip age == 30 → M2 mid into +1
    #        age == 50 (file403100/2@890: mid 840 over tip@860).
    if best_len == 2 and len(cands) == B2_M3TIP30_TO_M2INTO1_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP30_TO_M2INTO1_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B2_M3TIP30_TO_M2INTO1_INTO
                and (pos - c) == B2_M3TIP30_TO_M2INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf91. best==2, 5-cand: just-finished M4, M5 mid into +3 age == 8 → M4 mid
    #        into +2 age == 2 when pos < 100 and data[pos-1] == 0
    #        (file102000/0@84 family).
    if (
        best_len == 2
        and len(cands) == B2_M5INTO3_TO_M4INTO2_NC
        and pos < B2_M5INTO3_TO_M4INTO2_MAX_POS
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M5INTO3_TO_M4INTO2_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        src_ok = (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO3_TO_M4INTO2_SRC_INTO
            and (pos - chosen) == B2_M5INTO3_TO_M4INTO2_SRC_AGE
        ) or (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M5INTO3_TO_M4INTO2_SRC_AGE
        )
        if src_ok and any(
            info[0] == "M"
            and info[1] == 4
            and own + 4 == pos
            for own, info in token_starts.items()
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B2_M5INTO3_TO_M4INTO2_DST_INTO
                and (pos - c) == B2_M5INTO3_TO_M4INTO2_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf92. best==2, 3-cand: just-finished M2, M3 mid into +1 age == 4 → M2 tip
    #        age == 204 (file444000/4@836: tip@632 over mid 832 of M3@831).
    if best_len == 2 and len(cands) == B2_M3INTO1_TO_M2TIP204_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_M2TIP204_INTO
            and (pos - chosen) == B2_M3INTO1_TO_M2TIP204_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M3INTO1_TO_M2TIP204_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf93. best==2, 4-cand: just-finished M4, M4 mid into +2 age == 2 → M4 mid
    #        into +2 age == 56 (file403100/2@900: mid 844 over pe 898).
    if best_len == 2 and len(cands) == B2_M4INTO2_TO_M4INTO2_56_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO2_TO_M4INTO2_56_INTO
            and (pos - chosen) == B2_M4INTO2_TO_M4INTO2_56_YOUNG_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B2_M4INTO2_TO_M4INTO2_56_INTO
                and (pos - c) == B2_M4INTO2_TO_M4INTO2_56_OLD_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf94. best==3, 6-cand: just-finished L, M4 mid into +3 age == 4 → M5 mid
    #        into +4 age == 12 (file102000/0@87 family: mid 75 over mid 83).
    if best_len == 3 and len(cands) == B3_M4INTO3_TO_M5INTO4_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B3_M4INTO3_TO_M5INTO4_SRC_INTO
            and (pos - chosen) == B3_M4INTO3_TO_M5INTO4_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == B3_M4INTO3_TO_M5INTO4_DST_INTO
                and (pos - c) == B3_M4INTO3_TO_M5INTO4_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf95. best==2, 6-cand: just-finished M3, M3 mid into +1 age == 8 → M5 tip
    #        age == 88 (file444000/4@860: tip@772 over mid 852 of M3@851).
    if best_len == 2 and len(cands) == B2_M3INTO1_TO_M5TIP88_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_M5TIP88_INTO
            and (pos - chosen) == B2_M3INTO1_TO_M5TIP88_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == B2_M3INTO1_TO_M5TIP88_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf96. best==2, 5-cand: just-finished L, M4 tip age == 38 → M2 tip age == 40
    #        (file403100/2@930: tip@890 over tip@892).
    if best_len == 2 and len(cands) == B2_M4TIP38_TO_M2TIP40_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP38_TO_M2TIP40_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4TIP38_TO_M2TIP40_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf97. best==3, 7-cand: just-finished L, M3 tip age == 8 → M4 mid into +3
    #        age == 12 (file102000/0@95 family: mid 83 of M4@80 over tip@87).
    if best_len == 3 and len(cands) == B3_M3TIP8_TO_M4INTO3_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B3_M3TIP8_TO_M4INTO3_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B3_M3TIP8_TO_M4INTO3_INTO
                and (pos - c) == B3_M3TIP8_TO_M4INTO3_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf98. best==2, 4-cand: just-finished M2, M2 tip age == 164 → M5 mid into
    #        +2 age == 90 (file444000/4@864: mid 774 of M5@772 over tip@700).
    if best_len == 2 and len(cands) == B2_M2TIP164_TO_M5INTO2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP164_TO_M5INTO2_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == B2_M2TIP164_TO_M5INTO2_INTO
                and (pos - c) == B2_M2TIP164_TO_M5INTO2_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf99. best==5, 2-cand: just-finished L, M2 mid into +1 age == 72 → M3 mid
    #        into +1 age == 8 (file403100/2@963: mid 955 over mid 891).
    if best_len == 5 and len(cands) == B5_M2INTO1_TO_M3INTO1_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == B5_M2INTO1_TO_M3INTO1_SRC_INTO
            and (pos - chosen) == B5_M2INTO1_TO_M3INTO1_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B5_M2INTO1_TO_M3INTO1_DST_INTO
                and (pos - c) == B5_M2INTO1_TO_M3INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf100. best==2, 3-cand: just-finished M2, M4 tip age == 6 → M4 mid into
    #         +2 age == 4 (file102000/0@122 family: mid 118 over tip@116).
    if best_len == 2 and len(cands) == B2_M4TIP6_TO_M4INTO2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP6_TO_M4INTO2_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B2_M4TIP6_TO_M4INTO2_INTO
                and (pos - c) == B2_M4TIP6_TO_M4INTO2_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf101. best==2, 5-cand: just-finished L, M3 tip age == 16 → M2 tip age
    #         == 22 (file444000/2@542: tip@520 over tip@526).
    if best_len == 2 and len(cands) == B2_M3TIP16_TO_M2TIP22_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP16_TO_M2TIP22_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M3TIP16_TO_M2TIP22_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf102. best==2, 5-cand: just-finished M2, M2 tip age == 10 with sibling
    #         M2 tip age == 6 → M2 tip age == 12 (file403100/2@1002).
    if best_len == 2 and len(cands) == B2_M2TIP10_TO_M2TIP12_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP10_TO_M2TIP12_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
            and any(
                token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP10_TO_M2TIP12_SIBLING_AGE
                for c in cands
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP10_TO_M2TIP12_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf103. best==2, 5-cand: just-finished L, M3 tip age == 4 → M2 tip age
    #         == 90 (file444000/4@870: tip@780 over tip@866).
    if best_len == 2 and len(cands) == B2_M3TIP4_TO_M2TIP90_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP4_TO_M2TIP90_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M3TIP4_TO_M2TIP90_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf104. best==3, 4-cand: just-finished L, lit tip age == 88 → M4 tip age
    #         == 58 (file444000/2@665: tip@607 over L@577).
    if best_len == 3 and len(cands) == B3_LIT88_TO_M4TIP58_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B3_LIT88_TO_M4TIP58_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == B3_LIT88_TO_M4TIP58_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf105. best==2, 9-cand: just-finished M4, pre-byte == 0, M3 mid into +1
    #         age == 36 → M3 mid into +1 age == 52 (file102000/0@148 family).
    if (
        best_len == 2
        and len(cands) == B2_M3INTO1_36_TO_52_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M3INTO1_36_TO_52_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_36_TO_52_INTO
            and (pos - chosen) == B2_M3INTO1_36_TO_52_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B2_M3INTO1_36_TO_52_INTO
                and (pos - c) == B2_M3INTO1_36_TO_52_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf106. best==2, 2-cand: just-finished M2, M2 tip age == 94 → lit tip age
    #         == 202 (file403100/2@1004: L@802 over tip@910).
    if best_len == 2 and len(cands) == B2_M2TIP94_TO_LIT202_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP94_TO_LIT202_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP94_TO_LIT202_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf107. best==2, 3-cand: just-finished M2, M2 tip age == 50 → lit tip age
    #         == 125 (file444000/4@884: L@759 over tip@834).
    if best_len == 2 and len(cands) == B2_M2TIP50_TO_LIT125_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP50_TO_LIT125_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP50_TO_LIT125_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf108. best==2, 3-cand: just-finished L, M2 tip age == 30 → M2 tip age
    #         == 76 (file444000/2@830: tip@754 over tip@800).
    if best_len == 2 and len(cands) == B2_M2TIP30_TO_M2TIP76_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP30_TO_M2TIP76_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP30_TO_M2TIP76_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf109. best==2, 6-cand: just-finished M5, pre-byte == 4, M5 mid into +3
    #         age == 2 → M2 tip age == 34 (file102000/0@156 family).
    if (
        best_len == 2
        and len(cands) == B2_M5INTO3_TO_M2TIP34_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M5INTO3_TO_M2TIP34_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO3_TO_M2TIP34_INTO
            and (pos - chosen) == B2_M5INTO3_TO_M2TIP34_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 5
                and own + 5 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M5INTO3_TO_M2TIP34_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf110. best==2, 4-cand: just-finished M2, M2 tip age == 2 → lit tip age
    #         == 136 (file444000/2@832: L@696 over tip@830).
    if best_len == 2 and len(cands) == B2_M2TIP2_TO_LIT136_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP2_TO_LIT136_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP2_TO_LIT136_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf111. best==2, 5-cand: just-finished L, pre-byte == 21, M4 mid into +2
    #         age == 10 → M2 tip age == 46 (file102000/0@186 family).
    if (
        best_len == 2
        and len(cands) == B2_M4INTO2_TO_M2TIP46_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M4INTO2_TO_M2TIP46_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO2_TO_M2TIP46_INTO
            and (pos - chosen) == B2_M4INTO2_TO_M2TIP46_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4INTO2_TO_M2TIP46_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf112. best==2, 13-cand: just-finished M4, M3 mid into +1 age == 248 →
    #         M4 tip age == 112 (file444000/4@898: tip@786 over mid 650).
    if best_len == 2 and len(cands) == B2_M3INTO1_248_TO_M4TIP112_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_248_TO_M4TIP112_INTO
            and (pos - chosen) == B2_M3INTO1_248_TO_M4TIP112_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == B2_M3INTO1_248_TO_M4TIP112_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf113. best==2, 2-cand: just-finished L, M6 mid into +2 age == 30 → lit
    #         tip age == 112 (file444000/4@932: L@820 over mid 902 of M6@900).
    if best_len == 2 and len(cands) == B2_M6INTO2_TO_LIT112_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == B2_M6INTO2_TO_LIT112_INTO
            and (pos - chosen) == B2_M6INTO2_TO_LIT112_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M6INTO2_TO_LIT112_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf114. best==2, 2-cand: just-finished M2, M2 tip age == 36 → lit tip age
    #         == 132 (file444000/2@834: L@702 over tip@798).
    if best_len == 2 and len(cands) == B2_M2TIP36_TO_LIT132_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP36_TO_LIT132_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP36_TO_LIT132_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf115. best==2, 6-cand: just-finished L, pre-byte == 4, M2 tip age == 38
    #         → M3 mid into +1 age == 52 (file102000/0@224 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP38_TO_M3INTO1_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP38_TO_M3INTO1_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP38_TO_M3INTO1_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B2_M2TIP38_TO_M3INTO1_INTO
                and (pos - c) == B2_M2TIP38_TO_M3INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf116. best==2, 3-cand: just-finished M4, M5 mid into +1 age == 22 → M2
    #         tip age == 102 (file444000/4@938: tip@836 over mid 916 of M5@915).
    if best_len == 2 and len(cands) == B2_M5INTO1_TO_M2TIP102_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO1_TO_M2TIP102_INTO
            and (pos - chosen) == B2_M5INTO1_TO_M2TIP102_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M5INTO1_TO_M2TIP102_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf117. best==2, 5-cand: just-finished L, M2 tip age == 34 → M2 tip age
    #         == 36 (file444000/2@866: tip@830 over tip@832).
    if best_len == 2 and len(cands) == B2_M2TIP34_TO_M2TIP36_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP34_TO_M2TIP36_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP34_TO_M2TIP36_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf118. best==2, 5-cand: just-finished L, pre-byte == 165, M5 mid into +4
    #         age == 84 → M5 tip age == 88 (file102000/0@239 family).
    if (
        best_len == 2
        and len(cands) == B2_M5INTO4_TO_M5TIP88_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M5INTO4_TO_M5TIP88_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == B2_M5INTO4_TO_M5TIP88_INTO
            and (pos - chosen) == B2_M5INTO4_TO_M5TIP88_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == B2_M5INTO4_TO_M5TIP88_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf119. best==2, 3-cand: just-finished L, M2 tip age == 18 → M6 mid into
    #         +2 age == 48 (file444000/4@950: mid 902 of M6@900 over tip@932).
    if best_len == 2 and len(cands) == B2_M2TIP18_TO_M6INTO2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP18_TO_M6INTO2_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == B2_M2TIP18_TO_M6INTO2_INTO
                and (pos - c) == B2_M2TIP18_TO_M6INTO2_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf120. best==2, 4-cand: just-finished M4, M4 tip age == 4 → M2 tip age
    #         == 94 (file444000/2@892: tip@798 over tip@888).
    if best_len == 2 and len(cands) == B2_M4TIP4_TO_M2TIP94_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP4_TO_M2TIP94_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4TIP4_TO_M2TIP94_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf121. best==2, 3-cand: just-finished M2, pre-byte == 16, M6 mid into +2
    #         age == 8 → M2 tip age == 16 (file102000/0@430 family).
    if (
        best_len == 2
        and len(cands) == B2_M6INTO2_TO_M2TIP16_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M6INTO2_TO_M2TIP16_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == B2_M6INTO2_TO_M2TIP16_INTO
            and (pos - chosen) == B2_M6INTO2_TO_M2TIP16_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M6INTO2_TO_M2TIP16_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf122. best==3, 2-cand: just-finished L, M5 tip age == 40 → M3 tip age
    #         == 124 (file444000/4@955: tip@831 over tip@915).
    if best_len == 3 and len(cands) == B3_M5TIP40_TO_M3TIP124_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 5
            and (pos - chosen) == B3_M5TIP40_TO_M3TIP124_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == B3_M5TIP40_TO_M3TIP124_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf123. best==3, 5-cand: just-finished L, lit tip age == 204 → M2 mid into
    #         +1 age == 18 (file444000/2@905: mid 887 of M2@886 over L@701).
    if best_len == 3 and len(cands) == B3_LIT204_TO_M2INTO1_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B3_LIT204_TO_M2INTO1_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B3_LIT204_TO_M2INTO1_INTO
                and (pos - c) == B3_LIT204_TO_M2INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf124. best==2, 4-cand: just-finished L, pre-byte == 24, M2 tip age == 20
    #         → lit tip age == 24 (file102000/0@436 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP20_TO_LIT24_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP20_TO_LIT24_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP20_TO_LIT24_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP20_TO_LIT24_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf125. best==5, 2-cand: just-finished L, lit tip age == 148 → M6 mid into
    #         +1 age == 66 (file444000/4@967: mid 901 of M6@900 over L@819).
    if best_len == 5 and len(cands) == B5_LIT148_TO_M6INTO1_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B5_LIT148_TO_M6INTO1_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == B5_LIT148_TO_M6INTO1_INTO
                and (pos - c) == B5_LIT148_TO_M6INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf126. best==2, 3-cand: just-finished L, M3 mid into +1 age == 30 → M2 tip
    #         age == 54 (file444000/2@940: tip@886 over mid 910 of M3@909).
    if best_len == 2 and len(cands) == B2_M3INTO1_TO_M2TIP54_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_M2TIP54_INTO
            and (pos - chosen) == B2_M3INTO1_TO_M2TIP54_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M3INTO1_TO_M2TIP54_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf127. best==2, 3-cand: just-finished L, pre-byte == 115, M4 tip age == 130
    #         → lit tip age == 132 (file102000/0@508 family).
    if (
        best_len == 2
        and len(cands) == B2_M4TIP130_TO_LIT132_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M4TIP130_TO_LIT132_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP130_TO_LIT132_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M4TIP130_TO_LIT132_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf128. best==4, 2-cand: just-finished L, M6 tip age == 74 → lit tip age
    #         == 156 (file444000/4@974: L@818 over tip@900).
    if best_len == 4 and len(cands) == B4_M6TIP74_TO_LIT156_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 6
            and (pos - chosen) == B4_M6TIP74_TO_LIT156_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B4_M6TIP74_TO_LIT156_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf129. best==2, 4-cand: just-finished L, M2 tip age == 6 → M3 mid into +1
    #         age == 36 (file444000/2@946: mid 910 of M3@909 over tip@940).
    if best_len == 2 and len(cands) == B2_M2TIP6_TO_M3INTO1_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP6_TO_M3INTO1_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B2_M2TIP6_TO_M3INTO1_INTO
                and (pos - c) == B2_M2TIP6_TO_M3INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf130. best==2, 2-cand: just-finished M3, pre-byte == 28, M2 tip age == 72
    #         → lit tip age == 160 (file102000/0@516 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP72_TO_LIT160_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP72_TO_LIT160_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP72_TO_LIT160_TIP_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP72_TO_LIT160_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf131. best==2, 5-cand: just-finished M2, M4 tip age == 6 → M4 tip age
    #         == 46 (file444000/4@980: tip@934 over tip@974).
    if best_len == 2 and len(cands) == B2_M4TIP6_TO_M4TIP46_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP6_TO_M4TIP46_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == B2_M4TIP6_TO_M4TIP46_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf132. best==2, 5-cand: just-finished L, M2 tip age == 12 → M2 tip age
    #         == 6 (file444000/2@952: tip@946 over tip@940).
    if best_len == 2 and len(cands) == B2_M2TIP12_TO_M2TIP6_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP12_TO_M2TIP6_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP12_TO_M2TIP6_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf133. best==3, 2-cand: just-finished L, M3 tip age == 24 → M2 mid into
    #         +1 age == 34 (file444000/2@1005: mid 971 of M2@970 over tip@981).
    if best_len == 3 and len(cands) == B3_M3TIP24_TO_M2INTO1_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B3_M3TIP24_TO_M2INTO1_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B3_M3TIP24_TO_M2INTO1_INTO
                and (pos - c) == B3_M3TIP24_TO_M2INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf134. best==2, 6-cand: just-finished M5, M2 tip age == 8 → M2 tip age
    #         == 80 (file444000/4@988: tip@908 over tip@980).
    if best_len == 2 and len(cands) == B2_M2TIP8_TO_M2TIP80_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP8_TO_M2TIP80_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 5
                and own + 5 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP8_TO_M2TIP80_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf135. best==2, 3-cand: just-finished M2, pre-byte == 0, lit tip age == 8
    #         → M2 mid into +1 age == 1 (file444000/2@1022: mid 1021 over L@1014).
    if (
        best_len == 2
        and len(cands) == B2_LIT8_TO_M2INTO1_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_LIT8_TO_M2INTO1_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B2_LIT8_TO_M2INTO1_LIT_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B2_LIT8_TO_M2INTO1_INTO
                and (pos - c) == B2_LIT8_TO_M2INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf136. best==2, 7-cand: just-finished M2, M2 tip age == 10 → M2 tip age
    #         == 2 (file444000/4@990: tip@988 over tip@980).
    if best_len == 2 and len(cands) == B2_M2TIP10_TO_M2TIP2_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP10_TO_M2TIP2_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP10_TO_M2TIP2_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf137. best==2, 3-cand: just-finished L, pre-byte == 132, M4 mid into +2
    #         age == 44 → M2 tip age == 66 (file444000/3@162 family).
    if (
        best_len == 2
        and len(cands) == B2_M4INTO2_TO_M2TIP66_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M4INTO2_TO_M2TIP66_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO2_TO_M2TIP66_INTO
            and (pos - chosen) == B2_M4INTO2_TO_M2TIP66_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4INTO2_TO_M2TIP66_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf138. best==3, 3-cand: just-finished L, M2 mid into +1 age == 8 → M4 mid
    #         into +3 age == 24 (file444000/4@1001: mid 977 of M4@974 over mid 993).
    if best_len == 3 and len(cands) == B3_M2INTO1_TO_M4INTO3_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == B3_M2INTO1_TO_M4INTO3_SRC_INTO
            and (pos - chosen) == B3_M2INTO1_TO_M4INTO3_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B3_M2INTO1_TO_M4INTO3_DST_INTO
                and (pos - c) == B3_M2INTO1_TO_M4INTO3_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf139. best==3, 2-cand: just-finished L, lit tip age == 120 → M4 mid into
    #         +1 age == 98 (file444000/3@215: mid 117 of M4@116 over L@95).
    if best_len == 3 and len(cands) == B3_LIT120_TO_M4INTO1_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B3_LIT120_TO_M4INTO1_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B3_LIT120_TO_M4INTO1_INTO
                and (pos - c) == B3_LIT120_TO_M4INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf140. best==3, 2-cand: just-finished L, M4 tip age == 18 → M3 mid into
    #         +1 age == 24 (file102000/0@538 family).
    if best_len == 3 and len(cands) == B3_M4TIP18_TO_M3INTO1_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B3_M4TIP18_TO_M3INTO1_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B3_M4TIP18_TO_M3INTO1_INTO
                and (pos - c) == B3_M4TIP18_TO_M3INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf141. best==2, 5-cand: just-finished L, M3 mid into +1 age == 32 → lit
    #         tip age == 166 (file444000/3@248: L@82 over mid 216 of M3@215).
    if best_len == 2 and len(cands) == B2_M3INTO1_TO_LIT166_NC:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_LIT166_INTO
            and (pos - chosen) == B2_M3INTO1_TO_LIT166_MID_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M3INTO1_TO_LIT166_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf142. best==2, 5-cand: just-finished L, M4 tip age == 36 → M2 tip age
    #         == 86 (file444000/3@266: tip@180 over tip@230).
    if best_len == 2 and len(cands) == B2_M4TIP36_TO_M2TIP86_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP36_TO_M2TIP86_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4TIP36_TO_M2TIP86_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf143. best==2, 2-cand: just-finished L, pre-byte == 12, M2 tip age == 42
    #         → M2 mid into +1 age == 43 (file102000/0@552 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP42_TO_M2INTO1_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP42_TO_M2INTO1_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP42_TO_M2INTO1_TIP_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B2_M2TIP42_TO_M2INTO1_INTO
                and (pos - c) == B2_M2TIP42_TO_M2INTO1_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf144. best==2, 6-cand: just-finished L, M2 tip age == 44 → M4 tip age
    #         == 80 (file444000/3@310: tip@230 over tip@266).
    if best_len == 2 and len(cands) == B2_M2TIP44_TO_M4TIP80_NC:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP44_TO_M4TIP80_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == B2_M2TIP44_TO_M4TIP80_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf145. best==2, 2-cand: just-finished M2, pre-byte == 148, lit tip age
    #         == 236 → M2 tip age == 142 (file444000/3@392: tip@250 over L@156).
    if (
        best_len == 2
        and len(cands) == B2_LIT236_TO_M2TIP142_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_LIT236_TO_M2TIP142_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B2_LIT236_TO_M2TIP142_LIT_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_LIT236_TO_M2TIP142_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf146. best==3, 5-cand: just-finished M4, pre-byte == 0, M4 mid into +3
    #         age == 1 → M2 mid into +1 age == 58 (file102000/0@567 family).
    if (
        best_len == 3
        and len(cands) == B3_M4INTO3_TO_M2INTO1_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_M4INTO3_TO_M2INTO1_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B3_M4INTO3_TO_M2INTO1_SRC_INTO
            and (pos - chosen) == B3_M4INTO3_TO_M2INTO1_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == B3_M4INTO3_TO_M2INTO1_DST_INTO
                and (pos - c) == B3_M4INTO3_TO_M2INTO1_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf147. best==4, 2-cand: just-finished M4, pre-byte == 13, lit tip age
    #         == 46 → M4 tip age == 4 (file102000/0@642 family).
    if (
        best_len == 4
        and len(cands) == B4_LIT46_TO_M4TIP4_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B4_LIT46_TO_M4TIP4_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B4_LIT46_TO_M4TIP4_LIT_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == B4_LIT46_TO_M4TIP4_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf148. best==2, 5-cand: just-finished M4, pre-byte == 13, M4 mid into +2
    #         age == 2 → M2 tip age == 48 (file102000/0@646 family).
    if (
        best_len == 2
        and len(cands) == B2_M4INTO2_TO_M2TIP48_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M4INTO2_TO_M2TIP48_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == B2_M4INTO2_TO_M2TIP48_INTO
            and (pos - chosen) == B2_M4INTO2_TO_M2TIP48_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4INTO2_TO_M2TIP48_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf149. best==2, 3-cand: just-finished M2, pre-byte == 13, M6 mid into +1
    #         age == 22 → lit tip age == 30 (file102000/0@648 family).
    if (
        best_len == 2
        and len(cands) == B2_M6INTO1_TO_LIT30_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M6INTO1_TO_LIT30_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == B2_M6INTO1_TO_LIT30_INTO
            and (pos - chosen) == B2_M6INTO1_TO_LIT30_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M6INTO1_TO_LIT30_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf150. best==2, 7-cand: just-finished L, pre-byte == 8, M4 tip age == 26
    #         → M2 tip age == 42 (file102000/0@688 family).
    if (
        best_len == 2
        and len(cands) == B2_M4TIP26_TO_M2TIP42_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M4TIP26_TO_M2TIP42_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP26_TO_M2TIP42_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4TIP26_TO_M2TIP42_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf151. best==2, 2-cand: just-finished M2, pre-byte == 13, M2 tip age
    #         == 38 → lit tip age == 40 (file102000/0@736 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP38_TO_LIT40_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP38_TO_LIT40_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP38_TO_LIT40_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP38_TO_LIT40_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf152. best==2, 2-cand: just-finished M3, pre-byte == 17, M3 mid into
    #         +1 age == 2 → lit tip age == 32 (file102000/0@742 family).
    if (
        best_len == 2
        and len(cands) == B2_M3INTO1_TO_LIT32_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M3INTO1_TO_LIT32_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_TO_LIT32_INTO
            and (pos - chosen) == B2_M3INTO1_TO_LIT32_MID_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M3INTO1_TO_LIT32_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf153. best==2, 2-cand: just-finished M3, pre-byte == 8, M2 tip age
    #         == 12 → lit tip age == 86 (file102000/0@768 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP12_TO_LIT86_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP12_TO_LIT86_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP12_TO_LIT86_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP12_TO_LIT86_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf154. best==2, 8-cand: just-finished L, pre-byte == 16, M3 mid into
    #         +1 age == 218 → M3 mid into +1 age == 225 (file102000/0@786).
    if (
        best_len == 2
        and len(cands) == B2_M3INTO1_218_TO_225_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M3INTO1_218_TO_225_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == B2_M3INTO1_218_TO_225_INTO
            and (pos - chosen) == B2_M3INTO1_218_TO_225_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B2_M3INTO1_218_TO_225_INTO
                and (pos - c) == B2_M3INTO1_218_TO_225_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf155. best==2, 9-cand: just-finished L, pre-byte == 46, M2 tip age
    #         == 16 → M3 mid into +1 age == 234 (file102000/0@802 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP16_TO_M3INTO1_234_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP16_TO_M3INTO1_234_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP16_TO_M3INTO1_234_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B2_M2TIP16_TO_M3INTO1_234_INTO
                and (pos - c) == B2_M2TIP16_TO_M3INTO1_234_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf156. best==2, 2-cand: just-finished L, pre-byte == 8, M2 tip age == 56
    #         → M2 tip age == 72 (file102000/0@858 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP56_TO_M2TIP72_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP56_TO_M2TIP72_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP56_TO_M2TIP72_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP56_TO_M2TIP72_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf157. best==2, 5-cand: just-finished L, pre-byte == 4, M2 tip age == 8
    #         → M2 tip age == 6 (file102000/0@866 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP8_TO_M2TIP6_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP8_TO_M2TIP6_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP8_TO_M2TIP6_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP8_TO_M2TIP6_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf158. best==2, 2-cand: just-finished M3, pre-byte == 24, M2 tip age
    #         == 100 → lit tip age == 108 (file102000/0@916 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP100_TO_LIT108_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP100_TO_LIT108_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP100_TO_LIT108_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP100_TO_LIT108_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf159. best==2, 3-cand: just-finished M2, pre-byte == 28, M2 tip age
    #         == 2 → lit tip age == 106 (file102000/0@928 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP2_TO_LIT106_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP2_TO_LIT106_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP2_TO_LIT106_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP2_TO_LIT106_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf160. best==3, 2-cand: just-finished L, pre-byte == 10, lit tip age
    #         == 10 → M4 mid into +1 age == 8 (file102000/0@931 family).
    if (
        best_len == 3
        and len(cands) == B3_LIT10_TO_M4INTO1_8_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_LIT10_TO_M4INTO1_8_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B3_LIT10_TO_M4INTO1_8_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == B3_LIT10_TO_M4INTO1_8_INTO
                and (pos - c) == B3_LIT10_TO_M4INTO1_8_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf161. best==2, 3-cand: just-finished M2, pre-byte == 57, M4 tip age
    #         == 10 → lit tip age == 12 (file102000/0@970 family).
    if (
        best_len == 2
        and len(cands) == B2_M4TIP10_TO_LIT12_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M4TIP10_TO_LIT12_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP10_TO_LIT12_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M4TIP10_TO_LIT12_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf162. best==2, 6-cand: just-finished L, pre-byte == 127, M2 tip age
    #         == 156 → M2 tip age == 236 (file102000/0@1022 family).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP156_TO_M2TIP236_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP156_TO_M2TIP236_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP156_TO_M2TIP236_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP156_TO_M2TIP236_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf163. best==2, 6-cand: just-finished L, pre-byte == 91, M11 mid into
    #         +6 age == 6 → M11 mid into +8 age == 4 (file20209@174).
    if (
        best_len == 2
        and len(cands) == B2_M11INTO6_TO_INTO8_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M11INTO6_TO_INTO8_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 11
            and chosen - o == B2_M11INTO6_TO_INTO8_SRC_INTO
            and (pos - chosen) == B2_M11INTO6_TO_INTO8_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 11
                and c - owners.get(c, c) == B2_M11INTO6_TO_INTO8_DST_INTO
                and (pos - c) == B2_M11INTO6_TO_INTO8_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf164. best==3, 5-cand: just-finished L, pre-byte == 26, lit tip age
    #         == 16 → M11 mid into +7 age == 8 (file20209@177).
    if (
        best_len == 3
        and len(cands) == B3_LIT16_TO_M11INTO7_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_LIT16_TO_M11INTO7_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B3_LIT16_TO_M11INTO7_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 11
                and c - owners.get(c, c) == B3_LIT16_TO_M11INTO7_INTO
                and (pos - c) == B3_LIT16_TO_M11INTO7_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf165. best==3, 4-cand: just-finished L, pre-byte == 58, M7 mid into +2
    #         age == 8 → M7 mid into +4 age == 6 (file20209@203).
    if (
        best_len == 3
        and len(cands) == B3_M7INTO2_TO_INTO4_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_M7INTO2_TO_INTO4_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == B3_M7INTO2_TO_INTO4_SRC_INTO
            and (pos - chosen) == B3_M7INTO2_TO_INTO4_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == B3_M7INTO2_TO_INTO4_DST_INTO
                and (pos - c) == B3_M7INTO2_TO_INTO4_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf166. best==5, 5-cand: just-finished L, pre-byte == 103, lit tip age
    #         == 16 → M13 mid into +7 age == 8 (file20209@328).
    if (
        best_len == 5
        and len(cands) == B5_LIT16_TO_M13INTO7_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B5_LIT16_TO_M13INTO7_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B5_LIT16_TO_M13INTO7_LIT_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 13
                and c - owners.get(c, c) == B5_LIT16_TO_M13INTO7_INTO
                and (pos - c) == B5_LIT16_TO_M13INTO7_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf167. best==4, 2-cand: just-finished M2, pre-byte == 158, lit tip age
    #         == 22 → M6 tip age == 8 (file20209@399).
    if (
        best_len == 4
        and len(cands) == B4_LIT22_TO_M6TIP8_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B4_LIT22_TO_M6TIP8_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B4_LIT22_TO_M6TIP8_LIT_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 6
                and (pos - c) == B4_LIT22_TO_M6TIP8_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf168. best==7, 3-cand: just-finished M8, pre-byte == 190, M8 mid into
    #         +4 age == 4 → M8 mid into +6 age == 2 (file20209@417).
    if (
        best_len == 7
        and len(cands) == B7_M8INTO4_TO_INTO6_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B7_M8INTO4_TO_INTO6_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == B7_M8INTO4_TO_INTO6_SRC_INTO
            and (pos - chosen) == B7_M8INTO4_TO_INTO6_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == B7_M8INTO4_TO_INTO6_DST_INTO
                and (pos - c) == B7_M8INTO4_TO_INTO6_DST_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf169. best==4, 2-cand: just-finished M8, pre-byte == 45, M8 tip age
    #         == 8 → lit tip age == 14 (file20121@226).
    if (
        best_len == 4
        and len(cands) == B4_M8TIP8_TO_LIT14_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B4_M8TIP8_TO_LIT14_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 8
            and (pos - chosen) == B4_M8TIP8_TO_LIT14_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B4_M8TIP8_TO_LIT14_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf170. best==2, 3-cand: just-finished L, pre-byte == 148, M2 tip age
    #         == 18 → M2 tip age == 36 (file503500/4@558).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP18_TO_M2TIP36_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP18_TO_M2TIP36_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP18_TO_M2TIP36_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M2TIP18_TO_M2TIP36_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf171. best==3, 3-cand: just-finished M2, pre-byte == 58, lit tip age
    #         == 10 → M6 mid into +3 age == 6 (file20235@242).
    if (
        best_len == 3
        and len(cands) == B3_LIT10_TO_M6INTO3_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_LIT10_TO_M6INTO3_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == B3_LIT10_TO_M6INTO3_LIT_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == B3_LIT10_TO_M6INTO3_INTO
                and (pos - c) == B3_LIT10_TO_M6INTO3_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf172. best==2, 2-cand: just-finished L, pre-byte == 94, M2 tip age
    #         == 26 → lit tip age == 68 (file20702/20711@159).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP26_TO_LIT68_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP26_TO_LIT68_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP26_TO_LIT68_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP26_TO_LIT68_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf173. best==4, 2-cand: just-finished M2, pre-byte == 140, M4 tip age
    #         == 14 → lit tip age == 202 (file104900/2 family).
    if (
        best_len == 4
        and len(cands) == B4_M4TIP14_TO_LIT202_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B4_M4TIP14_TO_LIT202_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B4_M4TIP14_TO_LIT202_SRC_AGE
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B4_M4TIP14_TO_LIT202_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf174. best==2, 2-cand: just-finished L, pre-byte == 110, M3 tip age
    #         == 8 → M3 mid into +2 age == 10 (file20220@279).
    if (
        best_len == 2
        and len(cands) == B2_M3TIP8_TO_INTO2_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M3TIP8_TO_INTO2_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B2_M3TIP8_TO_INTO2_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B2_M3TIP8_TO_INTO2_INTO
                and (pos - c) == B2_M3TIP8_TO_INTO2_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf175. best==2, 2-cand: just-finished L, pre-byte == 78, M2 tip age
    #         == 20 → lit tip age == 34 (file20212@222).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP20_TO_LIT34_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP20_TO_LIT34_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP20_TO_LIT34_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            lits = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == B2_M2TIP20_TO_LIT34_LIT_AGE
            ]
            if lits:
                chosen = max(lits)

    # 8cf176. best==2, 3-cand: just-finished L, pre-byte == 33, M4 tip age
    #         == 6 → M2 tip age == 226 (file402200@278).
    if (
        best_len == 2
        and len(cands) == B2_M4TIP6_TO_M2TIP226_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M4TIP6_TO_M2TIP226_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B2_M4TIP6_TO_M2TIP226_SRC_AGE
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == B2_M4TIP6_TO_M2TIP226_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf177. 20702@189: best==2 nc3 pre126 jf('L',)
    #         ('Mtip', 2, 30) → ('Mtip', 2, 56)
    if (
        best_len == 2
        and len(cands) == R177_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R177_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 30
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 56
            ]
            if dests:
                chosen = max(dests)

    # 8cf178. 104900@536: best==2 nc4 pre160 jf('L',)
    #         ('Mtip', 2, 8) → ('Mmid', 4, 2, 18)
    if (
        best_len == 2
        and len(cands) == R178_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R178_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 8
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 2
                and (pos - c) == 18
            ]
            if dests:
                chosen = max(dests)

    # 8cf179. 104900@550: best==2 nc4 pre152 jf('M', 4)
    #         ('Mtip', 2, 26) → ('Mtip', 2, 30)
    if (
        best_len == 2
        and len(cands) == R179_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R179_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 26
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 30
            ]
            if dests:
                chosen = max(dests)

    # 8cf180. 104900@572: best==2 nc5 pre148 jf('L',)
    #         ('Mtip', 2, 22) → ('L', 236)
    if (
        best_len == 2
        and len(cands) == R180_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R180_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 22
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 236
            ]
            if dests:
                chosen = max(dests)

    # 8cf181. 104900@666: best==2 nc2 pre132 jf('L',)
    #         ('Mmid', 2, 1, 14) → ('Mtip', 2, 4)
    if (
        best_len == 2
        and len(cands) == R181_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R181_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 14
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 4
            ]
            if dests:
                chosen = max(dests)

    # 8cf182. 104900@698: best==2 nc2 pre157 jf('L',)
    #         ('Mmid', 2, 1, 28) → ('L', 38)
    if (
        best_len == 2
        and len(cands) == R182_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R182_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 28
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 38
            ]
            if dests:
                chosen = max(dests)

    # 8cf183. 104900@718: best==2 nc4 pre140 jf('M', 4)
    #         ('Mmid', 3, 1, 46) → ('Mtip', 2, 52)
    if (
        best_len == 2
        and len(cands) == R183_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R183_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 46
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 52
            ]
            if dests:
                chosen = max(dests)

    # 8cf184. 104900@729: best==2 nc2 pre231 jf('M', 2)
    #         ('Mmid', 2, 1, 136) → ('Mmid', 4, 3, 210)
    if (
        best_len == 2
        and len(cands) == R184_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R184_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 136
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 3
                and (pos - c) == 210
            ]
            if dests:
                chosen = max(dests)

    # 8cf185. 104900@816: best==2 nc2 pre144 jf('L',)
    #         ('Mtip', 2, 250) → ('Mtip', 2, 18)
    if (
        best_len == 2
        and len(cands) == R185_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R185_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 250
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 18
            ]
            if dests:
                chosen = max(dests)

    # 8cf186. 503500@629: best==3 nc2 pre172 jf('L',)
    #         ('Mmid', 5, 2, 42) → ('Mtip', 2, 100)
    if (
        best_len == 3
        and len(cands) == R186_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R186_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 2
            and (pos - chosen) == 42
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 100
            ]
            if dests:
                chosen = max(dests)

    # 8cf187. 503500@715: best==2 nc2 pre104 jf('L',)
    #         ('Mtip', 3, 104) → ('L', 158)
    if (
        best_len == 2
        and len(cands) == R187_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R187_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 104
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 158
            ]
            if dests:
                chosen = max(dests)

    # 8cf188. 503500@801: best==2 nc3 pre7 jf('L',)
    #         ('Mmid', 3, 2, 4) → ('L', 40)
    if (
        best_len == 2
        and len(cands) == R188_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R188_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 2
            and (pos - chosen) == 4
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 40
            ]
            if dests:
                chosen = max(dests)

    # 8cf189. 503500@886: best==2 nc2 pre188 jf('L',)
    #         ('Mtip', 3, 6) → ('L', 10)
    if (
        best_len == 2
        and len(cands) == R189_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R189_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 6
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 10
            ]
            if dests:
                chosen = max(dests)

    # 8cf190. 503500@892: best==2 nc4 pre184 jf('L',)
    #         ('Mtip', 3, 4) → ('Mtip', 3, 12)
    if (
        best_len == 2
        and len(cands) == R190_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R190_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 4
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 12
            ]
            if dests:
                chosen = max(dests)

    # 8cf191. 20121@263: best==2 nc3 pre41 jf('M', 2)
    #         ('L', 10) → ('Mtip', 3, 9)
    if (
        best_len == 2
        and len(cands) == R191_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R191_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 10
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 9
            ]
            if dests:
                chosen = max(dests)

    # 8cf192. 20121@308: best==4 nc4 pre28 jf('M', 4)
    #         ('Mmid', 7, 2, 12) → ('Mtip', 7, 14)
    if (
        best_len == 4
        and len(cands) == R192_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R192_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == 2
            and (pos - chosen) == 12
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 7
                and (pos - c) == 14
            ]
            if dests:
                chosen = max(dests)

    # 8cf193. 20121@369: best==9 nc3 pre198 jf('M', 8)
    #         ('Mmid', 8, 4, 4) → ('Mmid', 8, 6, 2)
    if (
        best_len == 9
        and len(cands) == R193_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R193_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == 4
            and (pos - chosen) == 4
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 6
                and (pos - c) == 2
            ]
            if dests:
                chosen = max(dests)

    # 8cf194. 20121@430: best==6 nc3 pre12 jf('L',)
    #         ('Mmid', 8, 5, 10) → ('Mmid', 8, 7, 8)
    if (
        best_len == 6
        and len(cands) == R194_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R194_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == 5
            and (pos - chosen) == 10
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 7
                and (pos - c) == 8
            ]
            if dests:
                chosen = max(dests)

    # 8cf195. 20121@463: best==7 nc5 pre99 jf('M', 10)
    #         ('L', 18) → ('Mmid', 10, 8, 2)
    if (
        best_len == 7
        and len(cands) == R195_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R195_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 18
            and any(
                info[0] == "M"
                and info[1] == 10
                and own + 10 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 10
                and c - owners.get(c, c) == 8
                and (pos - c) == 2
            ]
            if dests:
                chosen = max(dests)

    # 8cf196. 20235@284: best==2 nc2 pre53 jf('L',)
    #         ('Mtip', 3, 4) → ('L', 8)
    if (
        best_len == 2
        and len(cands) == R196_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R196_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 4
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 8
            ]
            if dests:
                chosen = max(dests)

    # 8cf197. 20235@311: best==2 nc2 pre141 jf('M', 2)
    #         ('Mtip', 2, 2) → ('Mmid', 3, 2, 6)
    if (
        best_len == 2
        and len(cands) == R197_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R197_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 2
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 2
                and (pos - c) == 6
            ]
            if dests:
                chosen = max(dests)

    # 8cf198. 20235@339: best==2 nc3 pre107 jf('M', 3)
    #         ('Mmid', 3, 1, 2) → ('Mtip', 2, 6)
    if (
        best_len == 2
        and len(cands) == R198_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R198_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 2
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 6
            ]
            if dests:
                chosen = max(dests)

    # 8cf199. 20235@462: best==2 nc2 pre12 jf('L',)
    #         ('L', 6) → ('Mmid', 4, 1, 4)
    if (
        best_len == 2
        and len(cands) == R199_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R199_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 6
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 4
            ]
            if dests:
                chosen = max(dests)

    # 8cf200. 20235@481: best==2 nc2 pre66 jf('M', 3)
    #         ('Mtip', 3, 63) → ('L', 67)
    if (
        best_len == 2
        and len(cands) == R200_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R200_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 63
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 67
            ]
            if dests:
                chosen = max(dests)

    # 8cf201. 20235@510: best==2 nc18 pre0 jf('M', 17)
    #         ('Mmid', 17, 15, 2) → ('Mmid', 17, 16, 1)
    if (
        best_len == 2
        and len(cands) == R201_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R201_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 17
            and chosen - o == 15
            and (pos - chosen) == 2
            and any(
                info[0] == "M"
                and info[1] == 17
                and own + 17 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 17
                and c - owners.get(c, c) == 16
                and (pos - c) == 1
            ]
            if dests:
                chosen = max(dests)

    # 8cf202. 402200@346: best==2 nc3 pre8 jf('L',)
    #         ('Mtip', 2, 34) → ('Mtip', 2, 36)
    if (
        best_len == 2
        and len(cands) == R202_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R202_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 34
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 36
            ]
            if dests:
                chosen = max(dests)

    # 8cf203. 402200@358: best==2 nc2 pre0 jf('M', 2)
    #         ('Mtip', 2, 6) → ('Mtip', 2, 118)
    if (
        best_len == 2
        and len(cands) == R203_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R203_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 6
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 118
            ]
            if dests:
                chosen = max(dests)

    # 8cf204. 402200@400: best==2 nc4 pre16 jf('M', 2)
    #         ('Mtip', 2, 54) → ('Mtip', 2, 90)
    if (
        best_len == 2
        and len(cands) == R204_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R204_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 54
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 90
            ]
            if dests:
                chosen = max(dests)

    # 8cf205. 402200@414: best==2 nc6 pre24 jf('M', 2)
    #         ('Mtip', 4, 10) → ('L', 112)
    if (
        best_len == 2
        and len(cands) == R205_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R205_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 10
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 112
            ]
            if dests:
                chosen = max(dests)

    # 8cf206. 402200@434: best==2 nc3 pre41 jf('M', 2)
    #         ('Mtip', 2, 14) → ('L', 116)
    if (
        best_len == 2
        and len(cands) == R206_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R206_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 14
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 116
            ]
            if dests:
                chosen = max(dests)

    # 8cf207. 402200@510: best==2 nc3 pre90 jf('L',)
    #         ('Mtip', 2, 142) → ('Mtip', 2, 152)
    if (
        best_len == 2
        and len(cands) == R207_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R207_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 142
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 152
            ]
            if dests:
                chosen = max(dests)

    # 8cf208. 20220@308: best==2 nc2 pre45 jf('M', 2)
    #         ('Mmid', 3, 1, 4) → ('L', 14)
    if (
        best_len == 2
        and len(cands) == R208_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R208_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 4
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 14
            ]
            if dests:
                chosen = max(dests)

    # 8cf209. 20220@344: best==2 nc6 pre45 jf('M', 4)
    #         ('Mmid', 5, 1, 8) → ('Mmid', 5, 3, 14)
    if (
        best_len == 2
        and len(cands) == R209_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R209_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 1
            and (pos - chosen) == 8
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 3
                and (pos - c) == 14
            ]
            if dests:
                chosen = max(dests)

    # 8cf210. 20220@350: best==2 nc4 pre49 jf('M', 2)
    #         ('Mmid', 4, 2, 8) → ('Mmid', 5, 3, 12)
    if (
        best_len == 2
        and len(cands) == R210_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R210_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 2
            and (pos - chosen) == 8
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 3
                and (pos - c) == 12
            ]
            if dests:
                chosen = max(dests)

    # 8cf211. 20220@353: best==3 nc4 pre145 jf('L',)
    #         ('Mmid', 4, 1, 12) → ('Mmid', 5, 4, 22)
    if (
        best_len == 3
        and len(cands) == R211_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R211_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 1
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 4
                and (pos - c) == 22
            ]
            if dests:
                chosen = max(dests)

    # 8cf212. 20220@376: best==2 nc2 pre45 jf('M', 2)
    #         ('Mtip', 2, 2) → ('L', 6)
    if (
        best_len == 2
        and len(cands) == R212_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R212_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 2
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 6
            ]
            if dests:
                chosen = max(dests)

    # 8cf213. 20220@429: best==3 nc2 pre210 jf('L',)
    #         ('L', 18) → ('Mmid', 6, 1, 6)
    if (
        best_len == 3
        and len(cands) == R213_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R213_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 18
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == 1
                and (pos - c) == 6
            ]
            if dests:
                chosen = max(dests)

    # 8cf214. 20220@466: best==2 nc2 pre62 jf('L',)
    #         ('Mtip', 2, 4) → ('L', 18)
    if (
        best_len == 2
        and len(cands) == R214_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R214_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 4
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 18
            ]
            if dests:
                chosen = max(dests)

    # 8cf215. 20212@361: best==2 nc2 pre215 jf('L',)
    #         ('Mtip', 2, 24) → ('L', 28)
    if (
        best_len == 2
        and len(cands) == R215_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R215_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 24
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 28
            ]
            if dests:
                chosen = max(dests)

    # 8cf216. 20212@400: best==2 nc3 pre91 jf('L',)
    #         ('Mtip', 3, 4) → ('Mtip', 2, 14)
    if (
        best_len == 2
        and len(cands) == R216_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R216_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 4
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 14
            ]
            if dests:
                chosen = max(dests)

    # 8cf217. 20212@453: best==2 nc4 pre25 jf('L',)
    #         ('Mtip', 3, 12) → ('Mtip', 2, 20)
    if (
        best_len == 2
        and len(cands) == R217_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R217_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf218. 20212@498: best==2 nc3 pre107 jf('L',)
    #         ('Mtip', 7, 8) → ('Mmid', 7, 4, 4)
    if (
        best_len == 2
        and len(cands) == R218_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R218_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 7
            and (pos - chosen) == 8
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 4
                and (pos - c) == 4
            ]
            if dests:
                chosen = max(dests)

    # 8cf219. best==2, 3-cand: pre-byte == 0, M2 tip age == 28 → M4 tip age
    #         == 20 (file50146@124).
    if (
        best_len == 2
        and len(cands) == B2_M2TIP28_TO_M4TIP20_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B2_M2TIP28_TO_M4TIP20_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == B2_M2TIP28_TO_M4TIP20_SRC_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == B2_M2TIP28_TO_M4TIP20_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf220. best==3, 2-cand: pre-byte == 75, M3 tip age == 4 → M3 mid into
    #         +1 age == 12 (file20600@14).
    if (
        best_len == 3
        and len(cands) == B3_M3TIP4_TO_INTO1_12_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_M3TIP4_TO_INTO1_12_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B3_M3TIP4_TO_INTO1_12_SRC_AGE
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == B3_M3TIP4_TO_INTO1_12_INTO
                and (pos - c) == B3_M3TIP4_TO_INTO1_12_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf221. best==4, 3-cand: pre-byte == 142, M4 tip age == 16 → M3 tip age
    #         == 44 (file50119/20/21@136).
    if (
        best_len == 4
        and len(cands) == B4_M4TIP16_TO_M3TIP44_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B4_M4TIP16_TO_M3TIP44_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == B4_M4TIP16_TO_M3TIP44_SRC_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == B4_M4TIP16_TO_M3TIP44_DST_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf222. best==3, 33-cand: pre-byte == 137, M6 mid into +3 age == 24 → M3
    #         tip age == 36 (file30406/07@77).
    if (
        best_len == 3
        and len(cands) == B3_M6INTO3_TO_M3TIP36_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_M6INTO3_TO_M3TIP36_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == B3_M6INTO3_TO_M3TIP36_INTO
            and (pos - chosen) == B3_M6INTO3_TO_M3TIP36_MID_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == B3_M6INTO3_TO_M3TIP36_TIP_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8cf223. best==3, 33-cand: pre-byte == 117, M3 tip age == 24 → M7 mid into
    #         +4 age == 16 (file30300/01@81).
    if (
        best_len == 3
        and len(cands) == B3_M3TIP24_TO_M7INTO4_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == B3_M3TIP24_TO_M7INTO4_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == B3_M3TIP24_TO_M7INTO4_SRC_AGE
        ):
            mids = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == B3_M3TIP24_TO_M7INTO4_INTO
                and (pos - c) == B3_M3TIP24_TO_M7INTO4_MID_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cf224. pe2pkg 50146@136: best==4 nc3 pre142 jf('L',)
    #         ('Mtip', 4, 16) → ('Mtip', 3, 64)
    if (
        best_len == 4
        and len(cands) == R224_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R224_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 16
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 64
            ]
            if dests:
                chosen = max(dests)

    # 8cf225. pe2pkg 50146@152: best==4 nc4 pre16 jf('M', 3)
    #         ('Mtip', 4, 16) → ('Mtip', 4, 72)
    if (
        best_len == 4
        and len(cands) == R225_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R225_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 16
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 72
            ]
            if dests:
                chosen = max(dests)

    # 8cf226. pe2pkg 50146@161: best==2 nc3 pre30 jf('L',)
    #         ('Mmid', 3, 1, 32) → ('Mtip', 2, 44)
    if (
        best_len == 2
        and len(cands) == R226_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R226_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 32
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 44
            ]
            if dests:
                chosen = max(dests)

    # 8cf227. pe2pkg 50146@168: best==4 nc5 pre8 jf('M', 3)
    #         ('Mtip', 4, 16) → ('Mtip', 4, 48)
    if (
        best_len == 4
        and len(cands) == R227_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R227_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 16
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 48
            ]
            if dests:
                chosen = max(dests)

    # 8cf228. pe2pkg 30406@121: best==3 nc44 pre32 jf('L',)
    #         ('Mmid', 4, 1, 16) → ('Mmid', 7, 4, 20)
    if (
        best_len == 3
        and len(cands) == R228_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R228_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 1
            and (pos - chosen) == 16
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 4
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf229. pe2pkg 30406@125: best==3 nc45 pre40 jf('L',)
    #         ('Mtip', 7, 16) → ('Mmid', 6, 3, 72)
    if (
        best_len == 3
        and len(cands) == R229_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R229_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 7
            and (pos - chosen) == 16
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == 3
                and (pos - c) == 72
            ]
            if dests:
                chosen = max(dests)

    # 8cf230. pe2pkg 30406@164: best==3 nc2 pre0 jf('M', 2)
    #         ('L', 36) → ('Mmid', 7, 3, 24)
    if (
        best_len == 3
        and len(cands) == R230_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R230_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 36
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 3
                and (pos - c) == 24
            ]
            if dests:
                chosen = max(dests)

    # 8cf231. pe2pkg 30406@174: best==2 nc3 pre0 jf('M', 5)
    #         ('Mtip', 4, 18) → ('Mtip', 2, 42)
    if (
        best_len == 2
        and len(cands) == R231_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R231_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 18
            and any(
                info[0] == "M"
                and info[1] == 5
                and own + 5 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 42
            ]
            if dests:
                chosen = max(dests)

    # 8cf232. pe2pkg 30300@97: best==3 nc35 pre102 jf('L',)
    #         ('Mtip', 3, 16) → ('Mtip', 3, 48)
    if (
        best_len == 3
        and len(cands) == R232_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R232_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 16
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 48
            ]
            if dests:
                chosen = max(dests)

    # 8cf233. pe2pkg 30300@121: best==3 nc38 pre72 jf('L',)
    #         ('Mtip', 3, 24) → ('Mtip', 3, 40)
    if (
        best_len == 3
        and len(cands) == R233_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R233_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 24
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 40
            ]
            if dests:
                chosen = max(dests)

    # 8cf234. pe2pkg 30300@161: best==3 nc46 pre8 jf('L',)
    #         ('Mtip', 3, 12) → ('Mtip', 3, 40)
    if (
        best_len == 3
        and len(cands) == R234_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R234_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 40
            ]
            if dests:
                chosen = max(dests)

    # 8cf235. pe2pkg 30300@180: best==4 nc3 pre0 jf('M', 2)
    #         ('Mtip', 4, 12) → ('L', 36)
    if (
        best_len == 4
        and len(cands) == R235_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R235_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 12
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 36
            ]
            if dests:
                chosen = max(dests)

    # 8cf236. pe2pkg 20600@60: best==5 nc3 pre173 jf('L',)
    #         ('Mtip', 5, 20) → ('Mtip', 5, 32)
    if (
        best_len == 5
        and len(cands) == R236_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R236_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 5
            and (pos - chosen) == 20
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == 32
            ]
            if dests:
                chosen = max(dests)

    # 8cf237. pe2pkg 20600@70: best==3 nc6 pre82 jf('M', 2)
    #         ('Mmid', 5, 2, 8) → ('Mmid', 5, 2, 28)
    if (
        best_len == 3
        and len(cands) == R237_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R237_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 2
            and (pos - chosen) == 8
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 2
                and (pos - c) == 28
            ]
            if dests:
                chosen = max(dests)

    # 8cf238. pe2pkg 20600@88: best==5 nc4 pre106 jf('L',)
    #         ('Mtip', 5, 28) → ('Mtip', 5, 48)
    if (
        best_len == 5
        and len(cands) == R238_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R238_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 5
            and (pos - chosen) == 28
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == 48
            ]
            if dests:
                chosen = max(dests)

    # 8cf239. pe2pkg 20600@120: best==2 nc2 pre125 jf('L',)
    #         ('Mtip', 3, 4) → ('L', 98)
    if (
        best_len == 2
        and len(cands) == R239_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R239_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 4
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 98
            ]
            if dests:
                chosen = max(dests)

    # 8cf240. pe2pkg file50146@193: best==2 nc3 pre52 jf('L',)
    #         ('Mmid', 2, 1, 68) → ('L', 144)
    if (
        best_len == 2
        and len(cands) == R240_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R240_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 68
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 144
            ]
            if dests:
                chosen = max(dests)

    # 8cf241. pe2pkg file50146@217: best==2 nc2 pre26 jf('L',)
    #         ('Mmid', 2, 1, 8) → ('L', 84)
    if (
        best_len == 2
        and len(cands) == R241_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R241_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 8
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 84
            ]
            if dests:
                chosen = max(dests)

    # 8cf242. pe2pkg file50146@224: best==4 nc8 pre20 jf('L',)
    #         ('Mmid', 8, 4, 36) → ('Mtip', 4, 72)
    if (
        best_len == 4
        and len(cands) == R242_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R242_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == 4
            and (pos - chosen) == 36
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 72
            ]
            if dests:
                chosen = max(dests)

    # 8cf243. pe2pkg file50146@257: best==3 nc2 pre18 jf('L',)
    #         ('Mmid', 3, 2, 106) → ('Mtip', 5, 78)
    if (
        best_len == 3
        and len(cands) == R243_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R243_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 2
            and (pos - chosen) == 106
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == 78
            ]
            if dests:
                chosen = max(dests)

    # 8cf244. pe2pkg file50146@306: best==2 nc7 pre22 jf('L',)
    #         ('Mmid', 3, 1, 3) → ('Mtip', 2, 62)
    if (
        best_len == 2
        and len(cands) == R244_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R244_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 3
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 62
            ]
            if dests:
                chosen = max(dests)

    # 8cf245. pe2pkg file50146@314: best==2 nc8 pre54 jf('L',)
    #         ('Mtip', 2, 8) → ('Mtip', 2, 32)
    if (
        best_len == 2
        and len(cands) == R245_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R245_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 8
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 32
            ]
            if dests:
                chosen = max(dests)

    # 8cf246. pe2pkg file50146@380: best==3 nc2 pre174 jf('M', 4)
    #         ('Mtip', 3, 40) → ('Mtip', 2, 56)
    if (
        best_len == 3
        and len(cands) == R246_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R246_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 40
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 56
            ]
            if dests:
                chosen = max(dests)

    # 8cf247. pe2pkg file50146@384: best==4 nc10 pre146 jf('L',)
    #         ('Mmid', 5, 1, 16) → ('Mmid', 5, 1, 52)
    if (
        best_len == 4
        and len(cands) == R247_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R247_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 1
            and (pos - chosen) == 16
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 1
                and (pos - c) == 52
            ]
            if dests:
                chosen = max(dests)

    # 8cf248. pe2pkg file30406@181: best==3 nc4 pre254 jf('L',)
    #         ('L', 53) → ('Mtip', 4, 5)
    if (
        best_len == 3
        and len(cands) == R248_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R248_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 53
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 5
            ]
            if dests:
                chosen = max(dests)

    # 8cf249. pe2pkg file30406@197: best==7 nc16 pre88 jf('L',)
    #         ('Mtip', 7, 88) → ('Mtip', 11, 52)
    if (
        best_len == 7
        and len(cands) == R249_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R249_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 7
            and (pos - chosen) == 88
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 11
                and (pos - c) == 52
            ]
            if dests:
                chosen = max(dests)

    # 8cf250. pe2pkg file30406@289: best==3 nc8 pre166 jf('L',)
    #         ('Mtip', 3, 232) → ('Mmid', 7, 4, 8)
    if (
        best_len == 3
        and len(cands) == R250_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R250_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 232
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 4
                and (pos - c) == 8
            ]
            if dests:
                chosen = max(dests)

    # 8cf251. pe2pkg file30406@305: best==3 nc64 pre90 jf('L',)
    #         ('Mmid', 11, 4, 156) → ('Mtip', 3, 180)
    if (
        best_len == 3
        and len(cands) == R251_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R251_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 11
            and chosen - o == 4
            and (pos - chosen) == 156
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 180
            ]
            if dests:
                chosen = max(dests)

    # 8cf252. pe2pkg file30406@308: best==2 nc2 pre0 jf('M', 3)
    #         ('Mtip', 4, 124) → ('L', 128)
    if (
        best_len == 2
        and len(cands) == R252_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R252_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 124
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 128
            ]
            if dests:
                chosen = max(dests)

    # 8cf253. pe2pkg file30406@310: best==6 nc2 pre255 jf('M', 2)
    #         ('Mmid', 3, 1, 188) → ('Mmid', 7, 1, 40)
    if (
        best_len == 6
        and len(cands) == R253_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R253_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 188
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 1
                and (pos - c) == 40
            ]
            if dests:
                chosen = max(dests)

    # 8cf254. pe2pkg file30406@328: best==4 nc3 pre0 jf('M', 4)
    #         ('Mtip', 4, 240) → ('Mmid', 7, 3, 48)
    if (
        best_len == 4
        and len(cands) == R254_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R254_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 240
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 3
                and (pos - c) == 48
            ]
            if dests:
                chosen = max(dests)

    # 8cf255. pe2pkg file30406@348: best==2 nc2 pre0 jf('M', 4)
    #         ('L', 214) → ('Mtip', 2, 188)
    if (
        best_len == 2
        and len(cands) == R255_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R255_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 214
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 188
            ]
            if dests:
                chosen = max(dests)

    # 8cf256. pe2pkg file30300@185: best==3 nc4 pre254 jf('L',)
    #         ('L', 41) → ('Mtip', 4, 5)
    if (
        best_len == 3
        and len(cands) == R256_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R256_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 41
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 5
            ]
            if dests:
                chosen = max(dests)

    # 8cf257. pe2pkg file30300@209: best==3 nc55 pre1 jf('L',)
    #         ('Mmid', 4, 1, 96) → ('Mmid', 4, 1, 12)
    if (
        best_len == 3
        and len(cands) == R257_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R257_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 1
            and (pos - chosen) == 96
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 12
            ]
            if dests:
                chosen = max(dests)

    # 8cf258. pe2pkg file30300@266: best==2 nc109 pre0 jf('M', 17)
    #         ('Mmid', 13, 11, 228) → ('Mmid', 7, 5, 136)
    if (
        best_len == 2
        and len(cands) == R258_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R258_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 13
            and chosen - o == 11
            and (pos - chosen) == 228
            and any(
                info[0] == "M"
                and info[1] == 17
                and own + 17 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 5
                and (pos - c) == 136
            ]
            if dests:
                chosen = max(dests)

    # 8cf259. pe2pkg file30300@270: best==2 nc109 pre0 jf('M', 2)
    #         ('Mmid', 4, 2, 72) → ('Mmid', 3, 1, 172)
    if (
        best_len == 2
        and len(cands) == R259_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R259_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 2
            and (pos - chosen) == 72
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 1
                and (pos - c) == 172
            ]
            if dests:
                chosen = max(dests)

    # 8cf260. pe2pkg file30300@273: best==3 nc11 pre241 jf('L',)
    #         ('Mtip', 3, 220) → ('Mmid', 4, 1, 164)
    if (
        best_len == 3
        and len(cands) == R260_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R260_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 220
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 164
            ]
            if dests:
                chosen = max(dests)

    # 8cf261. pe2pkg file30300@277: best==3 nc12 pre204 jf('L',)
    #         ('Mmid', 7, 4, 200) → ('Mmid', 4, 3, 82)
    if (
        best_len == 3
        and len(cands) == R261_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R261_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == 4
            and (pos - chosen) == 200
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 3
                and (pos - c) == 82
            ]
            if dests:
                chosen = max(dests)

    # 8cf262. pe2pkg file30300@281: best==3 nc67 pre35 jf('L',)
    #         ('Mmid', 2, 1, 12) → ('Mmid', 4, 1, 168)
    if (
        best_len == 3
        and len(cands) == R262_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R262_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 168
            ]
            if dests:
                chosen = max(dests)

    # 8cf263. pe2pkg file30300@293: best==3 nc14 pre139 jf('L',)
    #         ('Mtip', 3, 240) → ('Mtip', 3, 20)
    if (
        best_len == 3
        and len(cands) == R263_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R263_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 240
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf264. pe2pkg file20600@140: best==11 nc2 pre97 jf('L',)
    #         ('Mtip', 15, 16) → ('Mmid', 7, 2, 32)
    if (
        best_len == 11
        and len(cands) == R264_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R264_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 15
            and (pos - chosen) == 16
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 2
                and (pos - c) == 32
            ]
            if dests:
                chosen = max(dests)

    # 8cf265. pe2pkg file20600@180: best==7 nc4 pre0 jf('M', 4)
    #         ('Mmid', 7, 6, 68) → ('Mmid', 11, 4, 36)
    if (
        best_len == 7
        and len(cands) == R265_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R265_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == 6
            and (pos - chosen) == 68
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 11
                and c - owners.get(c, c) == 4
                and (pos - c) == 36
            ]
            if dests:
                chosen = max(dests)

    # 8cf266. pe2pkg file20600@201: best==2 nc6 pre131 jf('M', 3)
    #         ('Mmid', 3, 1, 84) → ('Mmid', 7, 5, 16)
    if (
        best_len == 2
        and len(cands) == R266_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R266_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 84
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 5
                and (pos - c) == 16
            ]
            if dests:
                chosen = max(dests)

    # 8cf267. pe2pkg file20600@208: best==5 nc9 pre104 jf('L',)
    #         ('Mtip', 5, 120) → ('Mtip', 11, 68)
    if (
        best_len == 5
        and len(cands) == R267_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R267_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 5
            and (pos - chosen) == 120
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 11
                and (pos - c) == 68
            ]
            if dests:
                chosen = max(dests)

    # 8cf268. pe2pkg file20600@230: best==3 nc3 pre112 jf('M', 2)
    #         ('L', 156) → ('Mmid', 12, 1, 136)
    if (
        best_len == 3
        and len(cands) == R268_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R268_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 156
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 12
                and c - owners.get(c, c) == 1
                and (pos - c) == 136
            ]
            if dests:
                chosen = max(dests)

    # 8cf269. pe2pkg file20600@240: best==3 nc5 pre120 jf('L',)
    #         ('Mtip', 3, 164) → ('Mmid', 2, 1, 6)
    if (
        best_len == 3
        and len(cands) == R269_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R269_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 164
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == 1
                and (pos - c) == 6
            ]
            if dests:
                chosen = max(dests)

    # 8cf270. pe2pkg file20600@244: best==5 nc2 pre114 jf('L',)
    #         ('Mtip', 5, 52) → ('Mtip', 4, 68)
    if (
        best_len == 5
        and len(cands) == R270_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R270_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 5
            and (pos - chosen) == 52
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 68
            ]
            if dests:
                chosen = max(dests)

    # 8cf271. pe2pkg file20600@256: best==3 nc2 pre104 jf('M', 3)
    #         ('Mmid', 3, 2, 24) → ('Mmid', 5, 4, 212)
    if (
        best_len == 3
        and len(cands) == R271_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R271_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 2
            and (pos - chosen) == 24
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 4
                and (pos - c) == 212
            ]
            if dests:
                chosen = max(dests)

    # 8cf272. pe2pkg file50119@152: best==4 nc4 pre16 jf('M', 2)
    #         ('Mtip', 4, 16) → ('Mtip', 4, 52)
    if (
        best_len == 4
        and len(cands) == R272_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R272_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 16
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 52
            ]
            if dests:
                chosen = max(dests)

    # 8cf273. pe2pkg file50119@169: best==3 nc14 pre0 jf('M', 2)
    #         ('Mmid', 4, 1, 32) → ('Mmid', 4, 1, 48)
    if (
        best_len == 3
        and len(cands) == R273_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R273_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 1
            and (pos - chosen) == 32
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 48
            ]
            if dests:
                chosen = max(dests)

    # 8cf274. pe2pkg file50119@229: best==2 nc2 pre76 jf('L',)
    #         ('Mtip', 2, 52) → ('L', 84)
    if (
        best_len == 2
        and len(cands) == R274_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R274_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 52
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 84
            ]
            if dests:
                chosen = max(dests)

    # 8cf275. pe2pkg file50119@236: best==2 nc3 pre38 jf('L',)
    #         ('Mtip', 2, 12) → ('L', 132)
    if (
        best_len == 2
        and len(cands) == R275_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R275_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 132
            ]
            if dests:
                chosen = max(dests)

    # 8cf276. pe2pkg file50119@276: best==2 nc2 pre174 jf('M', 3)
    #         ('L', 236) → ('Mtip', 2, 144)
    if (
        best_len == 2
        and len(cands) == R276_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R276_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 236
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 144
            ]
            if dests:
                chosen = max(dests)

    # 8cf277. pe2pkg file50119@306: best==2 nc25 pre72 jf('L',)
    #         ('Mmid', 2, 1, 34) → ('Mmid', 8, 6, 108)
    if (
        best_len == 2
        and len(cands) == R277_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R277_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 34
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 6
                and (pos - c) == 108
            ]
            if dests:
                chosen = max(dests)

    # 8cf278. pe2pkg file50119@322: best==2 nc2 pre16 jf('M', 3)
    #         ('L', 178) → ('Mtip', 2, 150)
    if (
        best_len == 2
        and len(cands) == R278_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R278_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 178
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 150
            ]
            if dests:
                chosen = max(dests)

    # 8cf279. pe2pkg file50119@356: best==2 nc24 pre0 jf('M', 4)
    #         ('Mmid', 4, 2, 202) → ('Mmid', 5, 3, 194)
    if (
        best_len == 2
        and len(cands) == R279_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R279_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 2
            and (pos - chosen) == 202
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 3
                and (pos - c) == 194
            ]
            if dests:
                chosen = max(dests)

    # 8cf280. pe2pkg file30200@126: best==2 nc64 pre0 jf('M', 2)
    #         ('Mmid', 4, 2, 118) → ('Mmid', 4, 2, 52)
    if (
        best_len == 2
        and len(cands) == R280_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R280_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 2
            and (pos - chosen) == 118
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 2
                and (pos - c) == 52
            ]
            if dests:
                chosen = max(dests)

    # 8cf281. pe2pkg file30200@185: best==3 nc73 pre12 jf('L',)
    #         ('Mmid', 17, 14, 23) → ('Mmid', 8, 5, 124)
    if (
        best_len == 3
        and len(cands) == R281_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R281_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 17
            and chosen - o == 14
            and (pos - chosen) == 23
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 5
                and (pos - c) == 124
            ]
            if dests:
                chosen = max(dests)

    # 8cf282. pe2pkg file30200@209: best==3 nc10 pre186 jf('L',)
    #         ('Mtip', 3, 144) → ('Mtip', 7, 8)
    if (
        best_len == 3
        and len(cands) == R282_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R282_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 144
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 7
                and (pos - c) == 8
            ]
            if dests:
                chosen = max(dests)

    # 8cf283. pe2pkg file30200@229: best==3 nc75 pre55 jf('L',)
    #         ('Mmid', 5, 2, 138) → ('Mmid', 4, 1, 8)
    if (
        best_len == 3
        and len(cands) == R283_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R283_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 2
            and (pos - chosen) == 138
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 8
            ]
            if dests:
                chosen = max(dests)

    # 8cf284. pe2pkg file30200@301: best==3 nc62 pre48 jf('L',)
    #         ('Mtip', 3, 252) → ('Mtip', 3, 116)
    if (
        best_len == 3
        and len(cands) == R284_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R284_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 252
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 116
            ]
            if dests:
                chosen = max(dests)

    # 8cf285. pe2pkg file30200@304: best==3 nc7 pre0 jf('M', 3)
    #         ('Mtip', 3, 203) → ('Mmid', 4, 1, 195)
    if (
        best_len == 3
        and len(cands) == R285_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R285_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 203
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 195
            ]
            if dests:
                chosen = max(dests)

    # 8cf286. pe2pkg file30200@346: best==2 nc2 pre0 jf('M', 2)
    #         ('Mmid', 4, 2, 8) → ('L', 36)
    if (
        best_len == 2
        and len(cands) == R286_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R286_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 2
            and (pos - chosen) == 8
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 36
            ]
            if dests:
                chosen = max(dests)

    # 8cf287. pe2pkg file30200@352: best==4 nc2 pre0 jf('M', 4)
    #         ('L', 256) → ('Mtip', 3, 12)
    if (
        best_len == 4
        and len(cands) == R287_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R287_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 256
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 12
            ]
            if dests:
                chosen = max(dests)

    # 8cf288. pe2pkg file50146@396: best==4 nc10 pre150 jf('M', 4)
    #         ('Mtip', 4, 12) → ('Mmid', 8, 4, 208)
    if (
        best_len == 4
        and len(cands) == R288_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R288_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 12
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 4
                and (pos - c) == 208
            ]
            if dests:
                chosen = max(dests)

    # 8cf289. pe2pkg file50146@412: best==4 nc2 pre150 jf('M', 7)
    #         ('Mtip', 4, 136) → ('Mtip', 2, 152)
    if (
        best_len == 4
        and len(cands) == R289_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R289_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 136
            and any(
                info[0] == "M"
                and info[1] == 7
                and own + 7 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 152
            ]
            if dests:
                chosen = max(dests)

    # 8cf290. pe2pkg file50146@436: best==4 nc3 pre39 jf('L',)
    #         ('Mtip', 4, 24) → ('Mtip', 4, 160)
    if (
        best_len == 4
        and len(cands) == R290_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R290_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 24
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 160
            ]
            if dests:
                chosen = max(dests)

    # 8cf291. pe2pkg file50146@483: best==2 nc5 pre165 jf('L',)
    #         ('Mmid', 7, 3, 75) → ('Mtip', 3, 143)
    if (
        best_len == 2
        and len(cands) == R291_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R291_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == 3
            and (pos - chosen) == 75
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 143
            ]
            if dests:
                chosen = max(dests)

    # 8cf292. pe2pkg file50146@588: best==2 nc6 pre166 jf('L',)
    #         ('Mmid', 4, 2, 54) → ('Mmid', 4, 2, 74)
    if (
        best_len == 2
        and len(cands) == R292_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R292_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 2
            and (pos - chosen) == 54
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 2
                and (pos - c) == 74
            ]
            if dests:
                chosen = max(dests)

    # 8cf293. pe2pkg file50146@598: best==2 nc2 pre6 jf('M', 2)
    #         ('Mmid', 2, 1, 16) → ('Mtip', 2, 8)
    if (
        best_len == 2
        and len(cands) == R293_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R293_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 16
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 8
            ]
            if dests:
                chosen = max(dests)

    # 8cf294. pe2pkg file50146@605: best==2 nc14 pre42 jf('L',)
    #         ('Mmid', 8, 6, 31) → ('Mmid', 5, 3, 235)
    if (
        best_len == 2
        and len(cands) == R294_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R294_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == 6
            and (pos - chosen) == 31
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 3
                and (pos - c) == 235
            ]
            if dests:
                chosen = max(dests)

    # 8cf295. pe2pkg file50146@617: best==2 nc4 pre8 jf('L',)
    #         ('Mtip', 3, 24) → ('Mmid', 8, 1, 48)
    if (
        best_len == 2
        and len(cands) == R295_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R295_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 24
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 1
                and (pos - c) == 48
            ]
            if dests:
                chosen = max(dests)

    # 8cf296. pe2pkg file50146@628: best==3 nc4 pre162 jf('L',)
    #         ('Mtip', 3, 44) → ('Mtip', 4, 236)
    if (
        best_len == 3
        and len(cands) == R296_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R296_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 44
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 236
            ]
            if dests:
                chosen = max(dests)

    # 8cf297. pe2pkg file50146@632: best==4 nc4 pre134 jf('L',)
    #         ('Mmid', 5, 1, 20) → ('Mmid', 8, 4, 60)
    if (
        best_len == 4
        and len(cands) == R297_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R297_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 1
            and (pos - chosen) == 20
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 4
                and (pos - c) == 60
            ]
            if dests:
                chosen = max(dests)

    # 8cf298. pe2pkg file30406@350: best==2 nc3 pre0 jf('M', 2)
    #         ('Mmid', 6, 2, 38) → ('L', 226)
    if (
        best_len == 2
        and len(cands) == R298_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R298_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == 2
            and (pos - chosen) == 38
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 226
            ]
            if dests:
                chosen = max(dests)

    # 8cf299. pe2pkg file30406@352: best==4 nc2 pre0 jf('M', 2)
    #         ('Mtip', 4, 176) → ('Mtip', 3, 188)
    if (
        best_len == 4
        and len(cands) == R299_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R299_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 176
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 188
            ]
            if dests:
                chosen = max(dests)

    # 8cf300. pe2pkg file30406@356: best==4 nc2 pre40 jf('M', 4)
    #         ('Mmid', 6, 2, 92) → ('Mtip', 4, 200)
    if (
        best_len == 4
        and len(cands) == R300_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R300_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == 2
            and (pos - chosen) == 92
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 200
            ]
            if dests:
                chosen = max(dests)

    # 8cf301. pe2pkg file30406@469: best==3 nc79 pre227 jf('L',)
    #         ('Mmid', 10, 5, 132) → ('Mmid', 11, 8, 64)
    if (
        best_len == 3
        and len(cands) == R301_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R301_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 10
            and chosen - o == 5
            and (pos - chosen) == 132
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 11
                and c - owners.get(c, c) == 8
                and (pos - c) == 64
            ]
            if dests:
                chosen = max(dests)

    # 8cf302. pe2pkg file30406@473: best==3 nc80 pre70 jf('L',)
    #         ('Mmid', 7, 4, 200) → ('Mmid', 6, 3, 8)
    if (
        best_len == 3
        and len(cands) == R302_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R302_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == 4
            and (pos - chosen) == 200
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == 3
                and (pos - c) == 8
            ]
            if dests:
                chosen = max(dests)

    # 8cf303. pe2pkg file30406@489: best==3 nc7 pre206 jf('L',)
    #         ('Mmid', 4, 1, 160) → ('Mmid', 4, 1, 192)
    if (
        best_len == 3
        and len(cands) == R303_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R303_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 1
            and (pos - chosen) == 160
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 192
            ]
            if dests:
                chosen = max(dests)

    # 8cf304. pe2pkg file30406@546: best==2 nc2 pre0 jf('M', 3)
    #         ('Mtip', 17, 184) → ('Mtip', 4, 202)
    if (
        best_len == 2
        and len(cands) == R304_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R304_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 17
            and (pos - chosen) == 184
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 202
            ]
            if dests:
                chosen = max(dests)

    # 8cf305. pe2pkg file30406@552: best==3 nc9 pre0 jf('M', 4)
    #         ('Mmid', 17, 10, 163) → ('Mtip', 4, 200)
    if (
        best_len == 3
        and len(cands) == R305_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R305_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 17
            and chosen - o == 10
            and (pos - chosen) == 163
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 200
            ]
            if dests:
                chosen = max(dests)

    # 8cf306. pe2pkg file30406@582: best==2 nc103 pre0 jf('M', 4)
    #         ('Mmid', 4, 2, 68) → ('Mmid', 4, 2, 236)
    if (
        best_len == 2
        and len(cands) == R306_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R306_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 2
            and (pos - chosen) == 68
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 2
                and (pos - c) == 236
            ]
            if dests:
                chosen = max(dests)

    # 8cf307. pe2pkg file30406@645: best==3 nc5 pre220 jf('L',)
    #         ('Mtip', 3, 232) → ('Mtip', 7, 224)
    if (
        best_len == 3
        and len(cands) == R307_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R307_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 232
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 7
                and (pos - c) == 224
            ]
            if dests:
                chosen = max(dests)

    # 8cf308. pe2pkg file30300@410: best==2 nc102 pre0 jf('M', 17)
    #         ('Mtip', 7, 209) → ('Mtip', 2, 144)
    if (
        best_len == 2
        and len(cands) == R308_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R308_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 7
            and (pos - chosen) == 209
            and any(
                info[0] == "M"
                and info[1] == 17
                and own + 17 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 144
            ]
            if dests:
                chosen = max(dests)

    # 8cf309. pe2pkg file30300@413: best==3 nc78 pre12 jf('L',)
    #         ('Mtip', 3, 252) → ('Mmid', 2, 1, 144)
    if (
        best_len == 3
        and len(cands) == R309_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R309_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 252
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == 1
                and (pos - c) == 144
            ]
            if dests:
                chosen = max(dests)

    # 8cf310. pe2pkg file30300@417: best==3 nc8 pre231 jf('L',)
    #         ('Mmid', 17, 15, 78) → ('Mtip', 3, 144)
    if (
        best_len == 3
        and len(cands) == R310_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R310_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 17
            and chosen - o == 15
            and (pos - chosen) == 78
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 144
            ]
            if dests:
                chosen = max(dests)

    # 8cf311. pe2pkg file30300@428: best==4 nc2 pre0 jf('M', 7)
    #         ('Mtip', 8, 144) → ('L', 152)
    if (
        best_len == 4
        and len(cands) == R311_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R311_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 8
            and (pos - chosen) == 144
            and any(
                info[0] == "M"
                and info[1] == 7
                and own + 7 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 152
            ]
            if dests:
                chosen = max(dests)

    # 8cf312. pe2pkg file30300@477: best==3 nc73 pre27 jf('L',)
    #         ('Mmid', 10, 3, 128) → ('Mmid', 4, 1, 4)
    if (
        best_len == 3
        and len(cands) == R312_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R312_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 10
            and chosen - o == 3
            and (pos - chosen) == 128
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 4
            ]
            if dests:
                chosen = max(dests)

    # 8cf313. pe2pkg file30300@485: best==3 nc75 pre47 jf('L',)
    #         ('Mmid', 17, 15, 77) → ('Mtip', 7, 28)
    if (
        best_len == 3
        and len(cands) == R313_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R313_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 17
            and chosen - o == 15
            and (pos - chosen) == 77
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 7
                and (pos - c) == 28
            ]
            if dests:
                chosen = max(dests)

    # 8cf314. pe2pkg file30300@529: best==3 nc60 pre48 jf('L',)
    #         ('Mtip', 3, 248) → ('Mtip', 3, 116)
    if (
        best_len == 3
        and len(cands) == R314_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R314_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 248
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 116
            ]
            if dests:
                chosen = max(dests)

    # 8cf315. pe2pkg file30300@532: best==3 nc7 pre0 jf('M', 3)
    #         ('Mmid', 17, 13, 195) → ('Mtip', 17, 208)
    if (
        best_len == 3
        and len(cands) == R315_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R315_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 17
            and chosen - o == 13
            and (pos - chosen) == 195
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 17
                and (pos - c) == 208
            ]
            if dests:
                chosen = max(dests)

    # 8cf316. pe2pkg file30300@580: best==4 nc2 pre0 jf('M', 4)
    #         ('Mtip', 17, 256) → ('Mtip', 3, 12)
    if (
        best_len == 4
        and len(cands) == R316_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R316_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 17
            and (pos - chosen) == 256
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 12
            ]
            if dests:
                chosen = max(dests)

    # 8cf317. pe2pkg file30300@585: best==3 nc59 pre8 jf('L',)
    #         ('Mmid', 9, 4, 32) → ('Mmid', 11, 8, 88)
    if (
        best_len == 3
        and len(cands) == R317_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R317_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 9
            and chosen - o == 4
            and (pos - chosen) == 32
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 11
                and c - owners.get(c, c) == 8
                and (pos - c) == 88
            ]
            if dests:
                chosen = max(dests)

    # 8cf318. pe2pkg file30200@357: best==3 nc59 pre8 jf('L',)
    #         ('Mtip', 3, 172) → ('Mtip', 11, 108)
    if (
        best_len == 3
        and len(cands) == R318_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R318_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 172
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 11
                and (pos - c) == 108
            ]
            if dests:
                chosen = max(dests)

    # 8cf319. pe2pkg file20600@259: best==2 nc4 pre131 jf('M', 3)
    #         ('Mtip', 3, 6) → ('Mmid', 3, 1, 54)
    if (
        best_len == 2
        and len(cands) == R319_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R319_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 6
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 1
                and (pos - c) == 54
            ]
            if dests:
                chosen = max(dests)

    # 8cf320. pe2pkg file20600@289: best==2 nc2 pre131 jf('M', 7)
    #         ('Mtip', 12, 196) → ('L', 216)
    if (
        best_len == 2
        and len(cands) == R320_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R320_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 12
            and (pos - chosen) == 196
            and any(
                info[0] == "M"
                and info[1] == 7
                and own + 7 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 216
            ]
            if dests:
                chosen = max(dests)

    # 8cf321. pe2pkg file20600@314: best==3 nc6 pre104 jf('L',)
    #         ('Mmid', 3, 2, 56) → ('L', 126)
    if (
        best_len == 3
        and len(cands) == R321_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R321_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 2
            and (pos - chosen) == 56
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 126
            ]
            if dests:
                chosen = max(dests)

    # 8cf322. pe2pkg file20600@388: best==2 nc3 pre0 jf('M', 8)
    #         ('Mmid', 5, 4, 46) → ('Mmid', 3, 2, 86)
    if (
        best_len == 2
        and len(cands) == R322_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R322_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 4
            and (pos - chosen) == 46
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 2
                and (pos - c) == 86
            ]
            if dests:
                chosen = max(dests)

    # 8cf323. pe2pkg file20600@390: best==3 nc5 pre86 jf('M', 2)
    #         ('Mtip', 8, 10) → ('Mtip', 3, 176)
    if (
        best_len == 3
        and len(cands) == R323_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R323_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 8
            and (pos - chosen) == 10
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 176
            ]
            if dests:
                chosen = max(dests)

    # 8cf324. pe2pkg file20600@402: best==5 nc2 pre91 jf('M', 3)
    #         ('Mmid', 7, 4, 116) → ('Mmid', 9, 4, 28)
    if (
        best_len == 5
        and len(cands) == R324_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R324_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 7
            and chosen - o == 4
            and (pos - chosen) == 116
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 9
                and c - owners.get(c, c) == 4
                and (pos - c) == 28
            ]
            if dests:
                chosen = max(dests)

    # 8cf325. pe2pkg file20600@438: best==3 nc13 pre0 jf('M', 8)
    #         ('Mtip', 9, 20) → ('Mmid', 5, 2, 88)
    if (
        best_len == 3
        and len(cands) == R325_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R325_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 9
            and (pos - chosen) == 20
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 2
                and (pos - c) == 88
            ]
            if dests:
                chosen = max(dests)

    # 8cf326. pe2pkg file20600@442: best==3 nc9 pre110 jf('L',)
    #         ('Mtip', 8, 12) → ('Mmid', 5, 1, 28)
    if (
        best_len == 3
        and len(cands) == R326_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R326_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 8
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 1
                and (pos - c) == 28
            ]
            if dests:
                chosen = max(dests)

    # 8cf327. pe2pkg file20600@454: best==12 nc2 pre103 jf('M', 7)
    #         ('Mmid', 8, 6, 228) → ('Mmid', 5, 2, 104)
    if (
        best_len == 12
        and len(cands) == R327_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R327_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == 6
            and (pos - chosen) == 228
            and any(
                info[0] == "M"
                and info[1] == 7
                and own + 7 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 2
                and (pos - c) == 104
            ]
            if dests:
                chosen = max(dests)

    # 8cf328. pe2pkg file20600@486: best==3 nc13 pre104 jf('M', 4)
    #         ('Mtip', 11, 16) → ('Mmid', 5, 2, 136)
    if (
        best_len == 3
        and len(cands) == R328_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R328_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 11
            and (pos - chosen) == 16
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 2
                and (pos - c) == 136
            ]
            if dests:
                chosen = max(dests)

    # 8cf329. pe2pkg file50119@360: best==4 nc6 pre132 jf('L',)
    #         ('Mmid', 8, 4, 164) → ('Mmid', 5, 1, 200)
    if (
        best_len == 4
        and len(cands) == R329_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R329_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 8
            and chosen - o == 4
            and (pos - chosen) == 164
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 5
                and c - owners.get(c, c) == 1
                and (pos - c) == 200
            ]
            if dests:
                chosen = max(dests)

    # 8cf330. pe2pkg file50119@407: best==2 nc2 pre2 jf('M', 2)
    #         ('Mmid', 3, 2, 56) → ('Mmid', 3, 2, 92)
    if (
        best_len == 2
        and len(cands) == R330_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R330_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 2
            and (pos - chosen) == 56
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 2
                and (pos - c) == 92
            ]
            if dests:
                chosen = max(dests)

    # 8cf331. pe2pkg file50119@418: best==2 nc3 pre20 jf('L',)
    #         ('Mtip', 2, 182) → ('Mtip', 2, 194)
    if (
        best_len == 2
        and len(cands) == R331_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R331_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 182
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 194
            ]
            if dests:
                chosen = max(dests)

    # 8cf332. pe2pkg file50119@427: best==3 nc5 pre98 jf('L',)
    #         ('Mmid', 4, 1, 66) → ('Mmid', 8, 5, 230)
    if (
        best_len == 3
        and len(cands) == R332_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R332_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 4
            and chosen - o == 1
            and (pos - chosen) == 66
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 5
                and (pos - c) == 230
            ]
            if dests:
                chosen = max(dests)

    # 8cf333. pe2pkg file50119@452: best==2 nc4 pre132 jf('M', 3)
    #         ('Mmid', 6, 2, 10) → ('Mtip', 2, 130)
    if (
        best_len == 2
        and len(cands) == R333_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R333_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == 2
            and (pos - chosen) == 10
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 130
            ]
            if dests:
                chosen = max(dests)

    # 8cf334. pe2pkg file50119@457: best==2 nc3 pre42 jf('L',)
    #         ('Mmid', 6, 1, 16) → ('Mmid', 3, 2, 136)
    if (
        best_len == 2
        and len(cands) == R334_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R334_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == 1
            and (pos - chosen) == 16
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 2
                and (pos - c) == 136
            ]
            if dests:
                chosen = max(dests)

    # 8cf335. pe2pkg file50119@480: best==2 nc2 pre144 jf('L',)
    #         ('Mtip', 2, 68) → ('Mmid', 2, 1, 72)
    if (
        best_len == 2
        and len(cands) == R335_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R335_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 68
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == 1
                and (pos - c) == 72
            ]
            if dests:
                chosen = max(dests)

    # 8cf336. pe2pkg file50119@485: best==2 nc2 pre8 jf('L',)
    #         ('Mtip', 3, 36) → ('Mmid', 2, 1, 128)
    if (
        best_len == 2
        and len(cands) == R336_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R336_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 36
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 2
                and c - owners.get(c, c) == 1
                and (pos - c) == 128
            ]
            if dests:
                chosen = max(dests)

    # 8cf337. pe2pkg file50119@502: best==2 nc6 pre134 jf('L',)
    #         ('Mtip', 2, 180) → ('Mtip', 2, 50)
    if (
        best_len == 2
        and len(cands) == R337_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R337_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 180
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 50
            ]
            if dests:
                chosen = max(dests)

    # 8cf338. pe2pkg file50119@524: best==2 nc3 pre36 jf('M', 2)
    #         ('Mtip', 4, 92) → ('Mtip', 2, 184)
    if (
        best_len == 2
        and len(cands) == R338_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R338_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 92
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 184
            ]
            if dests:
                chosen = max(dests)

    # 8cf339. pe2pkg file30406/07@693: best==3 nc3 pre44
    #         ('Mtip', 3, 204) → ('Mmid', 15, 10, 78)
    if (
        best_len == R339_BEST
        and len(cands) == R339_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R339_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 204
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 15
                and c - owners.get(c, c) == 10
                and (pos - c) == 78
            ]
            if dests:
                chosen = max(dests)

    # 8cf340. pe2pkg file50146@644: best==2 nc4 pre20 jf('M', 3)
    #         ('Mmid', 10, 6, 148) → ('Mtip', 5, 184)
    if (
        best_len == R340_BEST
        and len(cands) == R340_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R340_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 10
            and chosen - o == 6
            and (pos - chosen) == 148
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == 184
            ]
            if dests:
                chosen = max(dests)

    # 8cf341. pe2pkg file30406/07@697: best==3 nc82 pre20 jf('L',)
    #         ('Mmid', 6, 3, 8) → ('Mmid', 4, 1, 148)
    if (
        best_len == R341_BEST
        and len(cands) == R341_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R341_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 6
            and chosen - o == 3
            and (pos - chosen) == 8
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 1
                and (pos - c) == 148
            ]
            if dests:
                chosen = max(dests)

    # 8cf342. pe2pkg file20600@493: best==3 nc2 pre131 jf('M', 3)
    #         ('L', 222) → ('Mmid', 7, 2, 44)
    if (
        best_len == R342_BEST
        and len(cands) == R342_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R342_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) == 222
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 7
                and c - owners.get(c, c) == 2
                and (pos - c) == 44
            ]
            if dests:
                chosen = max(dests)

    # 8cf343. pe2pkg file50119/20/21@530: best==2 nc7 pre22 jf('L',)
    #         ('Mtip', 2, 28) → ('Mmid', 4, 3, 31)
    if (
        best_len == R343_BEST
        and len(cands) == R343_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R343_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 28
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 4
                and c - owners.get(c, c) == 3
                and (pos - c) == 31
            ]
            if dests:
                chosen = max(dests)

    # 8cf344. pe2pkg file50146@646: best==3 nc2 pre32 jf('M', 2)
    #         ('Mmid', 2, 1, 57) → ('Mtip', 2, 65)
    if (
        best_len == R344_BEST
        and len(cands) == R344_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R344_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 2
            and chosen - o == 1
            and (pos - chosen) == 57
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 65
            ]
            if dests:
                chosen = max(dests)

    # 8cf345. pe2pkg file30406/07@700: best==2 nc2 pre0 jf('M', 3)
    #         ('Mmid', 15, 3, 92) → ('Mmid', 17, 16, 96)
    if (
        best_len == R345_BEST
        and len(cands) == R345_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R345_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 15
            and chosen - o == 3
            and (pos - chosen) == 92
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 17
                and c - owners.get(c, c) == 16
                and (pos - c) == 96
            ]
            if dests:
                chosen = max(dests)

    # 8cf346. pe2pkg file20600@526: best==17 nc2 pre81 jf('L',)
    #         ('Mtip', 3, 40) → ('Mtip', 17, 20)
    if (
        best_len == R346_BEST
        and len(cands) == R346_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R346_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 40
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 17
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf347. pe2pkg file50119/20/21@550: best==2 nc8 pre54 jf('L',)
    #         ('Mtip', 2, 20) → ('Mtip', 2, 48)
    if (
        best_len == R347_BEST
        and len(cands) == R347_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R347_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 20
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 48
            ]
            if dests:
                chosen = max(dests)

    # 8cf348. pe2pkg file30406/07@702: best==2 nc115 pre255 jf('M', 2)
    #         ('Mmid', 3, 1, 8) → ('Mmid', 6, 4, 12)
    if (
        best_len == R348_BEST
        and len(cands) == R348_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R348_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 8
            and any(
                info[0] == "M"
                and info[1] == 2
                and own + 2 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == 4
                and (pos - c) == 12
            ]
            if dests:
                chosen = max(dests)

    # 8cf349. pe2pkg file30406/07@709: best==3 nc75 pre17 jf('L',)
    #         ('Mtip', 3, 12) → ('Mmid', 6, 3, 20)
    if (
        best_len == R349_BEST
        and len(cands) == R349_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R349_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 6
                and c - owners.get(c, c) == 3
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf350. pe2pkg file50146@649: best==2 nc8 pre36 jf('M', 3)
    #         ('Mtip', 3, 3) → ('Mmid', 3, 1, 64)
    if (
        best_len == R350_BEST
        and len(cands) == R350_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R350_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 3
            and any(
                info[0] == "M"
                and info[1] == 3
                and own + 3 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 1
                and (pos - c) == 64
            ]
            if dests:
                chosen = max(dests)

    # 8cf351. pe2pkg file20600@546: best==17 nc3 pre82 jf('L',)
    #         ('Mtip', 17, 40) → ('Mtip', 17, 20)
    if (
        best_len == R351_BEST
        and len(cands) == R351_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R351_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 17
            and (pos - chosen) == 40
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 17
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf352. pe2pkg file50119/20/21@556: best==4 nc2 pre12 jf('L',)
    #         ('Mtip', 4, 60) → ('Mtip', 2, 76)
    if (
        best_len == R352_BEST
        and len(cands) == R352_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R352_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 60
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 76
            ]
            if dests:
                chosen = max(dests)

    # 8cf353. pe2pkg file30406/07@728: best==4 nc4 pre0 jf('M', 8)
    #         ('Mtip', 4, 216) → ('Mtip', 4, 200)
    if (
        best_len == R353_BEST
        and len(cands) == R353_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R353_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 4
            and (pos - chosen) == 216
            and any(
                info[0] == "M"
                and info[1] == 8
                and own + 8 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 200
            ]
            if dests:
                chosen = max(dests)

    # 8cf354. pe2pkg file20600@563: best==2 nc3 pre130 jf('M', 17)
    #         ('Mtip', 2, 40) → ('Mtip', 2, 20)
    if (
        best_len == R354_BEST
        and len(cands) == R354_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R354_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 40
            and any(
                info[0] == "M"
                and info[1] == 17
                and own + 17 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf355. pe2pkg file50119/20/21@576: best==6 nc2 pre134 jf('L',)
    #         ('Mtip', 6, 136) → ('L', 228)
    if (
        best_len == R355_BEST
        and len(cands) == R355_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R355_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 6
            and (pos - chosen) == 136
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "L"
                and (pos - c) == 228
            ]
            if dests:
                chosen = max(dests)

    # 8cf356. pe2pkg file30406/07@736: best==4 nc3 pre0 jf('M', 4)
    #         ('Mtip', 8, 24) → ('Mtip', 4, 32)
    if (
        best_len == R356_BEST
        and len(cands) == R356_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R356_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 8
            and (pos - chosen) == 24
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 4
                and (pos - c) == 32
            ]
            if dests:
                chosen = max(dests)

    # 8cf357. pe2pkg file50146@664: best==4 nc4 pre150 jf('L',)
    #         ('Mmid', 5, 1, 12) → ('Mmid', 8, 4, 92)
    if (
        best_len == R357_BEST
        and len(cands) == R357_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R357_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 5
            and chosen - o == 1
            and (pos - chosen) == 12
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 8
                and c - owners.get(c, c) == 4
                and (pos - c) == 92
            ]
            if dests:
                chosen = max(dests)

    # 8cf358. pe2pkg file20600@566: best==17 nc4 pre83 jf('L',)
    #         ('Mtip', 17, 40) → ('Mtip', 17, 20)
    if (
        best_len == R358_BEST
        and len(cands) == R358_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R358_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 17
            and (pos - chosen) == 40
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 17
                and (pos - c) == 20
            ]
            if dests:
                chosen = max(dests)

    # 8cf359. pe2pkg file50119/20/21@604: best==2 nc2 pre0 jf('M', 4)
    #         ('Mtip', 2, 94) → ('Mtip', 2, 202)
    if (
        best_len == R359_BEST
        and len(cands) == R359_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R359_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 2
            and (pos - chosen) == 94
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and (pos - c) == 202
            ]
            if dests:
                chosen = max(dests)

    # 8cf360. pe2pkg file30406/07@837: best==3 nc5 pre228 jf('L',)
    #         ('Mtip', 3, 4) → ('Mtip', 3, 200)
    if (
        best_len == R360_BEST
        and len(cands) == R360_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R360_PRE
    ):
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == 3
            and (pos - chosen) == 4
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 3
                and (pos - c) == 200
            ]
            if dests:
                chosen = max(dests)

    # 8cf361. pe2pkg file30406/07@842: best==2 nc7 pre242 jf('L',)
    #         ('Mmid', 3, 1, 8) → ('Mmid', 3, 1, 200)
    if (
        best_len == R361_BEST
        and len(cands) == R361_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R361_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and chosen - o == 1
            and (pos - chosen) == 8
            and any(
                info[0] == "L" and own + 1 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == 3
                and c - owners.get(c, c) == 1
                and (pos - c) == 200
            ]
            if dests:
                chosen = max(dests)

    # 8cf362. pe2pkg file50146@680: best==4 nc3 pre150 jf('M', 4)
    #         ('Mmid', 10, 6, 184) → ('Mtip', 5, 220)
    if (
        best_len == R362_BEST
        and len(cands) == R362_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R362_PRE
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 10
            and chosen - o == 6
            and (pos - chosen) == 184
            and any(
                info[0] == "M"
                and info[1] == 4
                and own + 4 == pos
                for own, info in token_starts.items()
            )
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 5
                and (pos - c) == 220
            ]
            if dests:
                chosen = max(dests)

    # 8cf363. pe2pkg file30300/01@604: best==4 nc3 pre0 jf('M', 2)
    #         cands T4@12, T4@24, T3@36 — pick T4@24 → retail T3@36.
    if (
        best_len == R363_BEST
        and len(cands) == R363_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R363_PRE
        and any(
            info[0] == "M" and info[1] == 2 and own + 2 == pos
            for own, info in token_starts.items()
        )
    ):
        feats = []
        for c in cands:
            tip = token_starts.get(c)
            if tip is None or tip[0] != "M":
                feats = []
                break
            feats.append((tip[1], pos - c, c))
        if feats:
            feats.sort(key=lambda x: x[1])
            if (
                tuple(f[1] for f in feats) == R363_AGES
                and tuple(f[0] for f in feats) == R363_PLS
            ):
                chosen = feats[2][2]

    # 8cf364. pe2pkg file50146@688: best==2 nc2 pre0 jf('M', 4)
    #         mid into+1 of M3 age==160 → M2 tip age==180.
    if (
        best_len == R364_BEST
        and len(cands) == R364_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R364_PRE
        and any(
            info[0] == "M" and info[1] == 4 and own + 4 == pos
            for own, info in token_starts.items()
        )
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == R364_SRC_OWNER_PL
            and chosen - o == R364_SRC_INTO
            and (pos - chosen) == R364_SRC_AGE
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == R364_DST_PL
                and (pos - c) == R364_DST_AGE
            ]
            if dests:
                chosen = max(dests)

    # 8cf365. pe2pkg file30300/01@620 + file30200/01@392:
    #         best==4 nc31 pre255 jf('M', 4)
    #         M17 mid into+13 age==231 → M17 mid into+5 age==239.
    if (
        best_len == R365_BEST
        and len(cands) == R365_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R365_PRE
        and any(
            info[0] == "M" and info[1] == 4 and own + 4 == pos
            for own, info in token_starts.items()
        )
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == R365_SRC_OWNER_PL
            and chosen - o == R365_SRC_INTO
            and (pos - chosen) == R365_SRC_AGE
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == R365_DST_OWNER_PL
                and c - owners.get(c, c) == R365_DST_INTO
                and (pos - c) == R365_DST_AGE
            ]
            if dests:
                chosen = max(dests)

    # 8cf366. pe2pkg file50119/20/21@632: best==2 nc3 pre150,
    #         M2 tips ages 28/122/230 — pick@28 → tip@122.
    if (
        best_len == R366_BEST
        and len(cands) == R366_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R366_PRE
    ):
        ages: dict[int, int] = {}
        ok = True
        for c in cands:
            tip = token_starts.get(c)
            if tip is None or tip[0] != "M" or tip[1] != R366_PL:
                ok = False
                break
            ages[pos - c] = c
        if ok and set(ages) == set(R366_AGES) and (pos - chosen) == R366_SRC_AGE:
            chosen = ages[R366_DST_AGE]

    # 8cf367. pe2pkg file20600@598: best==3 nc13 pre109 jf('L',)
    #         M17 mid into+4 age==28 → M3 tip age==156.
    if (
        best_len == R367_BEST
        and len(cands) == R367_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R367_PRE
        and any(
            info[0] == "L" and own + 1 == pos
            for own, info in token_starts.items()
        )
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == R367_SRC_OWNER_PL
            and chosen - o == R367_SRC_INTO
            and (pos - chosen) == R367_SRC_AGE
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == R367_DST_PL
                and (pos - c) == R367_DST_AGE
            ]
            if dests:
                chosen = max(dests)

    # 8cf368. pe2pkg file30300/01@625 + file30200/01@397:
    #         best==7 nc16 pre136 jf('L',)
    #         M17 mid into+10 age==222 → M9 tip age==112.
    if (
        best_len == R368_BEST
        and len(cands) == R368_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R368_PRE
        and any(
            info[0] == "L" and own + 1 == pos
            for own, info in token_starts.items()
        )
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == R368_SRC_OWNER_PL
            and chosen - o == R368_SRC_INTO
            and (pos - chosen) == R368_SRC_AGE
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == R368_DST_PL
                and (pos - c) == R368_DST_AGE
            ]
            if dests:
                chosen = max(dests)

    # 8cf369. pe2pkg file20600@616: best==5 nc5 pre117 jf('L',)
    #         M17 mid into+2 age==68 → M17 mid into+2 age==48.
    if (
        best_len == R369_BEST
        and len(cands) == R369_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R369_PRE
        and any(
            info[0] == "L" and own + 1 == pos
            for own, info in token_starts.items()
        )
    ):
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        if (
            token_starts.get(chosen) is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == R369_SRC_OWNER_PL
            and chosen - o == R369_SRC_INTO
            and (pos - chosen) == R369_SRC_AGE
        ):
            dests = [
                c
                for c in cands
                if token_starts.get(c) is None
                and token_starts.get(owners.get(c, c)) is not None
                and token_starts[owners.get(c, c)][0] == "M"
                and token_starts[owners.get(c, c)][1] == R369_DST_OWNER_PL
                and c - owners.get(c, c) == R369_DST_INTO
                and (pos - c) == R369_DST_AGE
            ]
            if dests:
                chosen = max(dests)

    # 8cf370. pe2pkg file50119/20/21@648: best==2 nc2 pre0 jf('M', 8)
    #         M2 tip age==230 → M2 tip age==28.
    if (
        best_len == R370_BEST
        and len(cands) == R370_NC
        and data is not None
        and pos > 0
        and data[pos - 1] == R370_PRE
        and any(
            info[0] == "M" and info[1] == 8 and own + 8 == pos
            for own, info in token_starts.items()
        )
    ):
        ages: dict[int, int] = {}
        ok = True
        for c in cands:
            tip = token_starts.get(c)
            if tip is None or tip[0] != "M" or tip[1] != R370_PL:
                ok = False
                break
            ages[pos - c] = c
        if ok and set(ages) == set(R370_AGES) and (pos - chosen) == R370_SRC_AGE:
            chosen = ages[R370_DST_AGE]

    # 8cg. best==2: young M3 mid → M2 tip age in [40, 48)
    #      (file444000@596: tip@554 over mid 594 of M3@593).
    if best_len == 2:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] == 3
            and (pos - chosen) < B2_M3MID_TO_OLD_M2TIP_MID_MAX_AGE
        ):
            tips = [
                c
                for c in cands
                if token_starts.get(c) is not None
                and token_starts[c][0] == "M"
                and token_starts[c][1] == 2
                and B2_M3MID_TO_OLD_M2TIP_TIP_MIN_AGE
                <= (pos - c)
                < B2_M3MID_TO_OLD_M2TIP_TIP_MAX_AGE
            ]
            if tips:
                chosen = max(tips)

    # 8ch. best ≥ 14: old lit tip → newest mid into +2 of same-len long
    #      (file29@2626: mid 2594 of M17@2592 over L@2562; M17 offΔ=32 family).
    if best_len >= LIT_TO_SAMELEN_INTO2_MIN_BEST:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "L"
            and (pos - chosen) >= LIT_TO_SAMELEN_INTO2_LIT_MIN_AGE
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1] == best_len
                and ref - owners.get(ref, ref) == LIT_TO_SAMELEN_INTO2
                and (pos - ref) <= LIT_TO_SAMELEN_INTO2_MID_MAX_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8ci. best ≥ 3: deep/old long mid → newest long mid into +2
    #      (file29@2745: into2 age32 over into8 age128).
    if best_len >= DEEP_LONG_MID_TO_INTO2_MIN_BEST:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        if (
            tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= JUST_FINISHED_LONG_MIN_PL
            and (chosen - o) >= DEEP_LONG_MID_TO_INTO2_MIN_INTO
            and (pos - chosen) >= DEEP_LONG_MID_TO_INTO2_MIN_AGE
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1]
                >= JUST_FINISHED_LONG_MIN_PL
                and ref - owners.get(ref, ref) == LIT_TO_SAMELEN_INTO2
                and (pos - ref) <= DEEP_LONG_MID_TO_INTO2_TARGET_MAX_AGE
            ]
            if mids:
                chosen = max(mids)

    # 8cj. best ≥ 14: just-finished medium pl ∈ [8, 13] → PE
    #      (file29@2754: pe of M9@2745; file20328@488: pe of M8@480).
    if best_len >= JUST_FIN_MED_PE_MIN_BEST:
        for o, info in token_starts.items():
            if info[0] != "M":
                continue
            pl = info[1]
            if pl < JUST_FIN_MED_PE_MIN_PL or pl > JUST_FIN_MED_PE_MAX_PL:
                continue
            if o + pl != pos:
                continue
            pe = o + pl - 1
            if pe in cands:
                chosen = pe
                break

    # 8ck. best ≥ 14: same-len long MS tip age == 30 → newest mid of
    #      that phrase (file30101@1100 into1; @1130 into3 of re-anchored M17).
    if best_len >= LONG_TIP_TO_NEWEST_MID_MIN_BEST:
        tip = token_starts.get(chosen)
        if (
            tip is not None
            and tip[0] == "M"
            and tip[1] == best_len
            and (pos - chosen) == LONG_TIP_TO_NEWEST_MID_TIP_AGE
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and owners.get(ref, ref) == chosen
                and ref > chosen
            ]
            if mids:
                chosen = max(mids)

    # 8cl. best in {7, 11}: just-finished long + long-mid pick → newest
    #      long mid (file30101@1147 best7; @1239 best11 age32 over age64).
    if best_len in JF_LONG_TO_NEWEST_LONG_MID_BESTS:
        o = owners.get(chosen, chosen)
        oi = token_starts.get(o)
        tip = token_starts.get(chosen)
        jf_long = any(
            info[0] == "M"
            and info[1] >= JUST_FINISHED_LONG_MIN_PL
            and own + info[1] == pos
            for own, info in token_starts.items()
        )
        if (
            jf_long
            and tip is None
            and oi is not None
            and oi[0] == "M"
            and oi[1] >= JUST_FINISHED_LONG_MIN_PL
        ):
            mids = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1]
                >= JUST_FINISHED_LONG_MIN_PL
            ]
            if mids:
                chosen = max(mids)

    # 8cm. best ≥ 14: just-finished M2 → PE
    #      (file30101@1191: pe 1190 of M2@1189 over long mid 1135).
    if best_len >= JF_M2_PE_MIN_BEST:
        for o, info in token_starts.items():
            if info[0] != "M" or info[1] != JF_M2_PE_PL:
                continue
            if o + JF_M2_PE_PL != pos:
                continue
            pe = o + JF_M2_PE_PL - 1
            if pe in cands:
                chosen = pe
                break

    # 8cn. best == 10: just-finished long → long mid age == 128, into == best
    #      (file30101@1208: 1080 of M17@1070 over AP 2nd-newest 1144).
    if best_len == JF_LONG_INTO_BEST_MID_BEST:
        jf_long = any(
            info[0] == "M"
            and info[1] >= JUST_FINISHED_LONG_MIN_PL
            and own + info[1] == pos
            for own, info in token_starts.items()
        )
        if jf_long:
            hits = [
                ref
                for ref in cands
                if token_starts.get(ref) is None
                and token_starts.get(owners.get(ref, ref)) is not None
                and token_starts[owners.get(ref, ref)][0] == "M"
                and token_starts[owners.get(ref, ref)][1]
                >= JUST_FINISHED_LONG_MIN_PL
                and (pos - ref) == JF_LONG_INTO_BEST_MID_AGE
                and (ref - owners.get(ref, ref)) == best_len
            ]
            if hits:
                chosen = hits[0]

    return chosen


def encode_lzss(data: bytes) -> bytes:
    """Frozen cascade encoder (identity experiment).

    Greedy longest match in the last 256 written bytes, force literal at ring
    write index 0xFF, then :func:`_pick_match_ref` multi_max / force-lit
    cascade. Soft-freeze: do not expand casually.

    Always: ``lzss.decode_lzss(encode_lzss(data)) == data``.
    For decoder-only repack, use ``lzss.encode_lzss`` instead.
    """
    n = len(data)
    if n == 0:
        return _pack_tokens([("EOS",)])

    # head[b] = most recent absolute index with data[i]==b; prev[i] = older.
    head = [-1] * 256
    prev = [-1] * n
    # out_pos -> ('L',) or ('M', match_length) for phrase-start tie-breaks.
    token_starts: dict[int, tuple] = {}
    # out_pos -> absolute start of the token that wrote that byte.
    owners: dict[int, int] = {}

    def insert(i: int) -> None:
        b = data[i]
        prev[i] = head[b]
        head[b] = i

    tokens: list[tuple] = []
    pos = 0

    while pos < n:
        # Retail forces a literal when the next write ring index is 0xFF.
        # (Match *starts* at ring 0xFF are already unencodable as offset 0/EOS;
        # this is a separate write-cursor quirk that shows up heavily in CLUTs.)
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

            # not_max: sole best is old L tip, retail prefers shorter M4 tip
            # age == 24 len 4 (file30406/07@324). Before sole-old-L force-lit.
            if (
                sole is not None
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "L"
                and (pos - sole) >= NOT_MAX_OLD_LIT_TO_M4TIP24_LIT_MIN_AGE
            ):
                alt = None
                p = head[data[pos]]
                while p >= window_start:
                    if _offset_for_ref(p) is not None:
                        tip = token_starts.get(p)
                        if (
                            tip is not None
                            and tip[0] == "M"
                            and tip[1] == NOT_MAX_OLD_LIT_TO_M4TIP24_TIP_PL
                            and (pos - p) == NOT_MAX_OLD_LIT_TO_M4TIP24_TIP_AGE
                        ):
                            ml = _match_length(
                                data, pos, p, NOT_MAX_OLD_LIT_TO_M4TIP24_EMIT_LEN
                            )
                            if ml >= NOT_MAX_OLD_LIT_TO_M4TIP24_EMIT_LEN:
                                alt = p
                                break
                    p = prev[p]
                if alt is not None:
                    emit_len = NOT_MAX_OLD_LIT_TO_M4TIP24_EMIT_LEN
                    off = _offset_for_ref(alt)
                    assert off is not None
                    tokens.append(("M", off, emit_len))
                    token_starts[pos] = ("M", emit_len)
                    for i in range(emit_len):
                        owners[pos + i] = pos
                    cut = pos + emit_len - DICT_SIZE
                    if cut > 0:
                        token_starts = {
                            k: v for k, v in token_starts.items() if k >= cut
                        }
                        owners = {k: v for k, v in owners.items() if k >= cut}
                    end = pos + emit_len
                    while pos < end:
                        insert(pos)
                        pos += 1
                    continue

            # not_max: sole best old L tip, retail M3 tip age == 239 len 3
            # (file30200/01@340). Before sole-old-L force-lit.
            if (
                sole is not None
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "L"
                and (pos - sole) >= NOT_MAX_OLD_LIT_TO_M3TIP239_LIT_MIN_AGE
            ):
                alt = None
                p = head[data[pos]]
                while p >= window_start:
                    if _offset_for_ref(p) is not None:
                        tip = token_starts.get(p)
                        if (
                            tip is not None
                            and tip[0] == "M"
                            and tip[1] == NOT_MAX_OLD_LIT_TO_M3TIP239_TIP_PL
                            and (pos - p) == NOT_MAX_OLD_LIT_TO_M3TIP239_TIP_AGE
                        ):
                            ml = _match_length(
                                data, pos, p, NOT_MAX_OLD_LIT_TO_M3TIP239_EMIT_LEN
                            )
                            if ml >= NOT_MAX_OLD_LIT_TO_M3TIP239_EMIT_LEN:
                                alt = p
                                break
                    p = prev[p]
                if alt is not None:
                    emit_len = NOT_MAX_OLD_LIT_TO_M3TIP239_EMIT_LEN
                    off = _offset_for_ref(alt)
                    assert off is not None
                    tokens.append(("M", off, emit_len))
                    token_starts[pos] = ("M", emit_len)
                    for i in range(emit_len):
                        owners[pos + i] = pos
                    cut = pos + emit_len - DICT_SIZE
                    if cut > 0:
                        token_starts = {
                            k: v for k, v in token_starts.items() if k >= cut
                        }
                        owners = {k: v for k, v in owners.items() if k >= cut}
                    end = pos + emit_len
                    while pos < end:
                        insert(pos)
                        pos += 1
                    continue

            # Retail sometimes emits a literal even when a match exists: sole
            # candidate is a very old literal source (file30400@352).
            if (
                sole is not None
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "L"
                and (pos - sole) >= SOLE_OLD_LIT_FORCE_AGE
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole L tip, best_len == 2, age ≥ 240 → force lit
            # (file104900/2@604 family; pe2pkg-safe vs un-gated age≥240).
            if (
                sole is not None
                and best_len == SOLE_LIT_BEST2_FORCE_BEST
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "L"
                and (pos - sole) >= SOLE_LIT_BEST2_FORCE_AGE
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole old M2 tip (age ≥ 242) at best_len == 2 → force lit
            # (file102000/1@402; pe2pkg file30500@2920).
            if (
                sole is not None
                and best_len == SOLE_OLD_M2TIP_FORCE_BEST
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "M"
                and token_starts[sole][1] == SOLE_OLD_M2TIP_FORCE_PL
                and (pos - sole) >= SOLE_OLD_M2TIP_FORCE_AGE
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole M2 tip age == 244, best_len == 4, pre == 0, just-finished M4
            # → force lit (pe2pkg file30300/01@512).
            if (
                sole is not None
                and best_len == SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_BEST
                and pos > 0
                and data[pos - 1] == SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_PRE
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "M"
                and token_starts[sole][1] == SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_PL
                and (pos - sole) == SOLE_M2TIP244_BEST4_PRE0_JFM4_FORCE_AGE
                and any(
                    info[0] == "M"
                    and info[1] == 4
                    and own + 4 == pos
                    for own, info in token_starts.items()
                )
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole M2 tip age == 252, best_len == 4, pre == 40, just-finished M4
            # → force lit (pe2pkg file30200/01@368).
            if (
                sole is not None
                and best_len == SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_BEST
                and pos > 0
                and data[pos - 1] == SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_PRE
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "M"
                and token_starts[sole][1] == SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_PL
                and (pos - sole) == SOLE_M2TIP252_BEST4_PRE40_JFM4_FORCE_AGE
                and any(
                    info[0] == "M"
                    and info[1] == 4
                    and own + 4 == pos
                    for own, info in token_starts.items()
                )
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole M3 tip age == 252, best_len == 3, pre == 163, just-finished M2
            # → force lit (pe2pkg file50146@692).
            if (
                sole is not None
                and best_len == SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_BEST
                and pos > 0
                and data[pos - 1] == SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_PRE
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "M"
                and token_starts[sole][1] == SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_PL
                and (pos - sole) == SOLE_M3TIP252_BEST3_PRE163_JFM2_FORCE_AGE
                and any(
                    info[0] == "M"
                    and info[1] == 2
                    and own + 2 == pos
                    for own, info in token_starts.items()
                )
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole M4 tip age == 244, best_len == 4, pre == 134, just-finished L
            # → force lit (pe2pkg file50146@696).
            if (
                sole is not None
                and best_len == SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_BEST
                and pos > 0
                and data[pos - 1] == SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_PRE
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "M"
                and token_starts[sole][1] == SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_PL
                and (pos - sole) == SOLE_M4TIP244_BEST4_PRE134_JFL_FORCE_AGE
                and any(
                    info[0] == "L" and own + 1 == pos
                    for own, info in token_starts.items()
                )
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole M4 mid into +1 age == 244, best_len == 3, pre == 174,
            # just-finished L → force lit (pe2pkg file50146@697).
            if (
                sole is not None
                and best_len == SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_BEST
                and pos > 0
                and data[pos - 1] == SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_PRE
                and any(
                    info[0] == "L" and own + 1 == pos
                    for own, info in token_starts.items()
                )
            ):
                o = owners.get(sole, sole)
                oi = token_starts.get(o)
                if (
                    token_starts.get(sole) is None
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1] == SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_OWNER_PL
                    and sole - o == SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_INTO
                    and (pos - sole) == SOLE_M4INTO1_244_BEST3_PRE174_JFL_FORCE_AGE
                ):
                    tokens.append(("L", data[pos]))
                    token_starts[pos] = ("L",)
                    owners[pos] = pos
                    insert(pos)
                    pos += 1
                    continue

            # Sole M10 tip age == 244, best_len == 10, pre == 32, just-finished M7
            # → force lit (pe2pkg file30300/01@548).
            if (
                sole is not None
                and best_len == SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_BEST
                and pos > 0
                and data[pos - 1] == SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_PRE
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "M"
                and token_starts[sole][1]
                == SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_PL
                and (pos - sole) == SOLE_M10TIP244_BEST10_PRE32_JFM7_FORCE_AGE
                and any(
                    info[0] == "M"
                    and info[1] == 7
                    and own + 7 == pos
                    for own, info in token_starts.items()
                )
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole M5 mid into +3 age == 252, best_len == 4, pre == 40,
            # just-finished M4 → force lit (pe2pkg file30300/01@596).
            if (
                sole is not None
                and best_len == SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_BEST
                and pos > 0
                and data[pos - 1]
                == SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_PRE
            ):
                o = owners.get(sole, sole)
                oi = token_starts.get(o)
                if (
                    token_starts.get(sole) is None
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1]
                    == SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_OWNER_PL
                    and sole - o
                    == SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_INTO
                    and (pos - sole)
                    == SOLE_M5INTO3_AGE252_BEST4_PRE40_JFM4_FORCE_AGE
                    and any(
                        info[0] == "M"
                        and info[1] == 4
                        and own + 4 == pos
                        for own, info in token_starts.items()
                    )
                ):
                    tokens.append(("L", data[pos]))
                    token_starts[pos] = ("L",)
                    owners[pos] = pos
                    insert(pos)
                    pos += 1
                    continue

            # Sole old mid (not tip), age ≥ 248, best_len == 2 → force lit
            # (file102000/1@450 age 248; file20333@323; pe2pkg file30500@2704).
            if (
                sole is not None
                and best_len == SOLE_OLD_MID_FORCE_BEST
                and token_starts.get(sole) is None
                and (pos - sole) >= SOLE_OLD_MID_FORCE_AGE
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole mid into +1 of M4, age == 250, best_len == 7 → force lit
            # (file102000/1@451).
            if sole is not None and best_len == SOLE_OLD_M4INTO1_BEST7_FORCE_BEST:
                o = owners.get(sole, sole)
                oi = token_starts.get(o)
                if (
                    token_starts.get(sole) is None
                    and (pos - sole) == SOLE_OLD_M4INTO1_BEST7_FORCE_AGE
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1] == SOLE_OLD_M4INTO1_BEST7_FORCE_OWNER_PL
                    and sole - o == SOLE_OLD_M4INTO1_BEST7_FORCE_INTO
                ):
                    tokens.append(("L", data[pos]))
                    token_starts[pos] = ("L",)
                    owners[pos] = pos
                    insert(pos)
                    pos += 1
                    continue

            # Sole mid age == 250, best_len == 3 → force lit
            # (file444000/4@819 / file403200/4@819).
            if (
                sole is not None
                and best_len == SOLE_OLD_MID_BEST3_FORCE_BEST
                and token_starts.get(sole) is None
                and (pos - sole) == SOLE_OLD_MID_BEST3_FORCE_AGE
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # Sole mid age == 248, best_len == 3 → force lit
            # (file104900/2@793: mid into+1 of M2).
            if (
                sole is not None
                and best_len == SOLE_MID_AGE248_BEST3_FORCE_BEST
                and token_starts.get(sole) is None
                and (pos - sole) == SOLE_MID_AGE248_BEST3_FORCE_AGE
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # best==2, 2-cand: just-finished M4, pre-byte == 28, pick is M3 mid
            # into +1 age == 46 → force lit (file102000/0@524 family).
            if (
                best_len == FORCE_LIT_M3INTO1_BEST
                and len(cands) == FORCE_LIT_M3INTO1_NC
                and pos > 0
                and data[pos - 1] == FORCE_LIT_M3INTO1_PRE
                and any(
                    info[0] == "M"
                    and info[1] == 4
                    and own + 4 == pos
                    for own, info in token_starts.items()
                )
            ):
                pre_pick = _pick_match_ref(
                    cands,
                    best_len=best_len,
                    token_starts=token_starts,
                    owners=owners,
                    pos=pos,
                    data=data,
                )
                o = owners.get(pre_pick, pre_pick)
                oi = token_starts.get(o)
                if (
                    token_starts.get(pre_pick) is None
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1] == 3
                    and pre_pick - o == FORCE_LIT_M3INTO1_INTO
                    and (pos - pre_pick) == FORCE_LIT_M3INTO1_MID_AGE
                ):
                    tokens.append(("L", data[pos]))
                    token_starts[pos] = ("L",)
                    owners[pos] = pos
                    insert(pos)
                    pos += 1
                    continue

            # Sole L tip age == 52, best_len == 2, just-finished L, pre == 49 →
            # force lit (file102000/0@526 family).
            if (
                sole is not None
                and best_len == SOLE_LIT52_FORCE_BEST
                and pos > 0
                and data[pos - 1] == SOLE_LIT52_FORCE_PRE
                and token_starts.get(sole) is not None
                and token_starts[sole][0] == "L"
                and (pos - sole) == SOLE_LIT52_FORCE_AGE
                and any(
                    info[0] == "L" and own + 1 == pos
                    for own, info in token_starts.items()
                )
            ):
                tokens.append(("L", data[pos]))
                token_starts[pos] = ("L",)
                owners[pos] = pos
                insert(pos)
                pos += 1
                continue

            # best==2, 3-cand: just-finished L, pre-byte == 70, pick M2 tip age
            # == 122 → force lit (file102000/0@532 family).
            if (
                best_len == FORCE_LIT_M2TIP122_BEST
                and len(cands) == FORCE_LIT_M2TIP122_NC
                and pos > 0
                and data[pos - 1] == FORCE_LIT_M2TIP122_PRE
                and any(
                    info[0] == "L" and own + 1 == pos
                    for own, info in token_starts.items()
                )
            ):
                pre_pick = _pick_match_ref(
                    cands,
                    best_len=best_len,
                    token_starts=token_starts,
                    owners=owners,
                    pos=pos,
                    data=data,
                )
                tip = token_starts.get(pre_pick)
                if (
                    tip is not None
                    and tip[0] == "M"
                    and tip[1] == 2
                    and (pos - pre_pick) == FORCE_LIT_M2TIP122_TIP_AGE
                ):
                    tokens.append(("L", data[pos]))
                    token_starts[pos] = ("L",)
                    owners[pos] = pos
                    insert(pos)
                    pos += 1
                    continue

            # best==2, 3-cand: just-finished M2, pre-byte == 0, pick M2 tip age
            # == 128 → force lit (file102000/0@554 family).
            if (
                best_len == FORCE_LIT_M2TIP128_BEST
                and len(cands) == FORCE_LIT_M2TIP128_NC
                and pos > 0
                and data[pos - 1] == FORCE_LIT_M2TIP128_PRE
                and any(
                    info[0] == "M"
                    and info[1] == 2
                    and own + 2 == pos
                    for own, info in token_starts.items()
                )
            ):
                pre_pick = _pick_match_ref(
                    cands,
                    best_len=best_len,
                    token_starts=token_starts,
                    owners=owners,
                    pos=pos,
                    data=data,
                )
                tip = token_starts.get(pre_pick)
                if (
                    tip is not None
                    and tip[0] == "M"
                    and tip[1] == 2
                    and (pos - pre_pick) == FORCE_LIT_M2TIP128_TIP_AGE
                ):
                    tokens.append(("L", data[pos]))
                    token_starts[pos] = ("L",)
                    owners[pos] = pos
                    insert(pos)
                    pos += 1
                    continue

            # not_max: sole old M4 mid into +2 age == 250, best_len ≥ 6, lit tip
            # age == 2 → emit match len 2 from that lit (file102000/1@452).
            if (
                sole is not None
                and best_len >= NOT_MAX_M4INTO2_TO_LIT2_MIN_BEST
            ):
                o = owners.get(sole, sole)
                oi = token_starts.get(o)
                lit_cand = pos - NOT_MAX_M4INTO2_TO_LIT2_LIT_AGE
                lit_tip = token_starts.get(lit_cand)
                if (
                    token_starts.get(sole) is None
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1] == NOT_MAX_M4INTO2_TO_LIT2_OWNER_PL
                    and sole - o == NOT_MAX_M4INTO2_TO_LIT2_INTO
                    and (pos - sole) == NOT_MAX_M4INTO2_TO_LIT2_MID_AGE
                    and lit_tip is not None
                    and lit_tip[0] == "L"
                    and _offset_for_ref(lit_cand) is not None
                    and _match_length(
                        data, pos, lit_cand, NOT_MAX_M4INTO2_TO_LIT2_EMIT_LEN
                    )
                    >= NOT_MAX_M4INTO2_TO_LIT2_EMIT_LEN
                ):
                    emit_len = NOT_MAX_M4INTO2_TO_LIT2_EMIT_LEN
                    off = _offset_for_ref(lit_cand)
                    assert off is not None
                    tokens.append(("M", off, emit_len))
                    token_starts[pos] = ("M", emit_len)
                    for i in range(emit_len):
                        owners[pos + i] = pos
                    cut = pos + emit_len - DICT_SIZE
                    if cut > 0:
                        token_starts = {
                            k: v for k, v in token_starts.items() if k >= cut
                        }
                        owners = {k: v for k, v in owners.items() if k >= cut}
                    end = pos + emit_len
                    while pos < end:
                        insert(pos)
                        pos += 1
                    continue

            # not_max: sole M2 tip age == 250, best_len ≥ 4, M2 tip age == 154
            # matches ≥ 2 → emit len 2 from that tip (file102000/1@454).
            if (
                sole is not None
                and best_len >= NOT_MAX_M2TIP250_TO_M2TIP154_MIN_BEST
            ):
                tip = token_starts.get(sole)
                if (
                    tip is not None
                    and tip[0] == "M"
                    and tip[1] == 2
                    and (pos - sole) == NOT_MAX_M2TIP250_TO_M2TIP154_OLD_AGE
                ):
                    alt = None
                    p = head[data[pos]]
                    while p >= window_start:
                        if (
                            _offset_for_ref(p) is not None
                            and token_starts.get(p) is not None
                            and token_starts[p][0] == "M"
                            and token_starts[p][1] == 2
                            and (pos - p)
                            == NOT_MAX_M2TIP250_TO_M2TIP154_DST_AGE
                            and _match_length(
                                data,
                                pos,
                                p,
                                NOT_MAX_M2TIP250_TO_M2TIP154_EMIT_LEN,
                            )
                            >= NOT_MAX_M2TIP250_TO_M2TIP154_EMIT_LEN
                        ):
                            if alt is None or p > alt:
                                alt = p
                        p = prev[p]
                    if alt is not None:
                        emit_len = NOT_MAX_M2TIP250_TO_M2TIP154_EMIT_LEN
                        off = _offset_for_ref(alt)
                        assert off is not None
                        tokens.append(("M", off, emit_len))
                        token_starts[pos] = ("M", emit_len)
                        for i in range(emit_len):
                            owners[pos + i] = pos
                        cut = pos + emit_len - DICT_SIZE
                        if cut > 0:
                            token_starts = {
                                k: v
                                for k, v in token_starts.items()
                                if k >= cut
                            }
                            owners = {
                                k: v for k, v in owners.items() if k >= cut
                            }
                        end = pos + emit_len
                        while pos < end:
                            insert(pos)
                            pos += 1
                        continue

            # not_max: sole M2 tip age == 140, best_len == 3, pre == 70, just-
            # finished M2 → emit len 2 from M3 mid into +1 age == 12
            # (file102000/0@546 family).
            if (
                sole is not None
                and best_len == NOT_MAX_M2TIP140_BEST
                and pos > 0
                and data[pos - 1] == NOT_MAX_M2TIP140_PRE
            ):
                tip = token_starts.get(sole)
                if (
                    tip is not None
                    and tip[0] == "M"
                    and tip[1] == 2
                    and (pos - sole) == NOT_MAX_M2TIP140_SRC_AGE
                    and any(
                        info[0] == "M"
                        and info[1] == 2
                        and own + 2 == pos
                        for own, info in token_starts.items()
                    )
                ):
                    alt = None
                    p = head[data[pos]]
                    while p >= window_start:
                        o = owners.get(p, p)
                        oi = token_starts.get(o)
                        if (
                            _offset_for_ref(p) is not None
                            and token_starts.get(p) is None
                            and oi is not None
                            and oi[0] == "M"
                            and oi[1] == NOT_MAX_M2TIP140_DST_OWNER_PL
                            and p - o == NOT_MAX_M2TIP140_DST_INTO
                            and (pos - p) == NOT_MAX_M2TIP140_DST_AGE
                            and _match_length(
                                data,
                                pos,
                                p,
                                NOT_MAX_M2TIP140_EMIT_LEN,
                            )
                            >= NOT_MAX_M2TIP140_EMIT_LEN
                        ):
                            if alt is None or p > alt:
                                alt = p
                        p = prev[p]
                    if alt is not None:
                        emit_len = NOT_MAX_M2TIP140_EMIT_LEN
                        off = _offset_for_ref(alt)
                        assert off is not None
                        tokens.append(("M", off, emit_len))
                        token_starts[pos] = ("M", emit_len)
                        for i in range(emit_len):
                            owners[pos + i] = pos
                        cut = pos + emit_len - DICT_SIZE
                        if cut > 0:
                            token_starts = {
                                k: v
                                for k, v in token_starts.items()
                                if k >= cut
                            }
                            owners = {
                                k: v for k, v in owners.items() if k >= cut
                            }
                        end = pos + emit_len
                        while pos < end:
                            insert(pos)
                            pos += 1
                        continue

            # not_max: sole M3 tip age == 244, best_len == 4 → emit len 3 from
            # M7 mid into +3 age == 176 (pe2pkg file50146@584).
            if (
                sole is not None
                and best_len == NOT_MAX_M3TIP244_TO_M7INTO3_BEST
            ):
                tip = token_starts.get(sole)
                if (
                    tip is not None
                    and tip[0] == "M"
                    and tip[1] == NOT_MAX_M3TIP244_TO_M7INTO3_SRC_PL
                    and (pos - sole) == NOT_MAX_M3TIP244_TO_M7INTO3_SRC_AGE
                ):
                    alt = None
                    p = head[data[pos]]
                    while p >= window_start:
                        o = owners.get(p, p)
                        oi = token_starts.get(o)
                        if (
                            _offset_for_ref(p) is not None
                            and token_starts.get(p) is None
                            and oi is not None
                            and oi[0] == "M"
                            and oi[1] == NOT_MAX_M3TIP244_TO_M7INTO3_DST_OWNER_PL
                            and p - o == NOT_MAX_M3TIP244_TO_M7INTO3_DST_INTO
                            and (pos - p) == NOT_MAX_M3TIP244_TO_M7INTO3_DST_AGE
                            and _match_length(
                                data,
                                pos,
                                p,
                                NOT_MAX_M3TIP244_TO_M7INTO3_EMIT_LEN,
                            )
                            >= NOT_MAX_M3TIP244_TO_M7INTO3_EMIT_LEN
                        ):
                            if alt is None or p > alt:
                                alt = p
                        p = prev[p]
                    if alt is not None:
                        emit_len = NOT_MAX_M3TIP244_TO_M7INTO3_EMIT_LEN
                        off = _offset_for_ref(alt)
                        assert off is not None
                        tokens.append(("M", off, emit_len))
                        token_starts[pos] = ("M", emit_len)
                        for i in range(emit_len):
                            owners[pos + i] = pos
                        cut = pos + emit_len - DICT_SIZE
                        if cut > 0:
                            token_starts = {
                                k: v
                                for k, v in token_starts.items()
                                if k >= cut
                            }
                            owners = {
                                k: v for k, v in owners.items() if k >= cut
                            }
                        end = pos + emit_len
                        while pos < end:
                            insert(pos)
                            pos += 1
                        continue

            # not_max: sole M17 tip age == 244, best_len == 4, pre == 0 → emit
            # len 3 from M17 mid into +5 age == 239 (pe2pkg file30300/01@568).
            if (
                sole is not None
                and best_len == NOT_MAX_M17TIP244_TO_M17INTO5_BEST
                and pos > 0
                and data[pos - 1] == NOT_MAX_M17TIP244_TO_M17INTO5_PRE
            ):
                tip = token_starts.get(sole)
                if (
                    tip is not None
                    and tip[0] == "M"
                    and tip[1] == NOT_MAX_M17TIP244_TO_M17INTO5_SRC_PL
                    and (pos - sole) == NOT_MAX_M17TIP244_TO_M17INTO5_SRC_AGE
                ):
                    alt = None
                    p = head[data[pos]]
                    while p >= window_start:
                        o = owners.get(p, p)
                        oi = token_starts.get(o)
                        if (
                            _offset_for_ref(p) is not None
                            and token_starts.get(p) is None
                            and oi is not None
                            and oi[0] == "M"
                            and oi[1]
                            == NOT_MAX_M17TIP244_TO_M17INTO5_DST_OWNER_PL
                            and p - o
                            == NOT_MAX_M17TIP244_TO_M17INTO5_DST_INTO
                            and (pos - p)
                            == NOT_MAX_M17TIP244_TO_M17INTO5_DST_AGE
                            and _match_length(
                                data,
                                pos,
                                p,
                                NOT_MAX_M17TIP244_TO_M17INTO5_EMIT_LEN,
                            )
                            >= NOT_MAX_M17TIP244_TO_M17INTO5_EMIT_LEN
                        ):
                            if alt is None or p > alt:
                                alt = p
                        p = prev[p]
                    if alt is not None:
                        emit_len = NOT_MAX_M17TIP244_TO_M17INTO5_EMIT_LEN
                        off = _offset_for_ref(alt)
                        assert off is not None
                        tokens.append(("M", off, emit_len))
                        token_starts[pos] = ("M", emit_len)
                        for i in range(emit_len):
                            owners[pos + i] = pos
                        cut = pos + emit_len - DICT_SIZE
                        if cut > 0:
                            token_starts = {
                                k: v
                                for k, v in token_starts.items()
                                if k >= cut
                            }
                            owners = {
                                k: v for k, v in owners.items() if k >= cut
                            }
                        end = pos + emit_len
                        while pos < end:
                            insert(pos)
                            pos += 1
                        continue

            # not_max: sole M7 mid into +3 age == 252, best_len == 4, pre == 163,
            # just-finished M4 → emit len 3 from M3 mid into +2 age == 12
            # (pe2pkg file50146@660).
            if (
                sole is not None
                and best_len == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_BEST
                and pos > 0
                and data[pos - 1] == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_PRE
                and any(
                    info[0] == "M"
                    and info[1] == 4
                    and own + 4 == pos
                    for own, info in token_starts.items()
                )
            ):
                o = owners.get(sole, sole)
                oi = token_starts.get(o)
                if (
                    token_starts.get(sole) is None
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1]
                    == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_SRC_OWNER_PL
                    and sole - o
                    == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_SRC_INTO
                    and (pos - sole)
                    == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_SRC_AGE
                ):
                    alt = None
                    p = head[data[pos]]
                    while p >= window_start:
                        oo = owners.get(p, p)
                        oi2 = token_starts.get(oo)
                        if (
                            _offset_for_ref(p) is not None
                            and token_starts.get(p) is None
                            and oi2 is not None
                            and oi2[0] == "M"
                            and oi2[1]
                            == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_DST_OWNER_PL
                            and p - oo
                            == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_DST_INTO
                            and (pos - p)
                            == NOT_MAX_M7INTO3_252_TO_M3INTO2_12_DST_AGE
                            and _match_length(
                                data,
                                pos,
                                p,
                                NOT_MAX_M7INTO3_252_TO_M3INTO2_12_EMIT_LEN,
                            )
                            >= NOT_MAX_M7INTO3_252_TO_M3INTO2_12_EMIT_LEN
                        ):
                            if alt is None or p > alt:
                                alt = p
                        p = prev[p]
                    if alt is not None:
                        emit_len = NOT_MAX_M7INTO3_252_TO_M3INTO2_12_EMIT_LEN
                        off = _offset_for_ref(alt)
                        assert off is not None
                        tokens.append(("M", off, emit_len))
                        token_starts[pos] = ("M", emit_len)
                        for i in range(emit_len):
                            owners[pos + i] = pos
                        cut = pos + emit_len - DICT_SIZE
                        if cut > 0:
                            token_starts = {
                                k: v
                                for k, v in token_starts.items()
                                if k >= cut
                            }
                            owners = {
                                k: v for k, v in owners.items() if k >= cut
                            }
                        end = pos + emit_len
                        while pos < end:
                            insert(pos)
                            pos += 1
                        continue

            # not_max: sole M5 mid into +4 age == 244, best_len ≥ 6, pre == 125,
            # just-finished M2 → emit len 2 from M2 tip age == 198
            # (pe2pkg file20600@586).
            if (
                sole is not None
                and best_len >= NOT_MAX_M5INTO4_244_TO_M2TIP198_MIN_BEST
                and pos > 0
                and data[pos - 1] == NOT_MAX_M5INTO4_244_TO_M2TIP198_PRE
                and any(
                    info[0] == "M"
                    and info[1] == 2
                    and own + 2 == pos
                    for own, info in token_starts.items()
                )
            ):
                o = owners.get(sole, sole)
                oi = token_starts.get(o)
                if (
                    token_starts.get(sole) is None
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1] == NOT_MAX_M5INTO4_244_TO_M2TIP198_SRC_OWNER_PL
                    and sole - o == NOT_MAX_M5INTO4_244_TO_M2TIP198_SRC_INTO
                    and (pos - sole) == NOT_MAX_M5INTO4_244_TO_M2TIP198_SRC_AGE
                ):
                    alt = None
                    p = head[data[pos]]
                    while p >= window_start:
                        tip = token_starts.get(p)
                        if (
                            _offset_for_ref(p) is not None
                            and tip is not None
                            and tip[0] == "M"
                            and tip[1] == NOT_MAX_M5INTO4_244_TO_M2TIP198_DST_PL
                            and (pos - p)
                            == NOT_MAX_M5INTO4_244_TO_M2TIP198_DST_AGE
                            and _match_length(
                                data,
                                pos,
                                p,
                                NOT_MAX_M5INTO4_244_TO_M2TIP198_EMIT_LEN,
                            )
                            >= NOT_MAX_M5INTO4_244_TO_M2TIP198_EMIT_LEN
                        ):
                            if alt is None or p > alt:
                                alt = p
                        p = prev[p]
                    if alt is not None:
                        emit_len = NOT_MAX_M5INTO4_244_TO_M2TIP198_EMIT_LEN
                        off = _offset_for_ref(alt)
                        assert off is not None
                        tokens.append(("M", off, emit_len))
                        token_starts[pos] = ("M", emit_len)
                        for i in range(emit_len):
                            owners[pos + i] = pos
                        cut = pos + emit_len - DICT_SIZE
                        if cut > 0:
                            token_starts = {
                                k: v
                                for k, v in token_starts.items()
                                if k >= cut
                            }
                            owners = {
                                k: v for k, v in owners.items() if k >= cut
                            }
                        end = pos + emit_len
                        while pos < end:
                            insert(pos)
                            pos += 1
                        continue

            # not_max: sole M2 mid into +1 age == 244, best_len == 4, pre == 86,
            # just-finished M2 → emit len 3 from M9 mid into +6 age == 164
            # (pe2pkg file20600@588).
            if (
                sole is not None
                and best_len == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_BEST
                and pos > 0
                and data[pos - 1] == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_PRE
                and any(
                    info[0] == "M"
                    and info[1] == 2
                    and own + 2 == pos
                    for own, info in token_starts.items()
                )
            ):
                o = owners.get(sole, sole)
                oi = token_starts.get(o)
                if (
                    token_starts.get(sole) is None
                    and oi is not None
                    and oi[0] == "M"
                    and oi[1] == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_SRC_OWNER_PL
                    and sole - o == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_SRC_INTO
                    and (pos - sole) == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_SRC_AGE
                ):
                    alt = None
                    p = head[data[pos]]
                    while p >= window_start:
                        oo = owners.get(p, p)
                        oi2 = token_starts.get(oo)
                        if (
                            _offset_for_ref(p) is not None
                            and token_starts.get(p) is None
                            and oi2 is not None
                            and oi2[0] == "M"
                            and oi2[1]
                            == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_DST_OWNER_PL
                            and p - oo
                            == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_DST_INTO
                            and (pos - p)
                            == NOT_MAX_M2INTO1_244_TO_M9INTO6_164_DST_AGE
                            and _match_length(
                                data,
                                pos,
                                p,
                                NOT_MAX_M2INTO1_244_TO_M9INTO6_164_EMIT_LEN,
                            )
                            >= NOT_MAX_M2INTO1_244_TO_M9INTO6_164_EMIT_LEN
                        ):
                            if alt is None or p > alt:
                                alt = p
                        p = prev[p]
                    if alt is not None:
                        emit_len = NOT_MAX_M2INTO1_244_TO_M9INTO6_164_EMIT_LEN
                        off = _offset_for_ref(alt)
                        assert off is not None
                        tokens.append(("M", off, emit_len))
                        token_starts[pos] = ("M", emit_len)
                        for i in range(emit_len):
                            owners[pos + i] = pos
                        cut = pos + emit_len - DICT_SIZE
                        if cut > 0:
                            token_starts = {
                                k: v
                                for k, v in token_starts.items()
                                if k >= cut
                            }
                            owners = {
                                k: v for k, v in owners.items() if k >= cut
                            }
                        end = pos + emit_len
                        while pos < end:
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
            )
            off = _offset_for_ref(chosen)
            assert off is not None
            tokens.append(("M", off, best_len))
            token_starts[pos] = ("M", best_len)
            for i in range(best_len):
                owners[pos + i] = pos
            cut = pos + best_len - DICT_SIZE
            if cut > 0:
                token_starts = {k: v for k, v in token_starts.items() if k >= cut}
                owners = {k: v for k, v in owners.items() if k >= cut}
            end = pos + best_len
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




# Historical name used by identity tooling; prefer encode_lzss_cascading in new code.
encode_lzss_cascading = encode_lzss
