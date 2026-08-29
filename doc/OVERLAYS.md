# Parasite Eve 2 — overlays (`.pe2pkg`)

What the 448 unique room packages actually are: RAM slots, stage-0 file-id
encoding, room vs actor vs weapon vs menu, and where meshes / clips live.
Derived from USA `stages.json` + inflated `pe2pkg/` / `pe2img/`, the matching
decomp (`fs.c`, `loadui.c`, `stage.c`, `tmd.c`, gameplay `1BC.c`), and scans
of the binaries (MIPS prologues, TMD-like streams, sibling textures, strings).

Related: on-disc chunk packing in [`ASSET_FORMATS.md`](ASSET_FORMATS.md);
TMD playback in `src/main/tmd.c` / `include/main/tmd.h`; animation player in
`src/gameplay/1BC.c` / `include/gameplay/1BC.h`; the cooperative actor list in
[`TASKS.md`](TASKS.md) (bank 6 callbacks live in the room overlay slot).

| Area | Code / data |
|------|-------------|
| Load addresses | `assets/USA/stages.json` (`type: room_pkg`, `load_addr`) |
| File tree / ids | `tools/peassets/asset_data.py` (`TREE`) |
| CD file lookup | `src/main/fs.c` (`Fs_LoadFile`, `Fs_InitStage0TablesCb`) |
| Room enter | `src/main/stage.c` (`Display_TaskLoadStep`), `src/main/loadui.c` (`CdCmd_EnqueueLoadFile`) |
| Inflated bodies | `assets/USA/pe2pkg/` (LZSS-decoded); on-disc in `raw/pe2pkg/` |
| Model stream | `Tmd_InitSourceStream` / `Tmd_ProcessStream` |
| Anim player | `GpAnimCtx` / `GpAnimSlot` / `GpAnimSet` (`1BC.c`) |

The formats themselves are documented in
[`TMD_FORMAT.md`](TMD_FORMAT.md) (the model packet stream and its opcodes) and
[`ASSET_FORMATS.md` §9](ASSET_FORMATS.md#9-models-and-animation) (the
`GpAnimSet` clip layout, and how gameplay's index tables attribute a block to
the package that owns it).

There is **no separate model or animation chunk type**. Both sit inside the
same `.pe2pkg` as code (or *instead* of code). Textures are sibling `.pe2img`
/ `.pe2clut` in the same stage-0 file.

---

## 1. Two worlds: stage 0 vs stages 1–5

| Container | What a “file” is | Overlay role |
|-----------|------------------|--------------|
| **STAGE0** | Flat library, ids encoded as base-100 digits | Global swap-ins: gameplay/title, Aya, weapons, actors, maps, menus |
| **STAGE1–5** | **Folders** (one room each) | Per-room script overlay at a single address |

`CdCmd` file identity is `idB2 * 10000 + idB1 * 100 + idB0`
(`CdCmdEntry` in `fs.h`). `Fs_LoadFile` switches on `req[2]` (the 10000s
place / category). Categories `0`–`5` and `90` have dedicated tables;
everything else (`10`, `20`, `30`, `40`, `80`, `90` as a *file id prefix*,
…) goes through `Fs_FileTable` with
`fileId = req[2]*10000 + req[1]*100 + req[0]`.

`D5B498_8006ADF4 = req[2] / 10` is the high “bank” digit used when looking
those larger ids up.

---

## 2. RAM map (gameplay-resident)

Slots are packed back-to-back in high RAM. Title and gameplay share
`0x80093800` (whichever is loaded wins). The others can sit together:

| Address | Occupant | Typical inflated size | Notes |
|---------|----------|----------------------:|-------|
| `0x80093800` | **gameplay** *or* **title** | 520 KiB / 5.2 KiB | Gameplay ends at `0x80115769` |
| `0x80115770` | Aya costume (`10200`–`10600`); also replay-bonus `21000` | ~30 KiB | Starts the instant gameplay ends |
| `0x8011D1C0` | Equipped weapon (`10301`–`10332`) | ~50–70 KiB | |
| `0x8012EF30` | Map-picture helper **or** `501xx` helper | 0.5–12 KiB | Mutually exclusive |
| `0x80131E20` | Actor slot 1 (`1xxxxx`) | ~50–90 KiB | Stride `0x18000` to next slot |
| `0x80149E20` | Actor slot 2 (`2xxxxx`) | same | `slot1 + 0x18000` |
| `0x80161E20` | Actor slot 3 (`3xxxxx`) | same | `slot2 + 0x18000` |
| `0x80167A70` | Named human (`800101`–`800105`) | ~55 KiB | |
| `0x80179950` | Area-map UI / room names (`900000`–`900005`) | ~10–15 KiB | Ends at `0x8017D5BC` |
| `0x8017D5C0` | **Room overlay** (stages 1–5) | ~12–40 KiB typical | |
| just after that room | Room `.pe2cap2` dialogue | per-room | e.g. `stage1/101` overlay is `0xB35C` bytes → cap2 at `0x80188920` |
| `0x801D4000` | Options (`20100`) | ~8 KiB | Open-menu only |
| `0x801D6000` | Tiny helper (`20600`) | ~1.4 KiB | Role unknown |

