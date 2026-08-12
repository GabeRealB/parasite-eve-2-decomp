# Parasite Eve 2 — LZSS encoders

Production and identity-research encoders for the PE2 LZSS bitstream.
**Bitstream layout and decode** live in `ASSET_FORMATS.md` §4 and
`tools/peassets/lzss.py`. This document covers **encode** policies only.

## Modules

| Module | Role |
|--------|------|
| `lzss.encode_lzss` | **Production / best-effort matching.** Always decoder-compatible. Default: greedy newest multi_max. `kind="clut"` → `lzss_clut.encode_clut`. Used by `pack.py` / `image_codec.py`. |
| `lzss_cascading.encode_lzss` | **Frozen experiment** toward byte-identical re-encodes (large multi_max / force-lit cascade). Soft-freeze: do not expand casually. |
| `lzss_clut.encode_clut` | **CLUT identity** policy (posts + BST hybrid); also reached via `encode_lzss(..., kind="clut")`. |

Runtime decompressors (same bitstream, different I/O):

| Routine | Used for | Difference |
|---------|----------|------------|
| `Fs_DecompressChunk` | room packages (CD-fed) | Resumable / sector-fed |
| `Fs_DecompressImage` | image/CLUT payloads | Non-resumable, in-memory |

No second on-disc LZSS dialect is known. **Compress was offline** (not in the
retail main executable path we decompile).

## Production encoder (best-effort matching)

`lzss.encode_lzss` is the encoder for repack and image authoring. It is a
**best-effort** attempt to match retail bitstreams when policies exist
(especially CLUTs), while always remaining decoder-compatible:

* Always: `decode_lzss(encode_lzss(data)) == data`.
* Default policy: greedy longest match in the last 256 *written* bytes;
  multi_max → **newest**; skip unencodable match starts (ring index `0xFF`).
* **CLUTs** (`kind="clut"`): delegates to `lzss_clut.encode_clut`.

It does **not** guarantee byte-identical package or image-strip streams.
Measure round-trip and pack sector layout with:

```text
python3 tools/peassets/lzss_roundtrip_report.py --log layout_diff.log
```

Frozen cascade identity scores:

```text
python3 tools/peassets/lzss_identity_report.py
```

## Bit-identical identity targets

Every independent LZSS stream in stage assets (after `trim_lzss`):

* `.pe2pkg` bodies  
* `.pe2clut` payloads (after 16-byte header)  
* each `.pe2img` strip (dictionary reset between strips)

Known exact today (bit-identical after `trim_lzss`), **frozen cascade**
(`lzss_cascading`) unless noted:

| Corpus | Exact | Notes |
|---|---:|---|
| stage0 pe2clut data ≤ 4K | **476 / 674 (70.6%)** | multi_max-dominant; force-lit + cascade + not_max |
| all-stage pe2clut ≤ 4K | (re-measure after cascade) | |
| stage0 pe2pkg data ≤ 4K | **46 / 74 (62.2%)** | pure ladder + **file30406/07** exact; climbing: 50146 ~84%, 30300 ~79%, 30200 ~48%, 20600 ~40%, 50119 ~25% — **baseline frozen** |
| stage0 pe2pkg all sizes | **46 / 315 (14.6%)** | large packages diverge early on multi_max |
| all-stage pe2clut (full) | **617 / 859** via `lzss_clut` | CLUT-only encoder; cascade ~561/859 |

Also: 4-byte stubs; pure ladder pe2pkg (`file30102` / `30400`–`30402` /
`30503`); large M17 CLUT ladders (~3KB) and offΔ=1 cascade
(`file30100`–`30103` / `30404`). Among CLUT misses, first diverge is multi_max
in ~97% of cases (median matched prefix ~22% of tokens). Lit vs young same-len
MS is gated by the pre-byte before the same MS (`data[same-1] ≤ 36` → same).
Late multi_max post-fixes include pe2pkg ladder re-anchors, CLUT RLE
continuation, best==2/3/4/6 lit/same/PE/mid re-anchors, M17 into+2 / tip-age
re-anchors, sole-old-lit / M2-tip / mid force-lits, and gated **not_max**
short-match overrides (old M4 mid / old M2 tip → len-2 re-anchor). High-prefix
CLUT cascade landed full exacts for `file20318`–`20323`, `file20324`–`20329`,
`file20306`, `file20314`, `file20333`–`20341`, `file20342`–`20350`,
`file20408`/`20409`, `file20414`/`20407`/`20415`, `file101500`/`201500`/`301500`,
`file102000/1` family (13 streams), `file20213`–`20215`, `file403100/2`,
full `file444000`/`403200` pe2clut streams (`/2`–`/4`), full
`file102000/0` family (7 streams: 102000/202000/302000/105600/205600/305600/535800),
pure `file20209`, `file20121`, `file20212`/`20220`/`20235`, `file20702`/`20711`,
`file402200`, and `file503500/4`. Cascading: `file104900/2` family (~77%, force-lit).
Remaining work is lower-prefix multi_max, more not_max, and path misses.
Streams stay valid PE2 LZSS even when not bit-identical.

