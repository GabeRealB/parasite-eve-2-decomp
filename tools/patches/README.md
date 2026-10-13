# Patches to the tool submodules

Fixes this project needs in two of its submodules and that upstream does not
carry. A submodule's working tree is reset by `git submodule update` and by a
fresh clone, so each fix is kept here as a patch and re-applied by the tool
that needs it. Both submodules are marked `ignore = dirty` in `.gitmodules`
for that reason: a patched working tree is the expected state.

| Patch | Submodule | Applied by | The matching build needs it |
|---|---|---|---|
| `maspsx-lo-load-nop.patch` | `tools/maspsx` | `ninja_config.py` | yes |
| `maspsx-bss-align.patch` | `tools/maspsx` | `ninja_config.py` | yes |
| `maspsx-bss-local.patch` | `tools/maspsx` | `ninja_config.py` | yes |
| `decomp-permuter-objdump.patch` | `tools/decomp-permuter` | `tools/match/prepare_permuter.py` | no |
| `decomp-permuter-sizeof.patch` | `tools/decomp-permuter` | `tools/match/prepare_permuter.py` | no |

The maspsx patches are written against upstream `e85ecb3` (2026-10-02), the
commit the submodule points at. They apply in the order listed.

## maspsx

maspsx stands in for ASPSX, the Psy-Q assembler: it rewrites the compiler's
output so that GNU `as` produces what ASPSX 2.77 would have. Each patch below
closes a difference between the two.

`ninja_config.py` applies them every time it runs (`_ensure_maspsx_patch`,
`_ensure_maspsx_bss_align_patch`, `_ensure_maspsx_bss_local_patch`). Each
function looks for a line of its patch in `maspsx/__init__.py` and applies the
patch only when that line is missing. maspsx's two source files are inputs of
every assembly step, so a changed patch reassembles everything.

"Without it" below is what happens when the build is run with that one patch
reversed and the other two in place, measured on 2026-10-14. The functions
named are those whose bytes in the linked image differ from the matching
build's, compared function by function.

