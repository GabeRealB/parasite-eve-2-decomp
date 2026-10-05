# Parasite Eve 2 — TMD model format

What we know about the **model streams** inside the `.pe2pkg` overlay packages:
the container that ties a mesh together, the packet stream that describes its
faces, and what each opcode means. Derived from the matching decomp
(`src/main/tmd.c`, `src/main/hasm/Tmd_StreamHandlers_Ops.s`,
`src/gameplay/scene_runtime.c`) and from walking the 360 model streams the extractor
carves out of the retail USA discs.

These are **not** a stage chunk type. There is no model chunk on disc; a model
lives inside the room/actor package that uses it, so nothing in the CDF tree
points at one. See [`OVERLAYS.md`](OVERLAYS.md) for which package is which and
[`ASSET_FORMATS.md` §9](ASSET_FORMATS.md#9-models-and-animation) for how models
and animation sit together.

| Area | Code / tools |
|------|----------------|
| Stream walk + opcode switches | `src/main/tmd.c` (`_tmdResolveSourceDrawHandlers`, `tmdBuildBufferHalf`) |
| Early-image handlers | `src/main/hasm/Tmd_StreamHandlers_Ops.s` |
| Container types | `include/main/tmd_types.h` (`TmdSource`, `TmdObject`) |
| Attach path | `src/gameplay/model_objects.c` (`Gp_AttachTmd`), `src/main/task.c` |
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

## 1. A model's source data

The face stream alone is not a model: the vertices live outside it. A
`TmdSource` record ties the pieces together, and `Gp_AttachTmd` reaches one
through `TaskDesc.data.model`:

```text
TmdSource (0x24 bytes; handlersResolved is 0 on disc, set to 1 after first use)
  +0x00  s32     handlersResolved (0 unresolved, 1 resolved)
  +0x04  s32     bufferHalfBytes: byte capacity of one primitive-buffer half
  +0x08  s32     preXformRegionBytes: first-region capacity / second-region offset
  +0x0C  s32     part count
  +0x10  u32  -> partVertexCounts: vertex count per part, read-only metadata
  +0x14  u32  -> vertex array   8 bytes per entry (SVECTOR-shaped)
  +0x18  u32  -> normal array   same shape
  +0x1C  u32  -> skeleton      36 bytes per entry (TmdBone)
  +0x20  u32  -> face stream
```

`tmdBuildBufferHalf` copies those into its scratch as `workspace->verts` (vertices)
and `workspace->normals` (normals); the handlers index off them.

Each object allocates two `bufferHalfBytes` halves. The first
`preXformRegionBytes` bytes of each half hold pre-transformed primitives; the
remaining bytes hold primitives transformed directly when drawn. These fields
are buffer capacities, not serialized stream lengths. The half size must fit
the object's u16 cache, and the first region cannot exceed it.

Objects borrow the source record, geometry and stream. Geometry can be morphed
in place, and resolving the stream writes its handler slots. The skeleton is
copied only during creation. Stream-only sources may have NULL geometry and
vertex-count pointers; a model without normals can instead point `normals` at
the empty array boundary. Commands must not access absent geometry.

A model is laid out contiguously with the record last, so the counts fall out
of the gaps:

```text
[ skeleton ][ part vertex counts ][ vertices ][ normals ][ face stream ][ TmdSource ]
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
  u32 id            opcode; TMD_STREAM_END (0xFFFFFFFF) ends the stream,
                    0xFFFFFFFE ends one command group (see 2.2)
  u32 handler_slot  overwritten at runtime - see below
  u32 dims          (count << 16) | stride, stride in words
  u32 payload[stride * count]
```

`_tmdResolveSourceDrawHandlers` advances by `(dims >> 16) * (dims & 0xFFFF)` words. The
product is symmetric, so **delimiting** a stream works with the halves either
way round, but **parsing elements** does not: the high half is the count, the
low half the stride. The giveaway in real data is a CLUT-looking word recurring
at the stride interval — every 7 words in an `0x78` packet, not every 20.

`_tmdResolveSourceDrawHandlers` resolves each `id` to a handler and **writes the pointer
into `handler_slot`**, so a stream that has run once no longer matches its
on-disc form. Decode from the extracted file, never from a RAM dump.

The resolver uses the private `_TmdStreamWord` union to view the source's `u32`
storage as either `dataWord` or `drawHandler`. Only the second word of a command
holds a callback; group and stream terminators occupy one data word each.

### 2.2 `0xFFFFFFFE` terminates a command group

Reading it as "skip a word and carry on" merges the whole skeleton into one
coordinate frame. `Tmd_DispatchStream` **returns** when it reads `0xFFFFFFFE`
(`TMD_STREAM_GROUP_END`), handing the pointer at the marker back to its caller.
The marker occupies one word and has no handler slot, dimensions or payload.
The caller consumes that word and advances the coordinate slot even for an
empty group, so consecutive markers preserve empty parts:

```text
tmdDrawModelStream(workspace, objectFlags, stream, model):
    remainingParts = model->partCount
    part = model->coords                           # GfxCoord cursor
    loop:
        w = *stream
        if w == TMD_STREAM_END: return              # 0xFFFFFFFF; leave the word
        if w != TMD_STREAM_GROUP_END and remainingParts > 0:
            GTE rotation    <- part->workm.m
            GTE light       <- workspace->viewLightRotation * part->workm.m
            GTE translation <- part->workm.t
        stream = Tmd_DispatchStream(workspace, objectFlags, stream)
        stream += 1                                # u32 words; consume the marker
        remainingParts -= 1;  part += 1
```

The light-matrix composition uses GTE multiplication with 12 fractional bits
and signed IR saturation. The caller supplies projection settings, the colour
matrix and background colour, and initialized packet/depth/OT workspace state.
Neither the group walk nor its dispatcher initializes the workspace's saved
`gteFlag`; any command reading that word needs it supplied before the command.
Empty groups and groups beyond `partCount` leave the current GTE matrices in
place. Storage extents are unchecked, and emitted packets and the ordering
table must remain valid until the GPU finishes consuming them.

The groups corresponding to skeletal parts are drawn under their own bone
matrices, so **vertices only share a coordinate space within a part**. The
vertex array holds each part in its own local frame: in `aya_10200` the left
and right arm both span `x[-39, 40]`, sitting inside one another, and drawing all 19 parts
together gives a lump with no legs or head. Rendered one at a time the parts
are plainly a head, an upper arm, a forearm, a thigh, a shin and a foot.

Two consequences:

* **The part count is the bone count.** `TmdSource.partCount` is the number of
  skeleton entries, not the number of stream groups. Packaged streams have
  `partCount + 1` group terminators: one per skeleton part and one for a final
  group that can draw pre-transformed primitives without another matrix load.
  `aya_10200` is 19, the Kyle body 20, `actor_100300` 19. Parts with no
  geometry are joints.
* **The rest pose ships in the `TmdSource`.** The runtime matrices live in
  `TmdObject.coords`, a `GfxCoord` array — 0x50 bytes each, which is why
  the stride is 0x50, with `workm` at `+0x24` and its translation at
  `+0x38`/`+0x3C`/`+0x40` exactly as the handler reads them. But the skeleton
  those are built from is on disc, in three `TmdSource` fields:

  | Field | Meaning |
  |---|---|
  | `partCount` (`+0x0C`) | number of skeleton entries, copied to `TmdObject.partCount` |
  | `partVertexCounts` (`+0x10`) | `partCount` x u32: how many vertices each part owns |
  | `skeleton` (`+0x1C`) | `partCount` x `TmdBone` (0x24 bytes) — the rest pose |

  `TmdBone.local` is a `MATRIX` — a 3x3 rest rotation (identity on disc,
  `4096` = 1.0) and a `t[3]` translating from the parent — followed by the
  `s32` `TmdBone.parentIndex`. Every index is in `[0, partCount)`; a root names
  its own index and initially attaches to the view coordinate. `tmdCreateModel`
  copies the matrices into `TmdObject.coords`, so animation changes the runtime
  pose while the source skeleton remains unchanged.
  The `partVertexCounts` table describes the part-local vertex groups (352
  entries in total for `aya_10200`, 300 for the Kyle body). It does not prove
  the complete array extent: the tables in `actor_110300`, `actor_110800` and
  one `actor_311900` source sum to 357 while their vertex arrays contain 360
  entries. Use the established asset boundaries for total geometry lengths;
  neither total length is stored in `TmdSource`.

  Composing those through the parent the way `actorRenderComposeCoordChain` does —
  `workm.m = parent.workm.m * coord.m`, `workm.t = parent.workm.m * coord.t +
  parent.workm.t` — assembles the character. For Kyle it yields a
  pelvis at `y = -951`, a head at `-1594`, arms out to `x = ±211` and feet at
  `-108` with the root on the ground (`y` is down). `tmd_export.py` and the
  viewer both apply it, so exports and the Model tab show standing figures
  rather than a heap.

  Animation updates each bone's *local* transform and preserves its parent
  links: `_animationBlendRotation` writes the rotation in `GfxCoord.coord.m`
  and marks the cached composition stale, unless an unpacked pose is requested
  instead. Encoding 1 also updates local translation. Playback uses this
  same parent composition with the animated local transform, and an animation set
  carries exactly one track per bone (`ASSET_FORMATS.md` §9.3.1).

The final **command group** has no skeletal part: it can carry the
pre-transformed (`op & 0x01`) primitives, whose screen coordinates were written
by the earlier parts' `0xC8` passes under those parts' own matrices. They are already positioned, so
they cannot be drawn from raw vertices in any single frame.

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
they issue) and the draw handlers in `src/gameplay/model_objects.c` and
`src/gameplay/model_lighting.c` (which `POLY_*` type they build).