### pe2pkg ≤4K at 46/74 (baseline frozen)

Pure ladder packages were already exact; cascade 8cf224–370 + gated
force-lit/not_max landed **file30406/07** (first impure exacts). Remaining 28
misses are impure. Climbing prefixes (after encode-path first-miss cascade
R363–R370 + not_max/force-lit):

| File | Prefix | Notes |
|------|-------:|-------|
| `file50146` | **~84%** | path force-lit ladder (old M tips/mids age 244/252) |
| `file30300`/`30301` | **~79%** | R363+R365+R368 (M17 mid re-anchors) |
| `file30200`/`30201` | **~48%** | same R365/R368 family as 30300 + force-lit |
| `file20600` | **~40%** | not_max pair + R367/R369 |
| `file50119`–`21` | **~25%** | R366/R370 M2 tip age swaps |

**Soft-freeze:** keep production `R###` / force-lit / not_max in
`lzss_cascading.py` (do not delete). Prefer general encode rules over more
singleton cascade patches when possible. Baselines to protect (≤4K): pe2pkg
**46/74** (incl. `file30406`/`07`), pe2clut **476/674**. Measure with
`lzss_identity_report.py`.

## CLUT-only encoder (`lzss_clut.encode_clut`)

General-first policy for `*.pe2clut` (not the package cascade). Production
entry: `lzss.encode_lzss(data, kind="clut")`.

1. greedy longest match in the last 256 bytes  
2. force lit at ring write index `0xFF`  
3. force lit when the **sole** max-length candidate has age ≥ 242  
4. multi_max → **newest**, then safe post-rules only  
5. nc==2 L tip vs match tip, `best≤4`, match pl ≥ 3, age gap 2 → L  
6. long multi_max: young mid of **M16** → oldest **M17** mid with
   `into == best` and age ≥ 96  
7. young mid into +1 of **M4** → L tip  
8. just-finished **M6** tip re-used as match source (age==pl==6) → L tip  
9. nc==2 match tip vs older L tip when `(pl, best, T_age, L_age)` is in a
   small encode-validated pair set (1-fix oracle gains; pairs are disjoint
   from stream-exact multi_max sites). Broad `gap≥14` / `L_age≥20` over-fires.  
10. Mid re-anchor keys from 1-fix gains (e.g. `M17+16@1` → older M17 mid).  
11. 2-fix chain re-anchors: tip→tip / mid→mid pairs that are +0 alone but
    gain when both land (e.g. `T2@4→T2@8` then `M3+1@2→M3+2@14`).  
12. 3-fix / short-pair re-anchors (same key tables; e.g. T2@22→L + T2@16→T2@50
    + T2@46→L; mid→L triples; rare L→tip).  
13. 2-/3-fix safe greedy merge (+5/−0): additional nc2 T→L age pairs, mid
    re-anchors (e.g. `M2+1@4→L@6` before `→M5+4@6`), tip→tip / L→tip keys.
14. Tip→mid re-anchors + more 2-fix chains (+5/−0): e.g. `T2@8→M3+2@14`
    with `T2@4→T2@10`; `T5@6→M17+13@12`; M14/M15 ladder mids.
15. M17 ladder 2-fix (+3/−0): `M17+13@30→M17+15@62` then
    `M17+2@32→M17+11@126` (zero alone).
16. L→mid post + 3-fix cluster triples (+3/−0): e.g. `L@8→M4+1@4` with
    mid ladder steps; `T2@12→M16+15@50` tip→mid. Force-lit / `M17+14@224`
    still regress alone.
