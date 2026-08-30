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
| `0x8012EF30` | Map-picture helper **or** PE effect | 0.5–12 KiB | Mutually exclusive |
| `0x80131E20` | Actor slot 1 (`1xxxxx`) | ~50–90 KiB | Stride `0x18000` to next slot |
| `0x80149E20` | Actor slot 2 (`2xxxxx`) | same | `slot1 + 0x18000` |
| `0x80161E20` | Actor slot 3 (`3xxxxx`) | same | `slot2 + 0x18000` |
| `0x80167A70` | Kyle (`800101`–`800105`) | ~55 KiB | |
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

Some packages are **data-only** (Aya costumes, three of Kyle's, most weapons):
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
| `501xx` PE effects | `0x30`–`0x3E`, one value per spell / item effect |

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
binaries unique.

**Room names.** The map-UI overlays carry a packed pool of display names, and
the pool is indexed **positionally against the stage's folders in order** —
name *i* is the *i*-th folder, not folder `<i>01`. (Folder-number indexing
cannot work: stage 2 runs up to folder `3801` but has only 26 names.) Two
boundaries confirm it independently, because two stages hold more than one
area and the areas change at exactly the right index:

| Stage | Areas | Folder split | Name at the boundary |
|---|---|---|---|
| 1 | Acropolis, MIST | `101`–`1701`, then `1801`+ | name 17 `West elevator hall` → 18 `MIST parking` |
| 5 | Shelter 1F, Neo Ark | `101`–`601`, then `701`+ | name 6 `Guardroom` → 7 `Observatory` |

Stage 4's own split falls out of the same alignment: names 1–8 are the mine
(`Mesa` … `Secret passage`), 9 onward the shelter B1–B6 (`Elevator hall` …).
Rooms 1–35 are confirmed — the mine, then shelter B1 and B2 — so stage 4 has no
gaps at all.

The shelter is named by floor, since several of its names repeat across floors,
and the floor is what tells the two apart:

| Floor | Rooms |
|---|---|
| 1F | 38 |
| B1 | 9–21, 23–25 |
| B2 | 22, 26–35 |
| B3 | 39–42 |
| B4 | 43–46 |

Room 22 (`Pod bottom`) sitting on B2 between B1 rooms is not an anomaly — it is
the foot of the pod, reached from the B1 service gantry. Each room carries its
`floor` in the manifest, so `shelter_b1_main_corridor` and
`shelter_b2_main_corridor` are distinct without an id fallback.

Stage 4 rooms 1–35 are all confirmed, which means the stage has **no gaps**: its
35 names map one-for-one onto its first 35 rooms.

Rooms 36–49 lie past the end of that pool, and the stage-4 map does not name them
at all. They are not unnamed *rooms*, just rooms its own map ignores: room 38 is
the 1F heliport, which stage 5's map names, and rooms 39–42 are the B3
incinerator floor, which no map pool names. Both carry a `note` recording where
the name came from, since neither is derivable from the data here.

**The alignment has interior gaps.** A room with no map cell consumes a folder
but no name, and every name after it shifts. Nothing in a package marks such a
room — folder `1801` in stage 1 has an ordinary file count, an ordinary number
of backgrounds and an ordinary size — so a gap cannot be spotted from one stage
alone. Stage 1 has one at `1801`, which is why `MIST parking` is `1901`.

**Stages 2 and 3 pin each other.** They are the same town by day and by night,
and two facts make the day map derivable rather than guessed:

* stage 3's rooms are stage 2's **minus room 4** — the numbering is shared;
* the day pool is an exact **subsequence** of the night pool, missing precisely
  `Motel room 3`, `Motel room 4`, `Motel lobby`, `Motel room 5` and `Motel loft`
  — the motel rooms that are not open during the day.

So a stage-2 room takes its stage-3 name when the day pool also has it, and is a
gap otherwise. That structure alone does **not** pin the alignment: every choice
of where stage 3's single gap sits produces a valid fit, consuming all 26 day
names in the right order — it just moves which rooms are the gaps. What it does
give is a strong lever, because one known gap in either stage determines the
whole of both.

Two known cutscene rooms fix it: rooms **4** and **8** are unnamed by day. Room 4
does not exist in stage 3 at all, so room 8 must be stage 3's single gap, and
that solution is unique. Everything then falls out coherently:

* rooms 4 and 8 are unnamed in **both** visits — the cutscene rooms;
* every other room carries the **same** name in both;
* the five rooms the day map does not label (13, 14, 17, 28, 31) are exactly the
  five night-only names — `Motel room 3`, `Motel room 4`, `Motel lobby`,
  `Motel room 5`, `Motel loft` — the motel rooms that are shut during the day;
* rooms 11–14 are Motel rooms 1–4 and 28–31 the rest of the motel, so the
  numbering clusters where the geography does.

A room is named for **what it is**, not for whether a given map labels it. Those
five stage-2 packages *are* motel rooms 3–5, the lobby and the loft, so they take
those names and are flagged `day_unlabelled = true` in the manifest rather than
falling back to an id. Only rooms 4 and 8 have no name at all. After that, the
two stages agree on every one of their 32 shared rooms.

Packages are named `<area>_<room>` (`acropolis_cafeteria`,
`dryfield_night_motel_loft`, `neo_ark_island`), with the floor inserted where an
area has several (`shelter_b2_main_corridor`). **156 of 168** are named.

A room is named for **what it is**, not for what a given map happens to label.
Several names therefore come from outside the room's own stage: the day-locked
motel rooms take their names from the night map, stage 4's `Heliport` from
stage 5's, and the shelter's B3/B4 floors from no map at all. Each such entry
records where its name came from in a `note`.

The remaining 12 are cutscene or one-off rooms that no map names — five of them
identified as such by the derivation itself (they are the holes that make the
alignment work), the rest by inspection. They keep `<area>_r<nn>` and say so in
a `note`, so an unnamed room reads as a finding rather than as unfinished work. Entering a room is `CdCmd_EnqueueLoadFile(0, 0, 4)`
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

**Which package is which gun.** `Gp_EnqueueItemPreviewCd`
(`src/gameplay/3688.c`) maps an equipped item to its overlay:

```c
if ((u32)(arg0 - 0x80) < 0x20U) {   // the 32 weapon item ids
    type  = 1;
    index = arg0 - 0x7F;            // → file 10300 + index
```

so **item `0x80 + n` is package `10301 + n`**, and the name comes from
`Gp_ItemDescs[id].field_4`. Three independent facts agree with that mapping,
which is what rules out an off-by-one:

* The variant groups line up. `10316`/`10320`/`10321` share one animation
  block and land on `M4A1 Rifle` / `M4A1(+1)` / `M4A1(+2)`; `10330`–`10332`
  share another and land on `MP5A5` / `MP5A5(+1)` / `MP5A5(+2)`; `10301` and
  `10304` share a third and are `P08(S. Magazine)` / `P08` — one gun, two
  magazines.
* Six descriptors in `0x80`–`0x9F` are **fully zeroed** — unused weapon slots
  (`0x85`–`0x87`, `0x89`, `0x91`, `0x97`).
* Five packages are byte-for-byte the same size (53 332) and share a single
  placeholder animation block: `10307`, `10308`, `10310`, `10318`, `10324` —
  exactly five of those six zeroed slots. None of the six contains a single
  function; they are pure data.

The overlays are named after the weapon, in `configs/USA/overlays.toml`:

| Pkg | Item | Weapon | Overlay | | Pkg | Item | Weapon | Overlay |
|---|---|---|---|---|---|---|---|---|
| 10301 | `0x80` | P08(S. Magazine) | `p08_snail` | | 10317 | `0x90` | M249 | `m249` |
| 10302 | `0x81` | M93R | `m93r` | | 10318 | `0x91` | — | `unused_91` |
| 10303 | `0x82` | M950 | `m950` | | 10319 | `0x92` | Tonfa Baton | `tonfa_baton` |
| 10304 | `0x83` | P08 | `p08` | | 10320 | `0x93` | M4A1(+1) | `m4a1_p1` |
| 10305 | `0x84` | P229 | `p229` | | 10321 | `0x94` | M4A1(+2) | `m4a1_p2` |
| 10306 | `0x85` | — | `unused_85` | | 10322 | `0x95` | Hypervelocity | `hypervelocity` |
| 10307 | `0x86` | — | `unused_86` | | 10323 | `0x96` | Gunblade | `gunblade` |
| 10308 | `0x87` | — | `unused_87` | | 10324 | `0x97` | — | `unused_97` |
| 10309 | `0x88` | Mongoose | `mongoose` | | 10325 | `0x98` | M4A1 Hammer | `m4a1_hammer` |
| 10310 | `0x89` | — | `unused_89` | | 10326 | `0x99` | M4A1 Bayonet | `m4a1_bayonet` |
| 10311 | `0x8A` | Grenade Pistol | `grenade_pistol` | | 10327 | `0x9A` | M4A1 Grenade | `m4a1_grenade` |
| 10312 | `0x8B` | MM1 | `mm1` | | 10328 | `0x9B` | M4A1 Pyke | `m4a1_pyke` |
| 10313 | `0x8C` | PA3 | `pa3` | | 10329 | `0x9C` | M4A1 Javelin | `m4a1_javelin` |
| 10314 | `0x8D` | SP12 | `sp12` | | 10330 | `0x9D` | MP5A5 | `mp5a5` |
| 10315 | `0x8E` | AS12 | `as12` | | 10331 | `0x9E` | MP5A5(+1) | `mp5a5_p1` |
| 10316 | `0x8F` | M4A1 Rifle | `m4a1` | | 10332 | `0x9F` | MP5A5(+2) | `mp5a5_p2` |

Code is a rounding error here: 124 symbols across all 32, and `hypervelocity`
alone has 16 of them. The other ~98% of each package is model and animation
data.

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

### 5.5 Kyle — `800101`–`800105` @ `0x80167A70`

One character, not four. Same mesh language as actors, with a shared
face/clothes TIM (`pe2img_249`).

The four packages carry the *byte-identical* 300-vertex body, the same four
small meshes (two 27- and two 23-vertex) and the same 20-bone skeleton. Each
adds one mesh of its own — 22, 52, 36 and 78 vertices — plus its own animation
block and its own second CLUT/image pair, and only `kyle_800102` contains code.
File id `800105` is a fifth id onto `kyle_800103`'s package with the same
textures and no SPK, which is why there are five ids and four packages.

They are therefore laid out the way `aya` is: one family per character, its
packages being that character's variants.

### 5.6 Area maps — two cooperating slots

| Files | Address | Contents |
|-------|---------|----------|
| `30100`–`30504` | `0x8012EF30` | Map *pictures* (Akropolis, “2F DRYFIELD”, Neo Ark, …) + a 0.5–4 KiB metadata package |
| `900000`–`900005` | `0x80179950` | Map UI + room-name tables (“East elevator hall”, “Saloon G & R”, “Golem freezer 1”, “Oval Office”, …). One package per area |

`900000`/`900001` share a body (Akropolis / MIST names). `900002`/`900003`
are Dryfield variants.

### 5.7 `50100`–`50152` @ `0x8012EF30` — Parasite Energy

**The Parasite Energy effects.** Same address as the map pictures, so they
**replace** that helper — you are not looking at the map while casting. Sound
bank + 0.8–12 KiB of code, no TMD, no texture: pure effect code.

The 40 file ids are only **15 packages**, headers `0x30`–`0x3E`. Each package is
loaded by three consecutive ids that differ only in the SPK they pair with, so
the code is shared by the three and the sound is per-id.

`Gp_ItemDescs` is what identifies them. It carries the twelve PE spells at items
15–50, each as three consecutive entries — the three spell levels — and those
twelve are, in order, exactly the twelve packages in the contiguous run:

| File ids | Package | Spell | | File ids | Package | Spell |
|---|---|---|---|---|---|---|
| `50101`–`50103` | `pyrokinesis` | Pyrokinesis | | `50119`–`50121` | `metabolism` | Metabolism |
| `50104`–`50106` | `combustion` | Combustion | | `50122`–`50124` | `healing` | Healing |
| `50107`–`50109` | `inferno` | Inferno | | `50125`–`50127` | `lifedrain` | Lifedrain |
| `50110`–`50112` | `necrosis` | Necrosis | | `50128`–`50130` | `antibody` | Antibody |
| `50113`–`50115` | `plasma` | Plasma | | `50131`–`50133` | `energyshot` | Energyshot |
| `50116`–`50118` | `apobiosis` | Apobiosis | | `50134`–`50136` | `energyball` | Energyball |

So the three ids per package are the three levels, one sound bank each. `50100`
is an extra id onto the same package and sound bank as `50101`.

The call profiles agree with the spell descriptions: the offensive spells drive
`Gp_SpawnEff` and `Task_Reparent`, while the support ones — `metabolism`,
`healing`, `antibody` — draw rings and arcs around Aya and little else, and are
the smallest packages in the family.

`50146` / `50149` / `50152` keep the stride-3 spacing at slots 15, 16 and 17,
which lines up with the items after the four passive “Attach this and see what
happens” ornaments. Only two of those, Flare and Pepper Spray, are “Use to …”
items, so there is one package more than the usable items account for. The names
`ofuda` / `flare` / `pepper_spray` are assigned by position and by what the code
draws — rings and fade quads with no motion, a spawned moving object, and a
scrolling-texture cloud (`Gp_AddTpageShift`) that suits a gas spray — and are
marked `ambiguous` in the manifest.

### 5.8 Menus sitting above the stack

| File | Address | Evidence |
|------|---------|----------|
| `20100` | `0x801D4000` | “Option”, “Key Configuration”, Vibration, Stereo/Mono, Walk, Draw Weapon, PE Menu, Type A/B/C, … |
| `20200` | `0x801D4000` | 4-byte stub in that slot |
| `20600` | `0x801D6000` | Tiny; role unknown |

---

### 5.9 `20600` @ `0x801D6000` — the NMC name table

Not a helper at all: 1471 bytes, no code, and a **63-entry pointer table at
`+0x484`** into Shift-JIS strings. Index 0 is the empty string; 1–62 are the
enemy roster, left in Japanese in the US build:

| Idx | Name | | Idx | Name |
|---|---|---|---|---|
| 1 | ネズミ (rat) | | 30 | ステルスヘッド |
| 2 | ガ (moth) | | 31–35 | レーザーほうだい１–４, マシンガン |
| 3 | ミトコンバス | | 36–39 | ノーマル/ヘビーゴーレム（ブレード/グレネード） |
| 4 | コウモリ (bat) | | 40–41 | ステルスゴーレム, ホワイト |
| 5 | サソリ (scorpion) | | 42 | メガネおんな |
| 9–13 | チキンマンＡ/Ｂ, ウォーカー, ライダー, ホッパー | | 43–44 | ギュンター１/２ |
| 14–15 | コーンヘッド, タートルヘッド | | 45–46 | スノーマフラー, アンデッド— |
| 16 | パラサイトベイビー | | 47 | ギガントサピエンス |
| 17 | ブラッドサッカー | | 48–49 | ガービジイーター１/２ |
| 18 | デザートランナー | | 52 | ジャイアントストーカー |
| 19–20 | ステルス/ドッペルパニッシャー | | 53–60 | ブラフマンＡ–Ｄ, クリアブラフマンＡ–Ｄ |
| 21–29 | ナイトシンガー … パラサイトだま | | 61–62 | サボテンダー, チョコボ |

`サボテンダー` (Cactuar) and `チョコボ` (Chocobo) at the end are the Square
cameos. Being an indexed roster, this is the natural key for naming the actor
overlays — the mapping from actor file id to index is not yet established.

### 5.10 Shared code between overlays

Overlays in a family are separate links that share a lot of code. Measured on
the split rooms: **56% of the 4146 functions are copies** of another room's, and
58% of the instructions. 23 bodies appear in 20 or more rooms; the most-copied
appears 265 times. `tools/overlay_dup_index.py` indexes this.

Equality is decided on splat's **disassembly text**, not on the instruction
words. Comparing words is tempting and wrong: clearing the link-dependent fields
so two copies at different addresses compare equal also clears every other
I-type immediate, so `lw $v0, 0x4($t0)` and `lw $v0, 0xC($t0)` — a different
struct field, a different function — come out identical.

The canonical form drops only what names a copy's own position: the
`/* offset vram encoding */` column, local branch labels (splat names them
`.L<overlay>_<vram>`), and the *identity* of an overlay-local symbol, keeping
that it is local. Two hashes come out of it:

| | meaning |
|---|---|
| **text** | the same body, wherever it links — **52% of functions, 56% of instructions** |
| **raw** | the same bytes. Every room loads at `0x8017D5C0`, so a body referencing nothing overlay-local relocates identically everywhere — 24% |

Byte-identical implies text-identical, and the tool asserts it: a `raw` class
whose members disagree on `text` means the canonical form is losing something.
That check is what caught the label regex missing `.L<overlay>_<vram>`.

**How a body is shared.** `src_path` is the family root, so a subsegment can be
named `lib/<unit>` instead of `<overlay>/<unit>`. Several overlays name the same
path, splat puts the same object in each of their linker scripts, and one object
relocates into all of them. The manifest records the span per overlay:

```toml
acropolis_cafeteria = { shared = [{ start = "0x395C", end = "0x431C", unit = "room_draw_billboard" }] }
```

Three things this needs, each learned by hitting it:

* **A shared symbol name.** Each sharer's symbol map names the address
  (`Room_DrawBillboard = 0x…`), otherwise every room calls it by its own
  `func_<room>_<vram>` and the link fails.
* **One rule per object.** The object is reached once per overlay that links it;
  `ninja_config.py` emits the build rule the first time and afterwards records
  only the dependency, because ninja rejects two rules for one output.
* **No absolute aliases.** splat emits an `alabel` for any known symbol inside
  the body; named after the room it was split from, it collides in the other 32.
  Keeping such an address out of the family imports file stops it being emitted.

**A body appearing twice in one overlay cannot be shared** — `ld` includes an
input object once, so the second slot goes unfilled. `room_draw_billboard`
appears 55 times across 44 rooms, and 11 of those contain it twice, so 33 rooms
share it and the other 11 keep their own copies.

The shared unit is a **`c` subsegment, not `hasm`**. A shared body is exactly as
unmatched as any other until someone decompiles it, and `hasm` would mark it
hand-written and drop it out of the unmatched count. `asm_path` is the family
root for the same reason `src_path` is: it puts the shared body's disassembly at
an overlay-independent path, so one `INCLUDE_ASM` can name it.

That last point is also the limit on sharing an *unmatched* body: every overlay
writes the same shared `.s`, which is only well-defined when the copies are
byte-identical. Bodies that are the same source at a different link offset can
share the C file once matched, because the compiler then regenerates them per
link address.

## 6. Models and animations

### 6.1 What is *not* a model overlay

- Room `file1+` — `.bs` 320×240 plates.
- Room `.pe2pkg` — event tables + script code.
- Room `.pe2cap2` — dialogue.
- `301xx` / `900xxx` — map art + map names.
- `501xx` — code-only Parasite Energy effects.
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

448 unique inflated bodies — unique by *content*: the extractor dedups by
SHA-1, so a package loaded under more than one file id is extracted once and
both ids resolve to the same file. The counts below are **instances** in
`stages.json`, so they run ahead of the number of distinct packages: the 32
weapon ids are 32 packages, but the 7 aya ids are 6, the 5 Kyle ids are
4, the 6 map-UI ids are 5, and the 24 map-picture ids are 17.

Packages that have been identified are **named** in the extractor
(`tools/peassets/asset_data.py`), so they extract as `m93r.pe2pkg`,
`map_akropolis.pe2pkg`, `nmc_names.pe2pkg` rather than `pe2pkg_2.pe2pkg`. That
name is the unit everything else keys on — the decomp manifest, the split
config, the symbol prefix — so a package loaded under several ids is one thing
with one name, and file ids stop appearing outside the extractor.

The **containers** are named too, so the extracted tree reads the same way. Each
STAGE1–5 folder is one room, so the folder takes its room's name
(`stage1/acropolis_square/` rather than `stage1/101/`), and a stage-0 file takes
the name of the overlay it holds (`stage0/m93r/`, `stage0/nmc_names/`).

`TREE` keys those containers by **name** and carries the disc id as an `id`
attribute:

```python
1: {"folders": {
    'acropolis_square': {"id": 101, "files": {
        'file0': {"id": 0, "chunks": {3: 'acropolis_square', …}},
    }},
}},
```

A name therefore cannot collide — dict keys are unique — and `reverse_folder_id`
/ `reverse_file_id` are a plain lookup rather than a scan. The key doubles as
the on-disk directory name and the `stages.json` key, falling back to `<id>` /
`file<id>` for anything still unnamed.

Nine stage-0 packages are reachable through more than one file id (`10500` and
`10600` both load `aya_10500`). Only one file can carry the package's name —
names must be unique within a container — so those extra ids keep their default
`file<id>` key rather than a name that would claim to be the primary.

| Load | Count | Family |
|------|------:|--------|
| `0x80093800` | 2 | gameplay, title |
| `0x80115770` | 7 | Aya + replay-bonus + stubs |
| `0x8011D1C0` | 32 | weapons |
| `0x8012EF30` | 64 | map pictures + PE effects |
| `0x80131E20` | 102 | actor slot 1 (+ `4xxxxx`) |
| `0x80149E20` | 40 | actor slot 2 |
| `0x80161E20` | 54 | actor slot 3 (+ some `8xxxxx`) |
| `0x80167A70` | 5 | Kyle |
| `0x80179950` | 6 | map UI |
| `0x8017D5C0` | 168 | rooms |
| `0x801D4000` | 2 | options |
| `0x801D6000` | 1 | tiny helper |

---

## 9. Open questions

- What `20900` and the 4-byte actor stubs are *for* at runtime
  (id presence vs real payload).
- Full `TmdSource` / bone-matrix stride for Aya (first `MATRIX` is identity;
  later bones are not a plain `0x20` `MATRIX` array).
- Whether room TMD specks are world props, inventory pickups, or noise.
- Which effect the three `501xx` tail packages are (§5.7): the stride puts them
  at slots 15-17, but only two of the items there are usable, so one of
  `ofuda` / `flare` / `pepper_spray` is misassigned or is not an item at all.
- Friendly names for actor suffixes (`300`, `400`, `700`, …) — not yet
  matched to in-game character ids, though `20600` (§5.9) now supplies the
  roster to match them *against*.
- Who Kyle's packages are. There are five file ids `800101`–`800105` but only
  **four packages**: `800105` loads `kyle_800103`'s package with the same
  per-character CLUT and image, differing only in that it has no SPK, so it is
  that character without a sound bank.

  The four packages are near-duplicates rather than copies. All four carry the
  *byte-identical* 300-vertex body and the same four small meshes (two 27- and
  two 23-vertex), and the same 20-bone skeleton. Each adds exactly one mesh of
  its own — 22, 52, 36 and 78 vertices — and its own animation block, and only
  `kyle_800102` contains code. At the byte level `800101`/`800103`/`800104`
  are ~42% identical at the same offsets, while `800102` shares nothing at a
  fixed offset because its code shifts everything after it. The distinguishing
  meshes are small single-part objects that read as held props rather than
  heads; `tools/peassets/tmd_export.py kyle` writes them out as OBJ.