`4xxxxx` actor ids mostly alias **slot 1** (`0x80131E20`). Empty actor slots
are 4-byte stubs so the id still resolves.

---

## 3. Package layout (no PS-X EXE header)

Title / gameplay (splat’d) and the room/actor packages we have scanned share
the same flat order:

```text
[header / rodata]   function-pointer tables, jtbls, strings
[.text]             first real prologue … last jr $ra + delay
[trailing data]     scripts, TMD streams, clip tables, leftover
```

Some packages are **data-only** (Aya costumes, several humans, most weapons):
no `addiu $sp` / `jr $ra` at all. The header is then a skeleton / table, not
a task-func list.

Word 0 is an id, not a pointer count:

| Family | Word 0 |
|--------|--------|
| Title | `5` (then 5 phase-table pointers — coincidence with the count) |
| Gameplay | `4` |
| Aya costume | bone count (`46`, `44`, `41`, `7`) |
| Weapon `10301`–`10332` | `8` … `0x27` |
| Actor `100300` | `0x58` (relocated copy `200300` uses `0xB1`) |
| Room `stage1/101` … `2101` | `0x11D` … `0x131` (monotonic per room) |
| Map UI `900000`–`900005` | `0x118` … `0x11C` |
| `501xx` helpers | `0x30`, `0x31`, … one value per unique type |

---

## 4. Stages 1–5 — room overlays

### 4.1 Folder = room

Each STAGE1–5 folder is one room. **`file0` is the room bundle**, not only
the overlay:

| Chunk | Role |
|-------|------|
| `*.spk` (often two) | BGM / SFX bank |
| `*.pe2img` / `*.pe2clut` | Extra textures for that room |
| **`.pe2pkg` @ `0x8017D5C0`** | Room script / local state |
| **`.pe2cap2`** | Dialogue, packed against the overlay’s end |

`file1`, `file2`, … are mostly **camera-angle `.bs` backgrounds** (320×240)
plus the occasional extra image. File counts per room cluster around 4–12;
a few rooms exceed 40.

All 168 room overlays load at **`0x8017D5C0`**. One package per folder; all
binaries unique. Entering a room is `CdCmd_EnqueueLoadFile(0, 0, 4)`
(`stage.c` / `gameplay.c`), which fills the request from
`Game_Session->field_6` / `field_7` (folder / stage).

### 4.2 Size (inflated = RAM payload)

168 rooms, USA:

| | Inflated | On-disc LZSS |
|--|--------:|-------------:|
| Mean | **28.21 KiB** (28,883 B) | 16.30 KiB |
| Median | 25.49 KiB | 15.08 KiB |
| Min | 0.47 KiB (`stage1/2101`) | 0.36 KiB |
| Max | 114.38 KiB (`stage1/501`) | 68.82 KiB |
| Total | 4.63 MiB | 2.67 MiB |

README “14.0 KiB / 28.0 KiB …” figures are **sector-rounded on-disc** sizes
(ceil to 2 KiB), not the decoded overlay. Those average ~17.5 KiB.

| Stage | Count | Mean inflated | Mean on-disc |
|-------|------:|--------------:|-------------:|
| 1 | 21 | 43.80 KiB | 25.94 KiB |
| 2 | 33 | 22.01 KiB | 12.21 KiB |
| 3 | 32 | 26.21 KiB | 14.79 KiB |
| 4 | 49 | 31.65 KiB | 18.60 KiB |
| 5 | 33 | 21.29 KiB | 12.30 KiB |

### 4.3 Code vs data

MIPS span = first strong prologue (`addiu $sp,$sp,-N` / `jr $ra` / `jal`)
through the last `jr $ra` + delay. The same detector hits the known title
(`.text` `0x9C`–`0x140C`) and gameplay (`.text` `0x42C0`–`0x79208`) ranges
exactly. Spot-checks on rooms land on real epilogues; the bytes immediately
after are event records (`0x13EE` / pointer / `0x13F1` / … / `0x7FFFFFFF`).

Across all 168 rooms (weighted):

| Section | Total | Share | Mean / room |
|---------|------:|------:|------------:|
| `.text` | 2,131,384 B | **43.9%** | 12.4 KiB |
| Leading rodata | 25,676 B | 0.5% | 0.15 KiB |
| Trailing data (scripts / tables) | 2,695,300 B | **55.5%** | 15.7 KiB |

