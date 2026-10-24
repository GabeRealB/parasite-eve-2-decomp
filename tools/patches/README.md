# Patches to the tool submodules

Fixes this project needs in two of its submodules and that upstream does not
carry. A submodule's working tree is reset by `git submodule update` and by a
fresh clone, so each fix is kept here as a patch and re-applied by the tool
that needs it. Both submodules are marked `ignore = dirty` in `.gitmodules`
for that reason: a patched working tree is the expected state.

| Patch | Submodule | Applied by | The matching build needs it |
|---|---|---|---|
| `maspsx-lo-load-nop.patch` | `tools/maspsx` | `ninja_config.py` | yes |
| `maspsx-bss-layout.patch` | `tools/maspsx` | `ninja_config.py` | yes |
| `maspsx-bss-local.patch` | `tools/maspsx` | `ninja_config.py` | yes |
| `decomp-permuter-objdump.patch` | `tools/decomp-permuter` | `tools/match/prepare_permuter.py` | no |
| `decomp-permuter-sizeof.patch` | `tools/decomp-permuter` | `tools/match/prepare_permuter.py` | no |

The maspsx patches apply to upstream `4ffa8b5` (2026-10-24), the commit the
submodule points at, as written against `670417c` (2026-10-17); they apply in
the order listed. With them, one of upstream's own tests fails,
`test_lcomm_alignment_none`: it expects every `.lcomm` symbol global, which is
what `maspsx-bss-local.patch` undoes.

## maspsx

maspsx stands in for ASPSX, the Psy-Q assembler: it rewrites the compiler's
output so that GNU `as` produces what ASPSX 2.77 would have. Each patch below
closes a difference between the two. A patch belongs here only when the
behaviour it gives maspsx has been observed from the real tools; a change that
merely makes the images match is a hack and does not.

`ninja_config.py` applies them every time it runs (`_ensure_maspsx_patch`,
`_ensure_maspsx_bss_layout_patch`, `_ensure_maspsx_bss_local_patch`). Each
function looks for a line of its patch in `maspsx/__init__.py` and applies the
patch only when that line is missing. maspsx's two source files are inputs of
every assembly step, so a changed patch reassembles everything.

"Without it" below is what happens when the build is run with that one patch
reversed and the other two in place, measured on 2026-10-14. The functions
named are those whose bytes in the linked image differ from the matching
build's, compared function by function.