A caution learned the hard way: each handler loads *different* `ws` fields
into the same registers, so a register name means nothing on its own.
`tmdDrawStreamGt3` uses `$t6` for the vertex array; `tmdDrawStreamPrimGt3PreXform`
uses `$t6` for the per-vertex depth cache.

| Bit | Meaning | How it shows up |
|---|---|---|
| `0x40` | **quad** (4 corners) instead of triangle | `AVSZ4` instead of `AVSZ3`; every `AVSZ3` handler has an `AVSZ4` partner at `+0x40` |
| `0x20` | **gouraud** — one normal per corner | handler adds 3–4 refs to `$t5`; without it, exactly 1 |
| `0x10` | adds 3 words of tail | stride +3 on every pair differing only in this bit |
| `0x08` | **use a constant instead of a per-element value** — one word less | stride −1 on every such pair; `0xC0` carries a material-colour word where `0xC8` uses neutral RGB `0x00808080` (§3.5) |
| `0x10`+`0x08` | **textured** — net +2 words | XY stores 12 bytes apart (`POLY_*T*`) instead of 8; +2 on 7 of 8 pairs, the exception being `0x21`→`0x39` |
| `0x04` | **no per-vertex colour** — `POLY_F*` instead of `POLY_G*`, and no lighting at all in the transform pass | `0x18`→`0x1C` is `GT3`→`FT3`, `0x58`→`0x5C` is `GT4`→`FT4`; `0xC0`→`0xC4` drops `NCCS` and keeps only `RTPS` |
| `0x02` | ABR / semi-transparent variant | same handler and stride as the base opcode |
| `0x01` | **pre-transformed** — refs index the `0xC8` cache | handler reads screen coords already in the primitive buffer and only culls; refs are word offsets into `ws->szTable`, so the index is `ref / 4` (100% valid across every model) |
| `0x4000` | **two primitives per element** — a layered draw | §3.2.1 |
| `0x8000` `0x10000` `0x20000` | **alternate transform routine, supplied by the model's own package** — layout unchanged | §3.2.1 |
| `0x100` | **a colour per corner** — the element names one for each, not one for the element | one word more per corner: +2 on a triangle, +3 on a quad, nothing on a family already carrying no colour | §3.2.1 |