17. **Structural collapse** (exact unchanged **571/859**): broad predicates
    over-fire (into+1→L −22; M17+16@1→oldest mid −383). Instead:
    - `M17_INTO16_AGE1_BY_BEST` map (4 best→dest) replaces 4 mid rows
    - `MID_TO_L_KEYS` compact 5-tuples; drop dead `M3+1@8→L@12`
    - residual mid→mid/tip only in `MID_REANCHOR_KEYS`
    - unified `TIP_REANCHOR_KEYS` / `L_REANCHOR_KEYS` (tip|mid dest kind)
18. **Okumura BST last-at-max** (+46/−0 → **617/859**): when multi_max posts
    leave **newest** as a match **mid** with phrase length `pl < 17`, rebuild
    an Okumura-style BST over same-first-byte window refs and take the **last**
    search-path node at max true match length (if among max-cands). Classic
    first-at-max is useless (~6% multi_max agreement). Raw “any newest mid”
    over-fires (~−112); almost all exact-stream damage is M17 mids — hence
    `pl < 17`. Full key-exclude of residual over-fire shapes reaches ~625 but
    is a sparse catalog; general rule lands first.

Ablation: inherited multi_max cascades were net-negative vs newest; sole
force-lit singles collapsed into (3). Accept new post-rules only if full
corpus exact does not fall. Measured exact **617/859** (`lzss_cascading`
~561/859 — CLUT-only **ahead** of frozen cascade by ~56). Residual:
multi_max misses still ~4k sites. Shared decode / trim / pack stay in
`lzss.py`.

## Multi_max research findings (historical, offline)

| Partition | Sites | Naive newest | Current pick |
|-----------|------:|-------------:|-------------:|
| all multi_max ≤4K | ~30k | ~77% | **~80%** |
| **best ≤ 4** (short) | ~13k | ~51% | **~58%** |
| best ≥ 14 (long) | ~16k | **~99.9%** | ~99.7% |

Long ties are the M17 ladder: retail almost always picks `mid into=16` of an
M17, and **newest is already that mid** — tip-first global rankers collapse
accuracy to ~20%. Cascade value is concentrated in **short** multi_max
(`best≤4`): lit tips, M2/M3 tips vs mid-into-1, etc. Preferring “any lit tip
among cands” *hurts*; retail lit is rarely the newest candidate.

### Short-match class breakdown (`best≤4`, ~13.5k sites)

| Class | Meaning | Share |
|-------|---------|------:|
| A | current cascade pick correct | ~58% |
| B | pick wrong, **2nd-newest** correct | ~19% |
| C | pick wrong, **newest** correct (cascade over-eager) | ~7% |
| D | needs deeper rule | ~16% |
| **A∪B∪C oracle** | ceiling if we knew which of {pick,2nd,newest} | **~84%** |

Crude post-corrects over current (always 2nd, `nc==2` + lit, young tip→lit,
etc.) all **net negative** (high false positives).

### High-precision rule mine (offline)

Three gates with prec ≥70% gave **+41 short multi_max sites**
(58.08%→58.39%) on the *retail* path:

1. pick mid into≥15 → newest  
2. pick==newest, nw=T3, 2nd=Minto2 → 2nd  
3. pick==newest, nw=Minto2, 2nd=T3 → 2nd  

**Wire-up attempt rolled back:** applying those as encode post-corrects
**regressed** exacts (pe2pkg 46→42, pe2clut 476→472, lost file30406/07).
Lesson: retail-path multi_max agreement ≠ encode-path safety — changing a
mid-stream pick alters `token_starts`/`owners` for the rest of the stream.
Any scorer must be validated with **full encode identity**, not only multi_max
site accuracy.

### Encode-validated multi_max rule scan (historical)

Monkeypatched `_pick_match_ref` per candidate, pe2pkg/pe2clut exacts ≤4K:

| Candidate | pkg Δ | clut Δ | notes |
|-----------|------:|-------:|-------|
| mid15→newest (and variants) | −1..−6 | 0..−1 | loses 30406/07 |
| T2/L → 2nd (tip→lit) | −5 | **−135** | CLUT catastrophe |
| T3/L, T2/T2, M1/L → 2nd | −2..−6 | −12..−57 | still bad |
| **M2→T3 → 2nd** | **0** | **0** | SAFE but no exact gain; key prefixes unchanged |
| offline 3-rule combo | −4 | −4 | confirms prior regression |