"Verified" says whether the behaviour was observed from the real assembler
and, where the linker decides, the real linker. See
[Checking against the real tools](#checking-against-the-real-tools) for how.

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
| `akropolis_square` | `akropolisSquareTelephoneLightTask` | 924 becomes 922 |

Those five are the cause. Everything after a shortened function sits one or
two instructions early, so the functions that call or point at the moved code
differ too: 17 functions in `acropolis_fire_escape`, 14 in `acropolis_plaza`,
15 in `acropolis_roof_garden` and 26 in `akropolis_square`.

**Verified.** Yes. ASPSX 2.77 assembles

```
lw  $3,0($5)
lw  $2,%lo(sym)($3)
```

to `8CA30000 00000000 8C620000`: it inserts the nop and emits the second load
as one word. The same load written `lw $2,sym($3)` comes out as
`3C010000 00230821 8C220000`, the three-instruction form through `$at`.

### `maspsx-bss-layout.patch`

**What it does.** Aligns an object's `.comm` variables as PSYLINK places
them: to 2 from two bytes, to 4 from four and to 8 from eight.

**Why it is needed.** The compiler hands the assembler `.lcomm` and `.comm`
with a size and no alignment, so where each variable lands is decided
downstream, by two different programs. ASPSX places the `.lcomm` ones itself,
at the start of the object's `.bss`, and upstream maspsx (since `670417c`) lays them
out the same way, by ASPSX version. A `.comm` one ASPSX only records, by name
and size, and the linker places it: PSYLINK puts an object's `.comm` variables
after that object's own `.bss`, with the alignment above. Upstream aligns them
like `.lcomm` ones, up to 16. (Capping that at 8, upstream's
`--max-comm-alignment`, would match this game too, but differs from PSYLINK
for 3- and 5- to 7-byte variables.)

Upstream emits the `.lcomm` and `.comm` variables in the order given, where
ASPSX puts the `.lcomm` ones first. The sources follow ASPSX instead: every
`static` BSS variable is declared before the file's non-`static` ones.

Two things cannot be reproduced. PSYLINK orders an object's `.comm` variables
by a hash of their names (below), not in the order the object records them.
The original names are not known, so the sources state that order by declaring
those variables in address order.

**Without it.** Fifteen images fail the checksum (measured against `670417c`
with the `.lcomm`-first declarations in place): main, gameplay,
`acropolis_fire_escape`, `dryfield_gas_station`, `dryfield_motel_room_6`,
`shelter_r47`, `neo_ark_eve_access_tunnel`, `shelter_b6_nursery`,
`shelter_b6_training_room`, and `actor_121300`, `143000`, `160900`, `401000`,
`401800`, `450200`. A `.comm` variable of nine bytes or more lands on 16 instead
of 8, and everything after it moves.

The patch used to lay the whole `.bss` out itself, `.lcomm` padding and order
included; against upstream `e85ecb3`, which aligned `.bss` entries not at all,
nine images failed without it.

**Verified.** Yes, each part against the program that decides it.

- *`.bss` alignment.* The object ASPSX 2.77 writes declares `.bss` with an
  alignment of 8, and PSYLINK starts each object's `.bss` on a multiple of 8.
- *`.lcomm`, by ASPSX 2.77.* An N-byte `.lcomm` that follows a one-byte one is
  placed at offset 1 for N = 1, 2 for N = 2, 4 for N = 3 and 4, 8 for N = 5 to
  8, and 16 for every N from 9 to 100 that was tried.
- *`.comm`, by PSYLINK 4.4.* An N-byte `.comm` that follows one byte of the
  object's own `.bss` is placed 1 byte in for N = 1, 2 for N = 2 and 3, 4 for
  N = 4 to 7, and 8 for every N from 8 to 100 that was tried: never 16.
- *Where the `.comm` ones go.* With two objects linked, PSYLINK 4.4 and 4.5
  put each object's `.comm` variables after that object's `.lcomm` block and
  before the next object's. A `.comm` variable both objects name goes with
  the first.
- *In what order.* Linking Sony's own objects from the Psy-Q 4.5 archives
  (`CDROM.obj` from `libcd`, `WAITRC2.obj` from `libpad`, `FREE3.obj` from
  `libapi`), PSYLINK 4.4, 4.5 and 4.6 all place their `.comm` variables in
  the same order, which is not the order the objects record them in, and is
  exactly the order the game has. PSYLINK 4.5's code gives the rule:
  - its symbol table hashes a name as its length plus the sum of its bytes,
    modulo 512 (`0x40664a`; with `/c`, as here, the bytes are used as they
    are, otherwise each first goes through a translation table);
  - a name new to the link is added at the front of its bucket's chain
    (`0x4066b4`);
  - the `.comm` variables are taken by walking the buckets from 0 to 511 and
    each chain front to back (`0x406f0b`), and each is aligned to 1, 2, 4 or
    8 by its size (`0x406f55`).

  So within an object they come out in ascending hash order, and of two in
  the same bucket the one the link met later comes first. Every order
  observed fits: `cdrom` runs from `Stframe_no` (24) to `StRingSize` (508),
  with `StRingBase` before `StRingAddr` at 476; `waitrc2` is `_startTime`
  (38), `_waitTime` (428); `free3` is `_r_bp` (7), `_r_freep` (330), `_r_p`
  (420). In the small test objects above, record order happened to be hash
  order too. PSYLINK 4.1 walks the same table, but across all objects at
  once.

PSYLINK 4.1 gathers the `.comm` variables of all objects after all `.bss`.
The images match the 4.4 behaviour, which is what the patch follows.

**What this rules out in the sources.** A `static` variable that sits after a
non-`static` one of the same object is a layout the real tools cannot produce,
and so is a `static` of nine bytes or more at an offset that is not a multiple
of 16. Neither matches: the second because of the `.lcomm` layout, the first
because the sources declare `static`s first and maspsx keeps that order. 98
variables in 41 objects were once declared `static` in ways the tools could
not have laid out, and had only matched because an earlier version of this
patch used the linker's rule for every variable; they are not `static` now.

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

**Verified.** Yes. In the object ASPSX 2.77 writes, each `.lcomm` symbol is a
local-symbol record and each `.comm` symbol a global one.

### Checking against the real tools

`ASPSX.EXE` version 2.77 is the one in Psy-Q 4.3; `PSYLINK.EXE` was taken from
Psy-Q 4.4 and 4.5, which behave alike here. Both are 32-bit Windows console
programs and neither is in this repository. They run directly from WSL through
Windows interop, from a directory on a Windows drive; `wine` in 64-bit-only
mode does not run them.

```
ASPSX.EXE -G0 -o A.OBJ A.S
PSYLINK.EXE /m /c @LINK.TXT,OUT.CPE,OUT.SYM,OUT.MAP
```

- The assembler source needs CRLF line endings; with LF it stops at the first
  line.
- `-G0` is what this project builds with. Without it small objects go to
  `.sbss`, where the layout is the same.
- The assembler's output is a Psy-Q `LNK` object, not ELF. Its records give
  each section's alignment, the bytes of `.text`, and each symbol as local,
  global, or global and left to the linker (`.comm`).
- The linker's map file (`/m`) lists every global symbol with its address,
  which is where a `.comm` variable's placement is read from. `static`
  variables are not in it; their offsets are in the object.

The link script used:

```
	org	$80010000
text	group
bss	group	bss
	section	.rdata,text
	section	.text,text
	section	.data,text
	section	.sdata,text
	section	.sbss,bss
	section	.bss,bss
	include	"A.OBJ"
	include	"B.OBJ"
```

The assembler source used for the instruction and symbol-binding results:

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

For the alignment results each size got an object of its own: `.lcomm pad,1`
followed by `.lcomm probe,N` for the assembler's rule, and `.lcomm pad,1`
followed by `.comm probe,N` for the linker's, with all the objects of the
second kind linked together.

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