Reading `0x38` with this: `0x20` gouraud + `0x10`+`0x08` textured, no `0x40`,
so a textured gouraud triangle — a `POLY_GT3`, which is exactly what the
handler builds.

`0x10` and `0x08` are **separate bits**, not one "textured" flag: their stride
effects (+3 and −1) compose to the +2 seen when both are set, and each occurs
without the other (`0x30`, `0x70` carry `0x10` alone). Both produce a textured
primitive; what differs is what sits ahead of the UV words — `0x38`'s record is
lit from a constant and its refs run straight into its texture words, while
`0x30`'s carries a colour of its own between the two, putting the texture words
one word the later (§5.1).

Note that `0x20` and `0x04` are related but distinct: `0x20` controls how many
**normals are read** (lighting input), `0x04` controls whether the primitive
carries one colour or one per corner (`POLY_F*` vs `POLY_G*`, the output).

### 3.2.1 The high bits

They are not one thing. Comparing each pair's draw handler shows three
different mechanisms:

**`0x4000` — two primitives per element.** `tmdBuildStreamGt3LayeredBase` (`0x4038`)
reserves a pair of `POLY_GT3` slots per element and uses the same texture
initializer as `tmdBuildStreamGt3` (`0x38`) on only the second, opaque base slot.
That is a layered draw — the same face emitted twice, as an opaque base and a
semi-transparent layer that the transform handler links into the ordering table
after it. Verified on all four pairs (`0x38`, `0x78`, `0x39`, `0x79`):
primitives per element goes 1 → 2 with the UV word positions unchanged.

In this default path, the layer's texture coordinates do not come from the
element. Each pair has an alternate handler. For transform-region triangles,
`tmdBuildBufferHalf` selects `tmdBuildStreamGt3OffsetLayer` in stage 2 areas 15 and
16: both packets copy the element's texture words, with independent layer and
base page/CLUT displacements. The layer sets only ABR bit 5 after relocation,
preserving bit 6 (modes 1 or 3); it does not add the base offsets. The default
handlers differ by region: the transform-region ones fill the base alone and
leave the layer to the transform pass. The pre-transformed triangle builder
`tmdBuildStreamGt3PreXformEnvLayer` seeds the first slot's page with
`getTPage(0, GPU_BLEND_ADD, 960, 256)` and its CLUT with `getClut(256, 240)`.
It copies words 2..4 into the base's texture fields and adds only the base
displacements. Projection supplies the layer's U/V; its environment draw
handler replaces the seeded page with a direct-colour page, leaving the CLUT
stored but unused by that texture format.