So far **zero** multi_max scorer candidates improve *exact counts* under that
scan. Later **encode-path first-miss** gates (not retail-only tables) *did*
raise prefixes without exact drops:

| Gate | Kind | Target | Prefix Δ | Exact Δ |
|------|------|--------|----------|---------|
| **R363** | multi_max | 30300@604 T4@24→T3@36 | 71.3→73.4% | 0 |
| **R364** | multi_max | 50146@688 mid+1 M3@160→M2 tip@180 | 82.6→83.1% | 0 |
| **R365** | multi_max | 30300/30200 M17 mid+13@231→mid+5@239 | →74.0% / 47.6% | 0 |
| **R366** | multi_max | 50119 M2 tips 28/122/230 →@122 | 24.1→24.4% | 0 |
| **R367** | multi_max | 20600 M17 mid+4@28→M3 tip@156 | 37.5→39.6% | 0 |
| **R368** | multi_max | 30300/30200 M17 mid+10@222→M9 tip@112 | →**79.1%** / 48.4% | 0 |
| **R369** | multi_max | 20600 M17 mid+2@68→@48 | →**40.2%** | 0 |
| **R370** | multi_max | 50119 M2 tip@230→@28 (jf M8) | →**25.4%** | 0 |
| force-lit / not_max chain | path/not_max | 50146, 30200, 20600 | 50146→**84.6%** | 0 |

Lesson: **mine first-miss on the encode path** of high-prefix impure files.
Retail-only multi_max tables overfit; broad tip→lit style rules still nuke
CLUT (−7 exacts) unless age/nc gates are exact.

### pe2pkg not_max census (retail path, ≤4K)

357 events, **93% sole_best**, source ages cluster at 252/256/244/240/248;
heavy on 30500/30100/50152 families. Most are *behind* early multi_max
diverges — only **1** impure ≤4K package had first miss = not_max
(`file20600`). Fix multi_max first on high-prefix climbers; not_max/force-lit
when first-miss kind says so.

## One format, different data / maybe different tools

**Retail multi_max fingerprints differ sharply by asset class** (stage0 ≤4K):

| | pe2pkg | pe2clut |
|--|-------:|--------:|
| multi_max sites | 5 846 | 24 624 |
| naive newest OK | **47%** | **84%** |
| current pick OK | **51%** | **87%** |
| long `best≥14` share | ~3% | **~63%** (M17 `mid_into=16`) |
| short `best≤4` share | **~88%** | ~34% |
| **not_max** (len &lt; greedy best) | **2.85%** of matches | **0.10%** |
| P(retail=lit \| lit among max) | **25%** | **14%** |
| identity exact ≤4K (cascade) | 46/74 (62%) | 476/674 (71%) |
| miss prefix p50 | **4.3%** | 13.6% |

Interpretation:

1. **Not “different bit formats.”** Decode is one family.
2. **Likely different *content models*:** CLUTs are palette/RLE-heavy → long
   M17 ladders where newest already wins; packages are structured game blobs →
   short multi_max + **not_max** (almost absent in CLUT).
3. **Possibly different offline encoders or presets** (art pipeline vs level
   pack), or one encoder whose heuristics only *surface* as not_max on package
   data. Encode-validated tip→lit rules **destroy CLUT** (−135 exacts) while
   package movement is smaller — strong argument against one global short-match
   policy without a type/content gate.
4. **Practical path:** `lzss.encode_lzss` for production/best-effort (CLUTs via
   `kind="clut"`); frozen `lzss_cascading` for package identity baselines.
   Never land a cascade rule that drops pe2pkg ≤4K below 46/74.
5. **Encode-path first-miss cascade** (historical, in `lzss_cascading`) beats
   retail-table mining for prefix gains; re-measure with
   `lzss_identity_report.py`.

## Related tools

| Script | Purpose |
|--------|---------|
| `lzss_roundtrip_report.py` | Production encoder RT + sector layout log |
| `lzss_identity_report.py` | Frozen cascade bit-identity scores |
| `lzss.py` | Decode, trim, pack, production encode |
| `lzss_clut.py` | CLUT identity encoder |
| `lzss_cascading.py` | Frozen package/CLUT cascade experiment |