"Verified with ASPSX" says whether the behaviour was observed from the real
assembler. See [Checking against ASPSX](#checking-against-aspsx) for how.

### `maspsx-lo-load-nop.patch`

**What it does.** Treats a load written with an explicit `%lo()`,
`lw $2,%lo(sym)($3)`, as the single instruction it is, so the load-delay nop
before it is kept when the base register was loaded by the previous
instruction.

**Why it is needed.** Upstream maspsx assumes such a load expands through
`$at`, as `lw $2,sym($3)` does, and so drops the nop. It already makes this
distinction for stores; the patch makes it for loads.

**Without it.** Four room overlays fail the checksum; the other 445 images
match. Five functions lose instructions, each a light-flare drawer of an
Acropolis room:

| Image | Function | Instructions |
|---|---|---|
| `acropolis_fire_escape` | `acropolisFireEscapeFlareTask` | 950 becomes 948 |
| `acropolis_fire_escape` | `acropolisFireEscapeFlickerLightTask` | 627 becomes 625 |
| `acropolis_plaza` | `acropolisPlazaLightFlareTask` | 929 becomes 928 |
| `acropolis_roof_garden` | `acropolisRoofGardenFlareTask` | 924 becomes 922 |
| `acropolis_square` | `acropolisSquareBeaconGlowTask` | 924 becomes 922 |

Those five are the cause. Everything after a shortened function sits one or
two instructions early, so the functions that call or point at the moved code
differ too: 17 functions in `acropolis_fire_escape`, 14 in `acropolis_plaza`,
15 in `acropolis_roof_garden` and 26 in `acropolis_square`.

**Verified with ASPSX.** Yes. ASPSX 2.77 assembles

```
lw  $3,0($5)
lw  $2,%lo(sym)($3)
```

to `8CA30000 00000000 8C620000`: it inserts the nop and emits the second load
as one word. The same load written `lw $2,sym($3)` comes out as
`3C010000 00230821 8C220000`, the three-instruction form through `$at`.

### `maspsx-bss-align.patch`

**What it does.** Starts each object's `.bss` on a multiple of 8, and aligns
every `.bss` entry by its size: to 2 for two bytes or more, to 4 for four or
more, to 8 for eight or more. Upstream applies the size rule to `.sbss` only.

**Why it is needed.** The compiler hands the assembler `.comm` and `.lcomm`
with a size and no alignment, so where each object lands is the assembler's
decision. Without the rule a word that follows a halfword sits two bytes
early, and everything after it in the image moves.

**Without it.** Nine images fail the checksum; the other 440 match. No function
changes length. What moves is data: a `.bss` object lands a few bytes early,
everything after it in the image follows, and each function that addresses a
moved object then differs in the address it loads.

| Image | First object to move | Functions that differ |
|---|---|---|
| `SLUS_010.42` (main) | `gCdCmdQueue`, 4 bytes early; 265 objects move | 426 |
| `gameplay` | `gAttachmentState`, 2 bytes early; 196 objects move | 354 |
| `actor_110600` | `ActorContact_ScratchPosition`, 6 bytes early | `_actor110600ScriptedState`, `_actor110600SpawnHitEffect`, `_actorContactApplyGridPushback` |
| `actor_121300` | `_gScreenWaveColumnWaves`, 2 bytes early | `_screenWaveTask` |
| `actor_160900` | `_gScreenWaveRowWaves`, 2 bytes early | `_screenWaveTask` |
| `actor_215100` | `_gActor215100GalleryPendingExitRequest`, 4 bytes early | `actor215100GalleryExitDecisionTask`, `actor215100ResolveGalleryExit` |
| `dryfield_night_motel_lobby` | `gDryfieldNightMotelLobbyRevisitCutscene`, 1 byte early | `_dryfieldNightMotelLobbyCommandMsg` |
| `neo_ark_submarine_tunnel` | `gScreenWaveRowWaves`, 2 bytes early | `_screenWaveRun`, `_screenWaveTask` |
| `shelter_1f_parking_garage` | `gRoomEventLatched`, 4 bytes early | `_shelter1fParkingGarageResolveRoomEvent`, `roomEventStagedTask` |

**Verified with ASPSX.** For `.lcomm`, yes. Six `.lcomm` symbols of sizes
2, 4, 1, 8, 1 and 2, in that order, are placed by ASPSX 2.77 at offsets
0x0, 0x4, 0x8, 0x10, 0x18 and 0x1A, and its object declares `.bss` with an
alignment of 8. For `.comm`, no, and it cannot be: ASPSX does not place a
`.comm` symbol at all. It records the name and size and leaves the address to
the linker. That the same size rule holds for those rests on the Psy-Q library
objects and on this project's images matching.

### `maspsx-bss-local.patch`

**What it does.** Marks a `.bss` symbol global only when it came from `.comm`.
A symbol from `.lcomm`, a `static` variable in C, stays local.

**Why it is needed.** Upstream marks every `.bss` symbol global. Two source
files of one overlay that each have a `static` of the same name then define
the same global twice.

**Without it.** One overlay does not link, and the checksum step never runs:

```
actor_210600_contacts.c.o: multiple definition of `ActorContact_ScratchPosition';
actor_210600.c.o: first defined here
```

No function fails to match: in the other 448 images every function is
byte-identical to the matching build's. The failure is the link of
`actor_210600`, whose two source files each have a `static
ActorContact_ScratchPosition`.

**Verified with ASPSX.** Yes. In the object ASPSX 2.77 writes, each `.lcomm`
symbol is a local-symbol record and each `.comm` symbol a global one.

### Checking against ASPSX

`ASPSX.EXE` version 2.77 is the one in Psy-Q 4.3. It is a 32-bit Windows
console program; it is not in this repository. It runs directly from WSL
through Windows interop, from a directory on a Windows drive; `wine` in
64-bit-only mode does not run it.

```
ASPSX.EXE -G0 -o T.OBJ T.S
```

- The source needs CRLF line endings; with LF it stops at the first line.
- `-G0` is what this project builds with. Without it small objects go to
  `.sbss`, where the layout is the same.
- The output is a Psy-Q `LNK` object, not ELF. Its records give each section's
  alignment, the bytes of `.text`, and each symbol as local, global, or global
  and left to the linker.

The source used for the three results above:

```
	.comm	c1,2
	.comm	c2,4
	.lcomm	l1,2
	.lcomm	l2,4
	.lcomm	l3,1
	.lcomm	l4,8
	.lcomm	l5,1
	.lcomm	l6,2
	.text
	.globl	t1
t1:
	lw	$3,0($5)
	lw	$2,%lo(sym)($3)
	jr	$31
	lw	$2,sym($3)
```

## decomp-permuter

The permuter searches for source variations that compile closer to a target
function. It is a matching aid, not part of the build: the build passes
without these two patches, and nothing here is about ASPSX.

`tools/match/prepare_permuter.py` applies both, and `permute.sh` runs it
before every search. It applies a patch when `git apply --check` accepts it,
takes a patch that reverses cleanly as already applied, and stops otherwise.

### `decomp-permuter-objdump.patch`

**What it does.** Reads an immediate that objdump printed as bare hex, without
the `0x` prefix.

**Why it is needed.** A `j` or `jal` to a label inside the same function is
printed as `j 2d8 <fn+0x2d8>`. The permuter's scorer parses that operand as a
number with an automatic base, which rejects `2d8`, and the search crashes.
GCC emits such jumps for long `switch` bodies.

**Without it.** Scoring raises `ValueError` on any function with a jump of
that kind, so that function cannot be searched.

### `decomp-permuter-sizeof.patch`

**What it does.** Gives `sizeof(type)` and `_Alignof(type)` their own
precedence in the permuter's C grammar, so they are reduced before a following
`+`, `-`, `*` or `&`.

**Why it is needed.** The permuter's parser treats a cast as a unary
expression. Without the precedence, `sizeof(T) * n` is read as `sizeof` applied
to a cast of `*n`, and the source the permuter writes back differs from the
one it was given.

**Without it.** A search over a function that uses `sizeof(type)` in such an
expression starts from changed source, so its scores are not about the real
function. After a grammar change `prepare_permuter.py` also regenerates the
parser tables, which the permuter otherwise reuses stale.