`tmdBuildStreamGt4PreXformEnvLayer` does the same for the quad pair (`0x4079`):
the first slot receives `getTPage(0, GPU_BLEND_ADD, 960, 256)` / `getClut(256, 240)`
(`0x3F` / `0x3C10`), while the opaque base receives words 2..4 with the model's
base displacements. Its draw handler also replaces the layer page with a
direct-colour environment page. In stage 2 areas 15 and 16,
`tmdBuildStreamGt4PreXformOffsetLayer` copies those texture words to both slots
with independent layer/base offsets, setting only ABR bit 5 on the layer's
relocated page and retaining bit 6.

**`0x8000` / `0x10000` / `0x20000` — the model brings its own transform
routine.** These resolve to init handlers at `0x8013xxxx`, which is inside the
**actor package overlay**, not main or gameplay:

| Opcode | Init handler | Lives in | Packet builder |
|---|---|---|---|
| `0x38` | `tmdDrawStreamGt3` | main (hasm) | `tmdBuildStreamGt3` |
| `0x8038` | `D_80136224` | actor package | `tmdBuildStreamGt3` |
| `0x10038` | `D_8013700C` | actor package | `tmdBuildStreamGt3` |
| `0x20038` | `D_801379B4` | actor package | `tmdBuildStreamGt3` |

The packet builder — and therefore the element layout — is identical to the base
opcode. Only the transform/light routine changes, and it is supplied by the
package being drawn. That is why these bits never move the stride. What those
routines actually do is out of reach: they live in overlays this project does
not split.

**`0x100` — the element names a colour per corner.** The UV words move later by
a family-dependent amount: +2 words for `0x30`→`0x130`, +3 for `0x70`→`0x170`,
and +0 for `0x31`→`0x131` and `0x71`→`0x171`. What they move behind is the
element's colour: a family clearing `0x08` carries its material colour in the
element, and `0x100` makes that one word per corner instead of one for the
element. The early-image handlers are where this shows —
`tmdDrawStreamPrimGt3CornerColors` loads three such words into the GTE colour
register, one ahead of each corner's lighting step, and
`tmdDrawStreamPrimGt4CornerColors` loads four, each result stored into the matching
corner colour of the `POLY_GT3`/`POLY_GT4` packet they complete in place. The
families that gain nothing are the pre-transformed ones (bit `0x01`), whose
colours the `0xC8` pass writes into the primitive buffer, so their elements carry
none to shift.

### 3.3 How arity was established

From `AVSZ3` vs `AVSZ4`, and corroborated by where each handler stores screen
coordinates. Never from the hasm header comment, which is wrong in both
directions.

| Handler | XY stores | Primitive | Arity |
|---|---|---|---|
| `tmdDrawStreamPrimG3CornerNormals` | `0x8`, `0x10`, `0x18` | `POLY_G3` | triangle |
| `tmdDrawStreamGt3` | `0x8`, `0x14`, `0x20` | `POLY_GT3` | triangle |
| `tmdDrawStreamPrimG4CornerNormals` | `0x8`, `0x10`, `0x18`, `0x20` | `POLY_G4` | quad |
| `tmdDrawStreamGt4` | `0x8`, `0x14`, `0x20`, `0x2C` | `POLY_GT4` | quad |
| `tmdDrawStreamPrimGt4OneNormal` | `0x8`, `0x14`, `0x20`, `0x2C` | `POLY_GT4` | quad |

`tmdDrawStreamGt4` writes corner 3 before projecting corners 2, 1, 0. When
corner 3 is the preceding element's corner 0, it reuses that projection instead.

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

`0xC0` and `0xC8` use the same projection, lighting and scatter algorithm,
with bit `0x08` dropping the per-element colour word in favour of a constant.
Their destination words consequently occupy different element positions.
The handwritten expansions stay separate: `0xC0` loads that destination word
before it caches the depth, and its scatter tail decodes the word's halves
into the opposite temporaries from `0xC8`.

`0xC4` (`0xC0 | 0x04`) skips lighting entirely, matching
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