Per-overlay code fraction: min 2%, median 43%, max 90%. Stages 1–3 are
data-heavier (~33–36% code); 4–5 flip to ~54–55%. About 3,445 stack-frame
functions (`addiu $sp,$sp,-N`) in total, ~20 per room (leaf `jr $ra` stubs
not counted).

Title is ~94% code and gameplay ~91%. Room packages are the opposite mix.

Rooms are **not** 3D scenes. A minority (36/168) have a couple of tiny
TMD-like prim chunks — props or pickups at most.

---

## 5. Stage 0 — global library

### 5.1 Gameplay and title

| File | Load | Inflated | Role |
|------|------|--------:|------|
| `stage0/file0` (`gameplay`) | `0x80093800` | 532,329 B | Resident in-game overlay: actors, combat, field, inventory, **anim player**, Aya clip tables |
| `stage0/file1` (`title`) | `0x80093800` | 5,290 B | Title / demo / main menu |

Same address; swapped.

### 5.2 Aya costumes — `10200`–`10600` @ `0x80115770`

Four unique bodies (10500/10600 share one). **Data-only**: bone count +
identity `MATRIX` (`0x1000` = PS1 `ONE` on the diagonal) + TMD-like streams
(~60% of the package) + sibling face/body TIM.

| File | Bones | Notes |
|------|------:|-------|
| `10200` | 46 | Default black field outfit (`pe2img_4`) |
| `10300` | 7 | Reduced set (hands / first-person / similar) |
| `10400` | 44 | White-shirt costume (`pe2img_31`) |
| `10500` | 41 | Another full costume |

Shared walk / aim / hit clips live in **gameplay `.data`**
(`Gp_WeaponIdBase`, `Gp_AllyIdBase`, `Gp_PlayerAnimBlkTbl[field_93A]`, …). The costume
package only replaces skeleton + mesh + texture.

Also at this address: `21000` (replay-clear bonus: “Complete Bonus”,
“BONUS BP”, “You will lose the game clear data…”) and stub `20900`.

### 5.3 Weapons — `10301`–`10332` @ `0x8011D1C0`

32 packages, each with a gun TIM (M93R-style, P229-style, …) and often an
`.spk`. Header word `8`…`0x27`. Mesh is small (a few `0x38`/`0x78` chunks,
often &lt; 1 KiB). The rest of the ~55 KiB is clip tables (pointers back
into the same overlay, e.g. `10301` at `+0x774`) and keyframe-like
halfwords. Fire / reload / inspect are per-gun.

### 5.4 Actor / enemy characters — `1xxxxx` / `2xxxxx` / `3xxxxx`

Three **relocated copies** of the same character so three NPCs/enemies can
be resident at once:

| Prefix | Address | Delta from slot 1 |
|--------|---------|-------------------|
| `1` (`100300`, `100400`, …) | `0x80131E20` | 0 |
| `2` (`200300`, …) | `0x80149E20` | `+0x18000` |
| `3` (`300300`, …) | `0x80161E20` | `+0x30000` |

`100300` vs `200300`: identical 91,066-byte size, 40 stack prologues, almost
all words equal, remaining differences are KSEG0 pointers slid by exactly
`0x18000`. Unused slots are 4-byte stubs. Sibling `.pe2img` is the creature
skin (claws / chitin / …).

Of 196 unique actor binaries: 141 are code + TMD, 15 code-only, 2 data-only,
38 stubs. A typical 50–90 KiB package is roughly 20–30% TMD stream, the rest
code + clip / AI tables.

`4xxxxx` is more of this family, mostly loaded into slot 1.

### 5.5 Named humans — `800101`–`800105` @ `0x80167A70`

Same mesh language as actors, human face/clothes TIM (`pe2img_249`). Four
unique bodies.

### 5.6 Area maps — two cooperating slots

| Files | Address | Contents |
|-------|---------|----------|
| `30100`–`30504` | `0x8012EF30` | Map *pictures* (Akropolis, “2F DRYFIELD”, Neo Ark, …) + a 0.5–4 KiB metadata package |
| `900000`–`900005` | `0x80179950` | Map UI + room-name tables (“East elevator hall”, “Saloon G & R”, “Golem freezer 1”, “Oval Office”, …). One package per area |

`900000`/`900001` share a body (Akropolis / MIST names). `900002`/`900003`
are Dryfield variants.

### 5.7 `50100`–`50152` @ `0x8012EF30`

Same address as the map pictures, so they **replace** that helper. Music +
3–12 KiB of code, no TMD, no texture. Groups of three identical copies
with header `0x30`, `0x31`, … (~15 unique types). Best current fit:
enemy behaviour / sound helpers, not the models (those are the 50–90 KiB
actor slots). Not proven.

### 5.8 Menus sitting above the stack

