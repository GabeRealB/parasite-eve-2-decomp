# Parasite Eve 2 — TMD model format

What we know about the **model streams** inside the `.pe2pkg` overlay packages:
the container that ties a mesh together, the packet stream that describes its
faces, and what each opcode means. Derived from the matching decomp
(`src/main/tmd.c`, `src/main/hasm/Tmd_StreamHandlers_Ops.s`,
`src/gameplay/1BC.c`) and from walking the 360 model streams the extractor
carves out of the retail USA discs.

These are **not** a stage chunk type. There is no model chunk on disc; a model
lives inside the room/actor package that uses it, so nothing in the CDF tree
points at one. See [`OVERLAYS.md`](OVERLAYS.md) for which package is which and
[`ASSET_FORMATS.md` §9](ASSET_FORMATS.md#9-models-and-animation) for how models
and animation sit together.

| Area | Code / tools |
|------|----------------|
| Stream walk + opcode switches | `src/main/tmd.c` (`Tmd_InitSourceStream`, `Tmd_ProcessStream`) |
| Early-image handlers | `src/main/hasm/Tmd_StreamHandlers_Ops.s` |
| Container types | `include/main/tmd.h` (`TmdSource`, `TmdObject`) |
| Attach path | `src/gameplay/gameplay.c` (`Gp_AttachTmd`), `src/main/task.c` |
| Locate / carve streams | `tools/peassets/pkg_model.py` |

**Status.** The format is understood well enough to write an exporter. An
opcode's bits give the corner count, the shading model, whether it is textured
and whether it reads pre-transformed vertices; the refs decode against the
vertex and normal arrays at 100% across every model with a `TmdSource`; and
all 23 draw families are mapped to a `POLY_*` type with their texture
coordinates located (§5.1). What is left is peripheral — a few high bits that
select shading paths without changing layout, two transform helpers, and the
inverse direction for import. See §6.

---

## 1. A model is four pieces

The face stream alone is not a model: the vertices live outside it. A
`TmdSource` record ties the pieces together, and `Gp_AttachTmd` reaches one
through `TaskDesc.setupArg`:

```text
TmdSource (0x24 bytes; field_0 is 0 on disc, set to 1 after first use)
  +0x14  u32  -> vertex array   8 bytes per entry (SVECTOR-shaped)
  +0x18  u32  -> normal array   same shape
  +0x20  u32  -> face stream
```

`Tmd_ProcessStream` copies those into its scratch as `ws->field_8` (vertices)
and `ws->field_C` (normals); the handlers index off them.

A model is laid out contiguously with the record last, so the counts fall out
of the gaps:

```text
[ vertices ][ normals ][ face stream ][ TmdSource ]
  nverts   = (normals - vertices) / 8
  nnormals = (stream  - normals)  / 8
```

That makes `TmdSource` findable without guessing: look for a word at `+0x20`
equal to the start of an already-validated stream, with `+0x14` and `+0x18`
also inside the package. `pe2pkg_73` yields six records this way, `pe2pkg_0`
three.

---

## 2. Packet stream

```text
repeated:
  u32 id            opcode; 0xFFFFFFFF ends the stream,
                    0xFFFFFFFE is a skip (advance one word, re-read)
  u32 handler_slot  overwritten at runtime - see below
  u32 dims          (count << 16) | stride, stride in words
  u32 payload[stride * count]
```

`Tmd_InitSourceStream` advances by `(dims >> 16) * (dims & 0xFFFF)` words. The
product is symmetric, so **delimiting** a stream works with the halves either
way round, but **parsing elements** does not: the high half is the count, the
low half the stride. The giveaway in real data is a CLUT-looking word recurring
at the stride interval — every 7 words in an `0x78` packet, not every 20.

`Tmd_InitSourceStream` resolves each `id` to a handler and **writes the pointer
into `handler_slot`**, so a stream that has run once no longer matches its
on-disc form. Decode from the extracted file, never from a RAM dump.

### 2.1 Rejecting false streams

`0x0` looked like an open question — it is the highest-count opcode in the raw
scan and nearly always appears with `dims == 0`. It is not a format feature.
`0x0` is a valid opcode whose `dims` of 0 means no payload, so **a run of zero
padding parses as a chain of empty packets** and the scanner accepts it as a
stream.

Ground truth settles it. Of the streams a `TmdSource` actually points at, none
begins with `0x0` or `0x4`; of the unreferenced candidates, 215 of 415 do. So
those two opcodes are rejected as a stream's *first* packet:

```python
BAD_FIRST_OPCODES = frozenset({0x0, 0x4})
```

That takes the scan from 703 located streams to 572, and 360 unique files to
298, while keeping every source-backed stream but one — and that one begins
`op=0x0, dims=0x0000FFFF`, count 0 with stride 65535, so its "source" was a
coincidental pointer triple rather than a real record. Precision improved on
both sides of the check.

The lesson generalises: when a scanner's most common result has no analogue in
the ground-truth subset, suspect the scanner before theorising about the
format.

---

---

## 3. Element layout

### 3.1 General rule

A geometry element is a run of `u16` **byte offsets**, two per word, vertices
first and then normals. The index into either array is `offset / 8`, since both
are 8 bytes per entry. Anything after the refs is UV, CLUT, tpage and colour.

```text
refs = [v0 .. v(nv-1)] ++ [n0 .. n(nn-1)]     packed two per word from w0
nv   = 4 if opcode & 0x40 else 3              corners
nn   = nv if opcode & 0x20 else 1             per-vertex normals, or one face normal
```

So `0x38` (tri, gouraud) is `w0=(v0,v1) w1=(v2,n0) w2=(n1,n2)`, and `0x78`
(quad, gouraud) is `w0=(v0,v1) w1=(v2,v3) w2=(n0,n1) w3=(n2,n3)`.

**Validation.** Applying this rule to every model with a `TmdSource` and
checking that each ref is 8-aligned and inside its array:

| Opcodes | Layout | Elements checked | Valid |
|---|---|---:|---:|
| `0x18` `0x1C` `0x1E` | 3v + 1n | 127 | 100% |
| `0x20` `0x22` `0x30` `0x38` `0x3A` `0x10038` `0x1003A` | 3v + 3n | 5106 | 100% |
| `0x58` | 4v + 1n | 214 | 100% |
| `0x60` `0x62` `0x70` `0x78` `0x7A` `0x10078` | 4v + 4n | 6097 | 100% |
| `0x5C` `0x5E` | 4 refs | 102 | 100% |

`0x5C` / `0x5E` fit both `4v + 0n` and `3v + 1n` — the two consume the same
four refs, and range-checking cannot separate them. Their handler has to
settle it.

### 3.2 Opcode bits

Derived from both handler sets: the init handlers in
`Tmd_StreamHandlers_Ops.s` (which element words they read, which `ws` array
each ref is added to, where they store screen coordinates, which GTE commands
they issue) and the draw handlers in `src/gameplay/gameplay.c` (which
`POLY_*` type they build).

A caution learned the hard way: each handler loads *different* `ws` fields
into the same registers, so a register name means nothing on its own. `Op38`
uses `$t6` for the vertex array; `Op39` uses `$t6` for the transform cache.

| Bit | Meaning | How it shows up |
|---|---|---|
| `0x40` | **quad** (4 corners) instead of triangle | `AVSZ4` instead of `AVSZ3`; every `AVSZ3` handler has an `AVSZ4` partner at `+0x40` |
| `0x20` | **gouraud** — one normal per corner | handler adds 3–4 refs to `$t5`; without it, exactly 1 |
| `0x10` | adds 3 words of tail | stride +3 on every pair differing only in this bit |
| `0x08` | **use a constant instead of a per-element value** — one word less | stride −1 on every such pair; demonstrated by `0xC0` vs `0xC8`, whose handlers differ by the single instruction that loads the colour register from the element rather than from a constant (§3.5) |
| `0x10`+`0x08` | **textured** — net +2 words | XY stores 12 bytes apart (`POLY_*T*`) instead of 8; +2 on 7 of 8 pairs, the exception being `0x21`→`0x39` |
| `0x04` | **no per-vertex colour** — `POLY_F*` instead of `POLY_G*`, and no lighting at all in the transform pass | `0x18`→`0x1C` is `GT3`→`FT3`, `0x58`→`0x5C` is `GT4`→`FT4`; `0xC0`→`0xC4` drops `NCCS` and keeps only `RTPS` |
| `0x02` | ABR / semi-transparent variant | same handler and stride as the base opcode |
| `0x01` | **pre-transformed** — refs index the `0xC8` cache | handler reads screen coords already in the primitive buffer and only culls; refs are word offsets into `ws->field_10`, so the index is `ref / 4` (100% valid across every model) |
| `0x4000` | **two primitives per element** — a layered draw | §3.2.1 |
| `0x8000` `0x10000` `0x20000` | **alternate transform routine, supplied by the model's own package** — layout unchanged | §3.2.1 |
| `0x100` | shifts the ref block by 0, 2 or 3 words depending on family | §3.2.1 |

Reading `0x38` with this: `0x20` gouraud + `0x10`+`0x08` textured, no `0x40`,
so a textured gouraud triangle — a `POLY_GT3`, which is exactly what the
handler builds.

`0x10` and `0x08` are **separate bits**, not one "textured" flag: their stride
effects (+3 and −1) compose to the +2 seen when both are set, and each occurs
without the other (`0x30`, `0x70` carry `0x10` alone). Both produce a textured
primitive; what differs is the size of the ref block ahead of the UV words —
`0x30` has 4 ref words to `0x38`'s 3 (§5.1), which is where the extra word
goes.

Note that `0x20` and `0x04` are related but distinct: `0x20` controls how many
**normals are read** (lighting input), `0x04` controls whether the primitive
carries one colour or one per corner (`POLY_F*` vs `POLY_G*`, the output).

### 3.2.1 The high bits

They are not one thing. Comparing each pair's draw handler shows three
different mechanisms:

**`0x4000` — two primitives per element.** `func_8009F670` (`0x4038`) is
`func_8009ED90` (`0x38`) with an extra `poly++` before the UV writes, so the
handler consumes two primitive slots per element and fills only the second.
That is a layered draw — the same face emitted twice, presumably opaque plus a
blended pass. Verified on all four pairs (`0x38`, `0x78`, `0x39`, `0x79`):
primitives per element goes 1 → 2 with the UV word positions unchanged.

**`0x8000` / `0x10000` / `0x20000` — the model brings its own transform
routine.** These resolve to init handlers at `0x8013xxxx`, which is inside the
**actor package overlay**, not main or gameplay:

| Opcode | Init handler | Lives in | Draw family |
|---|---|---|---|
| `0x38` | `Tmd_StreamHandler_Op38` | main (hasm) | `D_8009ED90` |
| `0x8038` | `D_80136224` | actor package | `D_8009ED90` |
| `0x10038` | `D_8013700C` | actor package | `D_8009ED90` |
| `0x20038` | `D_801379B4` | actor package | `D_8009ED90` |

The draw family — and therefore the element layout — is identical to the base
opcode. Only the transform/light routine changes, and it is supplied by the
package being drawn. That is why these bits never move the stride. What those
routines actually do is out of reach: they live in overlays this project does
not split.

**`0x100` — shifts the ref block.** The UV words move later by a
family-dependent amount: +2 words for `0x30`→`0x130`, +3 for `0x70`→`0x170`,
and +0 for `0x31`→`0x131` and `0x71`→`0x171`. So it adds refs in some families
and nothing in others. None of the shifted opcodes appears in any extracted
model, so what the extra refs hold is unverified.

### 3.3 How arity was established

From `AVSZ3` vs `AVSZ4`, and corroborated by where each handler stores screen
coordinates. Never from the hasm header comment, which is wrong in both
directions.

| Handler | XY stores | Primitive | Arity |
|---|---|---|---|
| `Tmd_StreamHandler_Op20` | `0x8`, `0x10`, `0x18` | `POLY_G3` | triangle |
| `Tmd_StreamHandler_Op38` | `0x8`, `0x14`, `0x20` | `POLY_GT3` | triangle |
| `Tmd_StreamHandler_Op60` | `0x10`, `0x18`, `0x20` | `POLY_G4` | quad |
| `Tmd_StreamHandler_Op78` | `0x14`, `0x20`, `0x2C` | `POLY_GT4` | quad |

The quad handlers store three coordinates because the fourth is written on a
second pass.

Each handler's primitive advance equals the primitive size exactly, which
cross-checks the whole table:

| Advance | Primitive | Opcodes |
|---|---|---|
| `0x1C` | `POLY_G3` | `0x00` `0x20` |
| `0x24` | `POLY_G4` | `0x40` `0x60` |
| `0x28` | `POLY_GT3` | `0x18` `0x38` `0x39` `0x130` |
| `0x34` | `POLY_GT4` | `0x58` `0x78` `0x79` `0x170` |

### 3.4 Evidence from geometry

Under this reading every model in `pe2pkg_73` is a closed manifold — each edge
shared by exactly two faces, no boundary edges — and Euler's formula holds:

| Vertices | Edges | Faces | V − E + F |
|---:|---:|---:|---:|
| 13 | 33 | 22 | 2 |
| 40 | 114 | 76 | 2 |
| 22 | 60 | 40 | 2 |
| 33 | 93 | 62 | 2 |

Reading the `0x78` family as triangles instead leaves 21–42 boundary edges per
model and χ ≠ 2.

### 3.5 The transform pre-pass — `0xC0` / `0xC4` / `0xC8`

These three carry **no geometry**. They are a per-vertex transform pass that
runs before the drawing opcodes, filling the primitive buffer's screen
coordinates and colours and populating a per-vertex cache. `0xC8` is the
highest-volume opcode in the data.

They differ only in colour handling:

| Opcode | Stride | Element | Colour | GTE |
|---|---|---|---|---|
| `0xC0` | 3 | `w0` refs, `w1` colour, `w2` destinations | per element, `lwc2 $6, 0x4($a2)` | `RTPS` + `NCCS` |
| `0xC8` | 2 | `w0` refs, `w1` destinations | constant `0x00808080` loaded once into GTE `RGB` | `RTPS` + `NCCS` |
| `0xC4` | — | `w0` refs, `w1` destinations | none | `RTPS` only |

`0xC0` and `0xC8` are the same handler body apart from one instruction, which
is bit `0x08` doing exactly what §3.2 says: dropping the per-element word in
favour of a constant. `0xC4` (`0xC0 | 0x04`) skips lighting entirely, matching
`0x04` as the "no per-vertex colour" bit. `0xC4` never appears in the extracted
models.

Element layout, taking `0xC8`:

```text
w0.low   -> vertex byte offset    index = offset / 8
w0.high  -> normal byte offset    index = offset / 8
w1.low   -> destination offset for the transformed screen XY
w1.high  -> destination offset for the lit colour
```

Consecutive elements naming the same vertex skip the transform and reuse the
previous result, which is why one vertex appears in several elements with
different normals — in one 360-vertex model, 514 elements cover 180 distinct
vertices.

The pass also writes a **per-vertex depth cache** at `ws->field_10`, and that
is what settles the `ref / 4` divisor of §3.4 from the source rather than by
inference. `Tmd_StreamHandler_OpC8` stores the `RTPS` result with
`t3 = ws->field_10 + (vertex_byte_offset >> 1)`, so the cache holds one word
per vertex; `Tmd_StreamHandler_Op39` then reads its refs as
`ws->field_10 + ref` and feeds them to `SZ1`/`SZ2`/`SZ3`. Halving an 8-byte
stride gives 4, so a pre-transformed ref is `vertex_index * 4` and the cache
slot maps to a vertex one-to-one. The negative-value check either side of it
(`bltz` on the loaded word) is the off-screen flag `OpC8` sets from GTE
`FLAG`.

`0xC4`'s handler is decompiled C (`func_8009EAA4` in `src/gameplay/gameplay.c`)
and spells out what the hasm versions do:

```c
idx = rec[0];
if (idx != prev) {                                  // the caching branch
    gte_ldv0((u8*)ws->field_8 + (idx & 0xFFF8));    // vertex array, 8-byte aligned
    gte_rtps_real();
    gte_stsz(&ws->field_28);                        // keep Z
    if (ws->field_24 & 0x80000000)
        ws->field_28 |= 0x80000000;                 // mark culled
    ws->field_10[*(u16*)arg2 >> 3] = ws->field_28;  // cache[vertex index]
}
gte_stsxy(ws->field_4 + rec[1]);                    // screen XY into the prim
```

Three things fall out of it:

- **The cache is `ws->field_10` indexed by `offset >> 3`** — the vertex index —
  and holds one word each. That independently confirms the bit `0x01` reading
  in §3.2: those opcodes' refs are byte offsets into this word array, so the
  vertex index is `ref / 4`.
- **A negative cache entry means culled.** The sign bit is forced from
  `ws->field_24`, and the `0x01` handlers `bltz`-test the entry and skip the
  face.
- **`idx & 0xFFF8`** masks the low three bits before use, so they carry flags
  rather than address.

**An exporter can skip all three.** Positions and faces come from the geometry
opcodes; this pass only schedules shading. Its one use is the explicit
vertex-to-normal pairing, which is otherwise only implied by the face elements.

## 4. Opcode structure

### 4.1 Two dispatch tables

The opcode is resolved twice, against different handler sets, and the two do
different jobs:

| Switch | Handlers | What it does |
|---|---|---|
| `Tmd_InitSourceStream` | main, `Tmd_StreamHandler_*` at `0x80010A90` | one-shot, guarded by `TmdSource.field_0`. Resolves 61 opcodes to 53 handlers and **writes the pointer into the packet's slot word**. These are the transform/light/cull routines: they read vertices, run `RTPT`/`NCLIP`/`AVSZ`, and store screen XY and lit RGB. |
| `Tmd_ProcessStream` | the loaded overlay, `0x8009xxxx`, decompiled in `src/gameplay/gameplay.c` | walks the stream on each draw call. Reads the dims word, copies the cached pointer from the slot into `ws->field_20`, then calls the overlay handler, which fills the primitive's **static** fields — UV, CLUT, tpage. |

That split is why the untextured families do nothing per pass:
`func_8009FC90` and `func_8009FC44` only advance `prims` by `0x1C` and `0x24`
and step `arg2` by the stride. An untextured `POLY_G3`/`POLY_G4` has no UV to
refresh, so there is nothing for that handler to copy — and their prim advance
is what confirms the primitive type for opcodes whose handler names no
`POLY_*`.

`Tmd_ProcessStream` also confirms the `dims` split independently of the
empirical evidence in §2:

```c
ws->field_18 = ((u16*)stream)[0];   // stride, added to arg2 per element
ws->field_1C = ((u16*)stream)[1];   // count, the handler's loop counter
```

Little-endian, so `[0]` is the low halfword — stride low, count high.

### 4.2 Bit structure

See [§3.2](#32-opcode-bits) — the bits determine the element layout, so they
are documented with it. In short: `0x40` corners, `0x20` normals read, `0x10`
and `0x08` texturing, `0x04` flat vs gouraud primitive, `0x02` semi-transparent,
`0x01` pre-transformed.

---

## 5. Opcode reference

### 5.1 Draw families

Every family the per-frame switch dispatches to, with the primitive it builds
and where its texture coordinates come from. Read out of the **draw** handlers,
which are decompiled in `src/gameplay/gameplay.c` — `Tmd_ProcessStream` calls
them by address, and `tmd.c` declares them as `D_8009xxxx` while the overlay
defines them as `func_8009xxxx`, which is why they are easy to miss.

"Refs" is the ref block that precedes the UV words: `nv` vertex offsets then
`nn` normal offsets, packed two per word. It is derived — the first UV word
marks the end of the refs — and it agrees with the 100% range-check in §3.1.

| Base | Primitive | Corners | Refs | UV words | Opcodes | Elements |
|---|---|---|---|---|---|---:|
| `0x0` | untextured | 3 | — | — | `0x0` `0x20` `0x120` `0x4000` `0x4020` `0x4120` | 146169 |
| `0x4` | POLY_F3 | 3 | — | — | `0x4` | 52 |
| `0x5` | POLY_F3 | 3 | 3v (cache) | — | `0x5` | — |
| `0x18` | POLY_GT3 | 3 | 3v + 1n | u0=w2 u1=w3 u2=w4 lo | `0x18` `0x1A` | 128 |
| `0x1C` | POLY_FT3 | 3 | 3v + 1n | u0=w2 u1=w3 u2=w4 lo | `0x1C` `0x1E` | 357 |
| `0x30` | POLY_GT3 | 3 | 3v + 5n | u0=w4 u1=w5 u2=w6 lo | `0x30` | 28 |
| `0x31` | POLY_GT3 | 3 | 3v (cache) | u0=w2 u1=w3 u2=w4 lo | `0x31` `0x39` `0x3B` `0x131` `0x8039` | 6395 |
| `0x38` | POLY_GT3 | 3 | 3v + 3n | u0=w3 u1=w4 u2=w5 lo | `0x38` `0x3A` `0x8038` `0x10038` `0x1003A` `0x20038` | 13925 |
| `0x40` | untextured | 4 | — | — | `0x40` `0x60` `0x160` `0x4040` `0x4060` `0x4160` | 173 |
| `0x44` | POLY_F4 | 4 | — | — | `0x44` | 122 |
| `0x45` | POLY_F4 | 4 | 4v (cache) | — | `0x45` | — |
| `0x58` | POLY_GT4 | 4 | 4v + 2n | u0=w3 u1=w4 u2=w5 lo u3=w5 hi | `0x58` `0x5A` | 407 |
| `0x5C` | POLY_FT4 | 4 | 4v + 0n | u0=w2 u1=w3 u2=w4 lo u3=w4 hi | `0x5C` `0x5E` | 417 |
| `0x70` | POLY_GT4 | 4 | 4v + 6n | u0=w5 u1=w6 u2=w7 lo u3=w7 hi | `0x70` | 22 |
| `0x71` | POLY_GT4 | 4 | 4v (cache) | u0=w2 u1=w3 u2=w4 lo u3=w4 hi | `0x71` `0x79` `0x7B` `0x171` `0x8079` | 3566 |
| `0x78` | POLY_GT4 | 4 | 4v + 4n | u0=w4 u1=w5 u2=w6 lo u3=w6 hi | `0x78` `0x7A` `0x8078` `0x10078` `0x20078` | 13103 |
| `0x130` | POLY_GT3 | 3 | 3v + 9n | u0=w6 u1=w7 u2=w8 lo | `0x130` | — |
| `0x156` | POLY_GT4 | 4 | 4v + 8n | u0=w6 u1=w7 u2=w8 lo u3=w8 hi | `0x156` | 16 |
| `0x170` | POLY_GT4 | 4 | 4v + 12n | u0=w8 u1=w9 u2=w10 lo u3=w10 hi | `0x170` | — |
| `0x4038` | POLY_GT3 | 3 | 3v + 3n | u0=w3 u1=w4 u2=w5 lo | `0x4038` | 460 |
| `0x4039` | POLY_GT3 | 3 | 3v (cache) | u0=w2 u1=w3 u2=w4 lo | `0x4039` | 539 |
| `0x4078` | POLY_GT4 | 4 | 4v + 4n | u0=w4 u1=w5 u2=w6 lo u3=w6 hi | `0x4078` | 376 |
| `0x4079` | POLY_GT4 | 4 | 4v (cache) | u0=w2 u1=w3 u2=w4 lo u3=w4 hi | `0x4079` | 117 |

Each handler copies the UV words straight into the primitive and then biases
the page registers:

```c
poly->tpage += ws->field_70;      // from TmdObject.field_24
poly->clut  += ws->field_72;      // from TmdObject.field_25 << 6
```

So the stored `tpage`/`clut` are **relative** — the object's texture-page
assignment is added at draw time. An exporter has to apply the same bias to
resolve a real page.

`u0` and `u1` are written as full words, so each carries a `u`,`v` pair plus
the `clut` (in `u0`) or `tpage` (in `u1`) halfword, exactly as `POLY_GT3` /
`POLY_GT4` lay them out. `u2`/`u3` are halfword writes — UV only.

### Init-only opcodes

Resolved by `Tmd_InitSourceStream` but absent from the per-frame draw
switch, so they run once at setup rather than every frame.

| Opcode | Init handler | Stride | Elements | Role |
|---|---|---:|---:|---|
| `0x21` | `Tmd_StreamHandler_Prim30` | 2 | 2 | ? |
| `0x22` | `Tmd_StreamHandler_Op20` | 4 | 8 | ? |
| `0x61` | `Tmd_StreamHandler_Prim38` | — | — | ? |
| `0x62` | `Tmd_StreamHandler_Op60` | 5 | 26 | ? |
| `0xC0` | `Tmd_StreamHandler_OpC0` | 3 | 6 | "stream transform helper" per the hasm header; unsolved |
| `0xC4` | `D_8009EAA4` | — | — | "stream transform helper"; unsolved, never seen in data |
| `0xC8` | `Tmd_StreamHandler_OpC8` | 2 | 30262 | vertex transform + lighting pre-pass — **solved**, §3.0 |
| `0x121` | `Tmd_StreamHandler_Prim30` | — | — | ? |
| `0x122` | `D_8009E274` | — | — | ? |
| `0x161` | `Tmd_StreamHandler_Prim38` | — | — | ? |
| `0x162` | `D_8009E770` | — | — | ? |
| `0x40C8` | `D_8009AF90` | 2 | 1568 | `0xC8` with a different shading path |
| `0x200C8` | `D_801386EC` | 2 | 607 | `0xC8` with a different shading path |

---

## 6. What is still open

The format is understood: every draw family's primitive, corner count, ref
block and UV words are mapped (§5.1), and the opcode bits decode (§3.2).
What remains is narrower.

- **What the per-package transform routines do.** `0x8000` / `0x10000` /
  `0x20000` hand the transform off to a routine inside the actor package
  (§3.2.1). The element layout is unaffected, so geometry decodes either way,
  but the shading those routines apply cannot be read without splitting the
  actor overlays.
- **`0x100`'s extra refs.** It shifts the UV words later by 0, 2 or 3 words
  depending on family (§3.2.1), so it adds refs — but no opcode carrying it
  appears in any extracted model, so what they hold is unverified.
- **Import.** Writing a stream back needs the `handler_slot` written as it
  appears on disc rather than as the runtime pointer, and the tpage/clut bias
  (§5.1) undone.
- **The 37 unsourced streams.** 535 of the 572 located streams have a
  `TmdSource`; the rest are either real meshes reached from code the scan does
  not see, or remaining false positives. The validator checks structure, not
  semantics, so it cannot tell them apart. A short run in a package that is
  otherwise ~0% covered is the shape to distrust, and `MIN_PACKETS` is the
  remaining knob.

  This was 249 until the source matcher learned about **leading skip words**. A
  `TmdSource` may point at a stream that opens with one or more `0xFFFFFFFE`
  skips — `Tmd_InitSourceStream` steps over them before reading the first id —
  while the walker starts at the first packet, so the two addresses differ by
  the skips and an exact comparison threw the source away. The 41-packet body
  mesh in every named-human overlay is one of these: its source declares
  `0x15D4`, the first packet is at `0x15D8`.

---

## 7. Tooling

Reading a model is now end to end: `tmd_export.py` writes OBJ, and the two
corrections it needed are worth stating because both produce output that looks
plausible while being wrong.

**The transform pre-pass is not geometry, and its consumers are.** Skipping
`0xC0`/`0xC4`/`0xC8` is right (§3.5), but the opcodes with bit `0x01` must not
be skipped with them: they index the shading cache, which §3.5 shows is
`ws->field_10[offset >> 3]` — one word per *vertex*. So their refs are word
offsets into a vertex-keyed array and the index is `ref / 4`, not `ref / 8`.
Reading them as ordinary refs silently drops about a third of a character's
faces (270 of 398 on the named-human body).

**PlayStation quads are Z-ordered.** `v0 v1` across the top, `v2 v3` across the
bottom, so the polygon winding is `v0 v1 v3 v2`. Emitting them in index order
still references the right four vertices — the model looks almost right — but
every quad is a bow-tie and adjacent faces stop sharing edges. The tell is the
edge count: a closed mesh has `E = V + F - 2`, and the human accessories came
out at 80 edges against an expected 44 until the winding was fixed, after which
they are exact closed manifolds (χ = 2).

**Backface culling is `NCLIP`, not a normal test.** `Tmd_StreamHandler_Op38`
runs the projected points through `NCLIP` (`0x4B400006`) and drops the
primitive when `MAC0 <= 0` — `mfc2 $t0, $24` then `blez`. It never consults a
normal to decide visibility. The stored normals are the input to `NCCS`, which
is *lighting*: a normal here is a shading normal, not necessarily the geometric
face normal, so culling on it removes real surface. That is what makes heads
and legs vanish from an offline render while a single-part object like a hand
or a weapon still looks right.

So an offline renderer wants both: `MAC0 = x0(y1-y2) + x1(y2-y0) + x2(y0-y1)`
on the *projected* points for visibility, and the stored normal for shading.

**The winding is clockwise, so it cannot orient a face on its own either.**
Measured against the stored normal array in the file's raw coordinates,
`cross(v1-v0, v2-v0)` points *opposite* the element's own normal in 97-100% of
faces (aya 363/368, the named human 263/270, an actor 271/274, a weapon 45/45).
Take the normal from the normal ref. The pre-transformed opcodes spend their
trailing words on cache indices rather than normal refs, so those faces — about
a quarter of a character — fall back to the winding.

Negating Y to get from the PlayStation's Y-down space to a Y-up viewer is a
*reflection*, and a reflection reverses handedness, so in Y-up space the
winding-derived normal agrees with the (also flipped) stored normal and the two
can be mixed. It does **not** touch Z: `SZ3` grows with distance on the
hardware, so +Z stays *away* from the viewer and a painter's-algorithm sort
draws descending depth first.

The `ref / 4` divisor for the pre-transformed opcodes is confirmed the same
way. With `/4` the human body decodes to 398 faces, 0 rejected, every edge
shared by exactly two faces; with `/8` it is 279 faces, 119 rejected and only
71% of edges shared — and aya lands on χ = 2 exactly under `/4`.

| Tool | Role |
|---|---|
| `tools/peassets/pkg_model.py` | walks and delimits streams, resolves each to its `TmdSource`, carves them to `raw/model/*.tmd` |
| `tools/peassets/pkg_anim.py` | the animation side; see `ASSET_FORMATS.md` §9.2 |

Streams are stored raw only (`.tmd` is in `RAW_ONLY_EXTS`) because nothing
decodes them yet. 572 are located across 212 packages and dedup to 298 unique
files — 274 are meshes shared between packages.

Where a `TmdSource` is found (323 of the 572), the store's map entry also
carries what an exporter needs to read the mesh:

```json
{
  "model_source":  "pe2pkg_0.pe2pkg",
  "model_offset":  "0x0019C",
  "model_ops":     ["0x38", "0x78"],
  "source_offset": "0x00420",
  "verts_offset":  "0x0002C",
  "norms_offset":  "0x000E4",
  "vertex_count":  23,
  "normal_count":  23
}
```

The counts come from the gaps between the three pointers (§1), so they are
derived from the layout rather than declared anywhere.

Being source-backed is also the strongest precision signal available: a stream
no `TmdSource` points at is either a real mesh referenced from code the scan
cannot see, or a false positive. Rejecting the two opcodes that never begin a
real model (§2.1) removed most of the latter and made the scan roughly eight
times faster, since the walk is skipped rather than run and discarded.

## 8. Related docs

- [`ASSET_FORMATS.md`](ASSET_FORMATS.md) — chunk types, images, CLUTs, the
  store layout, and the animation format (§9).
- [`OVERLAYS.md`](OVERLAYS.md) — which package loads where, and what each RAM
  slot holds.
- [`STREAM_FORMATS.md`](STREAM_FORMATS.md) — CD audio and movie streams.