The pass also writes a **per-vertex depth cache** at `ws->szTable`, and that
is what settles the `ref / 4` divisor of §3.4 from the source rather than by
inference. `tmdXformStreamVerts` stores the `RTPS` result at byte address
`(u8*)workspace->szTable + (vertex_byte_offset >> 1)`, equivalently
`&workspace->szTable[vertex_byte_offset >> 3]`, so the cache holds one word
per vertex; `tmdDrawStreamPrimGt3PreXform` then reads its refs at byte addresses
`(u8*)workspace->szTable + ref` and feeds them to `SZ1`/`SZ2`/`SZ3`. Halving an 8-byte
stride gives 4, so a pre-transformed ref is `vertex_index * 4` and the cache
slot maps to a vertex one-to-one. The draw handler's negative-value check
(`bltz` on the loaded word) tests `TMD_VERTEX_DEPTH_INVALID`. `tmdXformStreamVerts`
sets that bit when GTE FLAG bit 31, `TMD_GTE_ERROR_FLAG`, is set.

`tmdXformStreamVerts` leaves the workspace's counts, saved GTE words and packet
cursors unchanged. Its packet destinations are relative to the current
`workspace->preXformWrite`, and it writes both XY and colour even on projection
failure. Projection reuse retains a failed vertex's coordinates and negative
cached depth too; the later primitive handler decides whether to link it.
The element walk's branch delay slot also reads the word immediately after the
payload, including for a record with zero elements, so that word must be
readable (normally the next opcode or group marker).

What either handler leaves at a destination is the GTE's colour register, and
that is a whole primitive colour word: the R, G and B the lighting produced,
with the top byte the colour source carried — the element's word on `0xC0`, the
constant on `0xC8`. The byte a packet draws with is not either of those: the
handler that completes a pre-transformed record (`0x39`, `0x79`) runs after the
pre-pass and writes its own code over the first corner's.

`0xC4`'s handler is decompiled C (`tmdXformStreamVertsUnlit` in
`src/gameplay/model_lighting.c`). The handwritten pre-passes do this depth
update from a fresh FLAG read (`cfc2` $31, then `bgez`); this handler tests
the saved `gteFlag` word as the walk left it:

```c
vertexRef = elementHalfwords[0];
if (vertexRef != previousVertexRef) {                                  // the caching branch
    gte_ldv0((const u8*)workspace->verts + (vertexRef & TMD_STREAM_GEOMETRY_BYTE_OFFSET_MASK));    // vertex array, 8-byte aligned
    gte_rtps();
    gte_stsz(&workspace->gteResult);                        // keep Z
    if (workspace->gteFlag & TMD_GTE_ERROR_FLAG)
        workspace->gteResult |= TMD_VERTEX_DEPTH_INVALID;   // reject using the saved FLAG
    workspace->szTable[elementHalfwords[0] >> TMD_STREAM_VERTEX_INDEX_SHIFT] = workspace->gteResult;  // cache[vertex index]
}
gte_stsxy(workspace->preXformWrite + elementHalfwords[1]);                    // screen XY into the prim
```

Three things fall out of it:

- **The cache is `ws->szTable` indexed by `offset >> 3`** — the vertex index —
  and holds one word each. That independently confirms the bit `0x01` reading
  in §3.2: those opcodes' refs are byte offsets into this word array, so the
  vertex index is `ref / 4`.
- **A negative cache entry marks a rejected projection.** Its sign bit is
  `TMD_VERTEX_DEPTH_INVALID`; the low 16 bits still hold the GTE screen Z.
  The pre-pass sets the bit when it tests `TMD_GTE_ERROR_FLAG` in FLAG, and
  the `0x01` handlers `bltz`-test the entry and skip the face if any corner
  carries it. For `0xC4`, the decision uses the previously saved FLAG rather
  than the current projection's hardware FLAG.
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
| `_tmdResolveSourceDrawHandlers` | draw callbacks in the early image, gameplay and actor403600 overlays | one-shot while `TmdSource.handlersResolved` is zero. Resolves 61 supported opcodes, with five location-dependent alternates, and **writes each callback into the command's slot word**; it invokes none. Unknown combinations resolve to `tmdSkipStreamRecord`. Later creations reuse the cached choices. Offset-layer callbacks are selected only in Dryfield by day's toilet; actor callbacks require the actor403600 overlay at draw time. |
| `tmdBuildBufferHalf` | the loaded overlay, `0x8009xxxx`, decompiled in `src/gameplay/model_objects.c` and `src/gameplay/model_lighting.c` | walks the stream when a model's primitives are built, and again when the model's texture page or CLUT changes: it lays the primitives out and fills their **static** fields — UV, CLUT, tpage. It picks the handler from the record's own opcode and steps over the slot word, which is the draw pass's to read. |

For supported untextured Gouraud records, construction reserves space without
initializing packet fields. `modelLightingReserveStreamPrimG3` and
`modelLightingReserveStreamPrimG4` only advance their
primitive cursors by one `POLY_G3` (`sizeof(POLY_G3)`, `0x1C`) and
one `POLY_G4` (`sizeof(POLY_G4)`, `0x24`), respectively, and step the stream
cursor by the stride in u32 words. An untextured
`POLY_G3`/`POLY_G4` has
no UV to refresh, so there is nothing for that handler to copy — and their prim
advance is what confirms the primitive type for opcodes whose handler names no
`POLY_*`.