| File | Address | Evidence |
|------|---------|----------|
| `20100` | `0x801D4000` | “Option”, “Key Configuration”, Vibration, Stereo/Mono, Walk, Draw Weapon, PE Menu, Type A/B/C, … |
| `20200` | `0x801D4000` | 4-byte stub in that slot |
| `20600` | `0x801D6000` | Tiny; role unknown |

---

## 6. Models and animations

### 6.1 What is *not* a model overlay

- Room `file1+` — `.bs` 320×240 plates.
- Room `.pe2pkg` — event tables + script code.
- Room `.pe2cap2` — dialogue.
- `301xx` / `900xxx` — map art + map names.
- `501xx` — code-only helpers.
- `20100` / title — menus.
- Gameplay — engine + Aya clip *tables*, almost no mesh (~0.8 KiB leftover prims).

If it walks or shoots, its mesh is in a stage-0 overlay at `0x80115770` /
`0x8011D1C0` / `0x80131E20+`.

### 6.2 Custom TMD-like stream (not retail `ID_TMD`)

Retail TMD magic `0x00000041` is not the container. The game walks its own
stream (`Tmd_InitSourceStream`):

```text
opcode, handler-slot, dims, payload words…
0xFFFFFFFE          object break
…
0xFFFFFFFF          end
```

`dims` is `(count_hi << 16) | count_lo`; payload length is
`count_hi * count_lo` words. Common opcodes: `0x38` / `0x78` / `0x3A` /
`0x7A`. Some (`0x8038`, `0x10038`, `0x20038`, …) jal into **handlers that
live in the actor overlay** (`D_80136224`, `D_8013700C`, … at
`0x80136xxx`), which is why those packages mix code and mesh.
`Tmd_InitSourceStream` patches the handler-slot word in place.

`TmdSource` (`tmd.h`) points at this stream via `field_20`. Type-1
`TaskDesc.setupArg` is a `TmdSource*` (`Task_SpawnFromDesc`).

### 6.3 Animation split

Playback is one system in gameplay (`Gp_AnimInitCtx` … `func_800B4754`,
`GpAnimCtx` / `GpAnimSlot` / `GpAnimSet`). What it *points at* depends on
who is moving:

| Who | Clip data |
|-----|-----------|
| **Aya** | Gameplay `.data`. `GameActor.field_928` ← `Gp_PlayerAnimBlkTbl[field_93A]` / `Gp_AnimBlkTbl[…]`. Costume overlay only swaps skeleton + mesh + TIM. |
| **Guns** | Pointer tables inside the **weapon** overlay (right after the mesh). |
| **Enemies / NPCs** | Mesh, clips, and AI in the **same** relocated actor binary. |

No anim chunk type exists in `STAGE*.CDF`. If it is not in that `.pe2pkg`
(or in gameplay `.data` for Aya), it is not on disc as its own asset.

---

## 7. How a room enter looks

1. Gameplay stays at `0x80093800`.
2. `Display_TaskLoadStep` → `CdCmd_EnqueueLoadFile(0, 0, 4)` reads
   `Game_Session->field_6` / `field_7` and loads that room’s `file0`.
3. The `0x8017D5C0` room overlay (and its cap2) always swap.
4. Aya / weapon / the three actor slots stay unless the room asks for
   different stage-0 ids.

---

## 8. Census (USA unique `.pe2pkg`)

448 unique inflated bodies. By `load_addr` (instance counts in
`stages.json`, including duplicate refs):

| Load | Count | Family |
|------|------:|--------|
| `0x80093800` | 2 | gameplay, title |
| `0x80115770` | 7 | Aya + replay-bonus + stubs |
| `0x8011D1C0` | 32 | weapons |
| `0x8012EF30` | 64 | map pictures + `501xx` |
| `0x80131E20` | 102 | actor slot 1 (+ `4xxxxx`) |
| `0x80149E20` | 40 | actor slot 2 |
| `0x80161E20` | 54 | actor slot 3 (+ some `8xxxxx`) |
| `0x80167A70` | 5 | named humans |
| `0x80179950` | 6 | map UI |
| `0x8017D5C0` | 168 | rooms |
| `0x801D4000` | 2 | options |
| `0x801D6000` | 1 | tiny helper |

---

## 9. Open questions

- Exact role of `501xx` (behaviour vs sound vs something else).
- What `20600` / `20900` and the 4-byte actor stubs are *for* at runtime
  (id presence vs real payload).
- Full `TmdSource` / bone-matrix stride for Aya (first `MATRIX` is identity;
  later bones are not a plain `0x20` `MATRIX` array).
- Whether room TMD specks are world props, inventory pickups, or noise.
- Friendly names for actor suffixes (`300`, `400`, `700`, …) — not yet
  matched to in-game character ids.
