# Rematching a follow-up

A rematching follow-up names a function that matches, but through C a programmer
would not write: a steering `do`/`while(0)`, a shared or redundant temporary, an
alias local, a `goto` join, a macro standing in for a helper or a loop, a duplicated
store, an odd declaration, a register pin or an empty `asm`. The job is to replace
that construct with plain C that still matches exactly.

## Read the entry

```
venv/bin/python3 tools/refactor/followups.py show <id>
```

`reason` says what was tried, `next_step` what to try next. Treat both as hints:
the saved trial files they cite under `local/` are mostly gone, and a stated
conclusion can be wrong (one said two arms could not merge; they could, once the
stores followed the struct's field order).

## Look for a sibling first

Most successful rematches copied a shape that already matches elsewhere:

- the same function in a sibling overlay (another room of the same family, the
  day or night version of a room, another actor slot):
  `python3 tools/bulk/overlay_dup_index.py similar <function>` ranks matched bodies
  that resemble it;
- a Psy-Q SDK macro (`setVector`, `setXY0` and the like) in place of member-by-member
  assignments.

## Read the target before building

Some entries can be settled from the target assembly alone, as "original" rather
than a hack:

- a stack frame exactly the size of a local nothing uses: an unused local in the
  original;
- a counter in an `s` register across loops that make no calls: one variable
  shared by those loops;
- a `$v0` setup in a delay slot at the end: the function returned `void`;
- a compare chain split at values only `case` labels produce: those labels exist.

Record such an entry as `wontfix` with the evidence, instead of attempting it.

## Try, and compare

```
./tools/build-and-verify.sh --only <image>          # builds and checksums one image
python3 tools/asm-differ/diff.py --no-pager <function>
```

The differ finds the function in the built images itself. For a function several
images define (shared code), give the address instead, and build every image that
compiles it. Finish with the bare `./tools/build-and-verify.sh`.

Budget about 8 builds per entry. Stop earlier once the evidence shows the current
form is required: a loop that is the only way to hoist an instruction into a delay
slot, a register shared across blocks, a scheduling barrier.

## Accept only plain C

- the images that compile the function match;
- the construct the entry names is gone or clearly reduced;
- nothing new steers the compiler: no `asm`, register pins, barrier macros, empty
  blocks or `do`/`while(0)`, codegen-only casts or `volatile`, dummy temporaries or
  dead stores;
- existing names kept; a new local or helper gets a meaningful name.

Then commit it, and mark the entry resolved with that commit:

```
venv/bin/python3 tools/refactor/followups.py mark <id> --status resolved --reason "..." --commit <sha>
```

## What tends to work

From the first pilot (11 entries, 4 rematched, 3 shown original):

- likely: `do`/`while(0)` wrappers around loops, `goto` joins between duplicated
  tails, a flag variable turned into a predicate helper, stores reordered to the
  struct's field order;
- unlikely: forms that exist for loop hoisting, a register shared across blocks,
  or a scheduling barrier; the remaining pins and empty `asm` statements are of
  this kind.