`tmdBuildBufferHalf` also confirms the `dims` split independently of the
empirical evidence in §2:

```c
workspace->elemStride = ((u16*)stream)[0];   // stride, added to the handler's cursor per element
workspace->elemCount = ((u16*)stream)[1];   // count, the handler's loop counter
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
and where its texture coordinates come from. Read out of the handlers: some of
the bodies are the early image's own, the rest are decompiled in
`src/gameplay/model_objects.c` and `src/gameplay/model_lighting.c`. The early
image resolves those by absolute address through `configs/USA/sym.main.imports.txt` — which is why they are easy
to miss.

"Refs" is what precedes the UV words: the ref block — `nv` vertex offsets then
`nn` normal offsets, packed two per word — plus, in the families that light from
the element's own colour rather than from a constant, a colour word after it.
Bit `0x08` is what decides which: a family clearing it carries the colour, and
one setting it is lit from a constant (§3.2). The column counts that word with
the normals, since what the UV offsets measure is the block. The block is
derived — the first UV word marks its end — and it agrees with the 100%
range-check in §3.1.

| Base | Primitive | Corners | Refs | UV words | Opcodes | Elements |
|---|---|---|---|---|---|---:|
| `0x0` | POLY_G3 | 3 | — | — | `0x0` `0x20` `0x120` `0x4000` `0x4020` `0x4120` | 146169 |
| `0x4` | POLY_F3 | 3 | — | — | `0x4` | 52 |
| `0x5` | POLY_F3 | 3 | 3v (cache) | — | `0x5` | — |
| `0x18` | POLY_GT3 | 3 | 3v + 1n | u0=w2 u1=w3 u2=w4 lo | `0x18` `0x1A` | 128 |
| `0x1C` | POLY_FT3 | 3 | 3v + 1n | u0=w2 u1=w3 u2=w4 lo | `0x1C` `0x1E` | 357 |
| `0x30` | POLY_GT3 | 3 | 3v + 3n, colour | u0=w4 u1=w5 u2=w6 lo | `0x30` | 28 |
| `0x31` | POLY_GT3 | 3 | 3v (cache) | u0=w2 u1=w3 u2=w4 lo | `0x31` `0x39` `0x3B` `0x131` `0x8039` | 6395 |
| `0x38` | POLY_GT3 | 3 | 3v + 3n | u0=w3 u1=w4 u2=w5 lo | `0x38` `0x3A` `0x8038` `0x10038` `0x1003A` `0x20038` | 13925 |
| `0x40` | POLY_G4 | 4 | — | — | `0x40` `0x60` `0x160` `0x4040` `0x4060` `0x4160` | 173 |
| `0x44` | POLY_F4 | 4 | — | — | `0x44` | 122 |
| `0x45` | POLY_F4 | 4 | 4v (cache) | — | `0x45` | — |
| `0x58` | POLY_GT4 | 4 | 4v + 1n | u0=w3 u1=w4 u2=w5 lo u3=w5 hi | `0x58` `0x5A` | 407 |
| `0x5C` | POLY_FT4 | 4 | 4v + 0n | u0=w2 u1=w3 u2=w4 lo u3=w4 hi | `0x5C` `0x5E` | 417 |
| `0x70` | POLY_GT4 | 4 | 4v + 4n, colour | u0=w5 u1=w6 u2=w7 lo u3=w7 hi | `0x70` | 22 |
| `0x71` | POLY_GT4 | 4 | 4v (cache) | u0=w2 u1=w3 u2=w4 lo u3=w4 hi | `0x71` `0x79` `0x7B` `0x171` `0x8079` | 3566 |
| `0x78` | POLY_GT4 | 4 | 4v + 4n | u0=w4 u1=w5 u2=w6 lo u3=w6 hi | `0x78` `0x7A` `0x8078` `0x10078` `0x20078` | 13103 |
| `0x130` | POLY_GT3 | 3 | 3v + 3n, 3 colours | u0=w6 u1=w7 u2=w8 lo | `0x130` | — |
| `0x156` | POLY_GT4 | 4 | 4v + 8n | u0=w6 u1=w7 u2=w8 lo u3=w8 hi | `0x156` | 16 |
| `0x170` | POLY_GT4 | 4 | 4v + 4n, 4 colours | u0=w8 u1=w9 u2=w10 lo u3=w10 hi | `0x170` | — |
| `0x4038` | POLY_GT3 | 3 | 3v + 3n | u0=w3 u1=w4 u2=w5 lo | `0x4038` | 460 |
| `0x4039` | POLY_GT3 | 3 | 3v (cache) | u0=w2 u1=w3 u2=w4 lo | `0x4039` | 539 |
| `0x4078` | POLY_GT4 | 4 | 4v + 4n | u0=w4 u1=w5 u2=w6 lo u3=w6 hi | `0x4078` | 376 |
| `0x4079` | POLY_GT4 | 4 | 4v (cache) | u0=w2 u1=w3 u2=w4 lo u3=w4 hi | `0x4079` | 117 |

Each handler copies the UV words straight into the primitive and then biases
the page registers:

```c
poly->tpage += ws->texturePageOffset; // from TmdObject.texturePageOffset
poly->clut  += ws->encodedClutOffset; // signed TmdObject.clutRowOffset scaled by 64
```

So the stored `tpage`/`clut` are **relative** — the object's texture-page
displacement is added when building the primitive buffer, and the CLUT
displacement is the object's signed row count scaled by `1 << TMD_ENCODED_CLUT_ROW_SHIFT`
(the row field of a GPU CLUT word, 64 per row). An exporter has to apply the same bias to
resolve a real page.

`u0` and `u1` are written as full words, so each carries a `u`,`v` pair plus
the `clut` (in `u0`) or `tpage` (in `u1`) halfword, exactly as `POLY_GT3` /
`POLY_GT4` lay them out. `u2`/`u3` are halfword writes — UV only.

### Records the build pass steps over

`tmdBuildBufferHalf` — the pass that lays a record's packets out — has no entry for
these opcodes, so it walks past their elements and builds nothing for them. That
says nothing about what a frame draws from the record: `_tmdResolveSourceDrawHandlers`
still resolves a handler into it, and the draw walk (`Tmd_DispatchStream`) runs
that handler every frame like any other. So each of these opcodes does draw, and
what it draws is its own handler's to say; the work is per-frame by nature — a
transform, a cull, a packet's filing and its ordering-table link.

| Opcode | Init handler | Stride | Elements | Role |
|---|---|---:|---:|---|
| `0x21` | `tmdDrawStreamPrimG3PreXform` | 2 | 2 | pre-transformed opaque `POLY_G3`: three u16 byte offsets address the depth cache; positive winding and depths without `TMD_VERTEX_DEPTH_INVALID` permit an `AVSZ3` OT link. Every element consumes 28 packet bytes; object blend/reverse-culling flags are ignored — **solved**, §3.5 |
| `0x22` | `tmdDrawStreamPrimG3CornerNormals` | 4 | 8 | per-corner-lit `POLY_G3`; the element's RGB/code word supplies blending, and drawing consumes one packet slot per element despite skipped construction |
| `0x61` | `tmdDrawStreamPrimG4PreXform` | — | — | pre-transformed opaque `POLY_G4`: four u16 byte offsets address the depth cache. Facing requires `NCLIP(0,1,2) > 0` or `NCLIP(1,2,3) < 0`; all four depths must lack `TMD_VERTEX_DEPTH_INVALID` for an `AVSZ4` OT link. Every element consumes 36 packet bytes; object blend/reverse-culling flags are ignored — **solved**, §3.2 |
| `0x62` | `tmdDrawStreamPrimG4CornerNormals` | 5 | 26 | per-corner-lit `POLY_G4`: four u16 vertex byte offsets, four u16 normal byte offsets, then one RGB/code word; shares the `0x60` draw body. The material command byte supplies blending. Drawing consumes 36 packet bytes per element despite skipped construction, including rejected quads |
| `0xC0` | `tmdXformStreamVertsElemColor` | 3 | 6 | vertex transform + lighting pre-pass, colour per element — **solved**, §3.5 |
| `0xC4` | `tmdXformStreamVertsUnlit` | — | — | the `0xC8` pre-pass with the lighting dropped; never seen in data — **solved**, §3.5 |
| `0xC8` | `tmdXformStreamVerts` | 2 | 30262 | vertex transform + lighting pre-pass — **solved**, §3.5 |
| `0x121` | `tmdDrawStreamPrimG3PreXform` | — | — | resolves to the same opaque `0x21` handler: both retain the corner RGB already written by the vertex pass and read only the three depth-cache offsets |
| `0x122` | `tmdDrawStreamPrimG3CornerColorsSemiTrans` | — | — | three vertex and three normal byte references, then three corner RGB/code words; lights a semitransparent G3 independently per corner. Construction skips this opcode, but drawing still consumes one packet slot per element |
| `0x161` | `tmdDrawStreamPrimG4PreXform` | — | — | resolves to the same opaque `0x61` handler: both retain corner RGB already written by the projection pass and read only the four depth-cache offsets |
| `0x162` | `tmdDrawStreamPrimG4CornerColorsSemiTrans` | — | — | the `0x60` quad's record with the per-corner colour bit, in its semi-transparent form: a vertex, a normal and a colour per corner, so each corner is lit from the pair it names — read from the handler, never seen in data |
| `0x40C8` | `tmdXformStreamVertsEnvLayer` | 2 | 1568 | vertex/normal references plus layer/base colour-group byte destinations; projects both corners, caches depth, lights both and supplies environment UVs and a temporary 0/1 page marker. Stage 2 area 16 resolves to the offset-layer pre-pass instead |
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
- **The shifted families' records.** `0x100` adds the element's per-corner
  colours (§3.2.1), which settles what the extra words are, but only one opcode
  carrying it (`0x156`) occurs in the extracted models, at 16 elements. The
  `0x130` and `0x170` layouts are read from the handlers alone, so their corner
  colours have not been seen in data. The untextured corner-normals family's
  shifted forms (`0x120`, `0x160`, `0x162`) are read that way too, and none of
  the three occurs in the extracted models at all.
- **Import.** Writing a stream back needs the `handler_slot` written as it
  appears on disc rather than as the runtime pointer, and the tpage/clut bias
  (§5.1) undone.
- **Where models are is settled.** The overlay manifest declares every model
  with its `TmdSource` record (764 across 225 packages), and the build checks
  each record decodes and that its model object is exactly the arrays and stream
  it points at. A record may point at a stream that opens with one or more
  `TMD_STREAM_GROUP_END` words (`0xFFFFFFFE`), closing empty groups -
  `_tmdResolveSourceDrawHandlers` steps over them - so the stream's
  first packet can sit past the address the record declares; the Kyle body mesh
  declares `0x15D4` and its first packet is at `0x15D8`. The streams an old
  opcode walk found without a record were 35, and none is open: 29 are models
  whose record the walk's `verts < norms < stream` test rejected, because they
  carry no normals (`norms == stream`) - flat-shaded props, including the
  geometry in 10 `mappic` packages - and they are declared like every other
  model; the other 6 are runs of opcode-0 packets with no record behind them.
  Every `mappic` package is nothing but such models, each followed by its
  record.

---

## 7. Tooling

Reading a model is now end to end: `tmd_export.py` writes OBJ, and the two
corrections it needed are worth stating because both produce output that looks
plausible while being wrong.

**The transform pre-pass is not geometry, and its consumers are.** Skipping
`0xC0`/`0xC4`/`0xC8` is right (§3.5), but the opcodes with bit `0x01` must not
be skipped with them: they index the shading cache, which §3.5 shows is
`ws->szTable[offset >> 3]` — one word per *vertex*. So their refs are word
offsets into a vertex-keyed array and the index is `ref / 4`, not `ref / 8`.
Reading them as ordinary refs silently drops about a third of a character's
faces (270 of 398 on the Kyle body).

**PlayStation quads are Z-ordered.** `v0 v1` across the top, `v2 v3` across the
bottom, so the polygon winding is `v0 v1 v3 v2`. Emitting them in index order
still references the right four vertices — the model looks almost right — but
every quad is a bow-tie and adjacent faces stop sharing edges. The tell is the
edge count: a closed mesh has `E = V + F - 2`, and Kyle's accessory meshes came
out at 80 edges against an expected 44 until the winding was fixed, after which
they are exact closed manifolds (χ = 2).

**Backface culling is `NCLIP`, not a normal test.** `tmdDrawStreamGt3`
runs the projected points through `NCLIP` (`0x4B400006`) and drops the
primitive when `MAC0 <= 0` — `mfc2 $t0, $24` then `blez`. It never consults a
normal to decide visibility. The stored normals are the input to `NCCT`, which
is *lighting*: a normal here is a shading normal, not necessarily the geometric
face normal, so culling on it removes real surface. That is what makes heads
and legs vanish from an offline render while a single-part object like a hand
or a weapon still looks right.

The walk is laid out twice, and one bit of the drawing object's `flags` picks
between the copies: the second drops on `MAC0 >= 0` instead. A model the room
mirror draws as a reflection asks for it — the mirroring transform reverses the
model's faces, so the sign that means "front" is the other one.

So an offline renderer wants both: `MAC0 = x0(y1-y2) + x1(y2-y0) + x2(y0-y1)`
on the *projected* points for visibility, and the stored normal for shading.

**The winding is clockwise, so it cannot orient a face on its own either.**
Measured against the stored normal array in the file's raw coordinates,
`cross(v1-v0, v2-v0)` points *opposite* the element's own normal in 97-100% of
faces (aya 363/368, Kyle 263/270, an actor 271/274, a weapon 45/45).
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
way. With `/4` Kyle's body decodes to 398 faces, 0 rejected, every edge
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
