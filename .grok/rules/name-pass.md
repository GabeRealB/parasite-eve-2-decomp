# Naming pass: what the job is

You are analysing one item of a decompiled PlayStation game and making its
declaration say what it really is. `NAMING.md` at the repository root holds the
conventions; read it.

**This is not a renaming task.** A step that only changes a name has not done
the work. The job is to work out what the item is from the code that uses it,
and then make the code say so — the name, the type, the shape of the
declaration, and the documentation together.

## Changing code is expected

Over the items done so far, "naming" an item has meant widening a field's type
because every user was casting, replacing a phantom overlay type with a nested
member, dropping casts that a corrected type made redundant, moving a private
declaration out of a header, and rewriting the prose that described any of it.
Edit callers, headers and sources as the finding requires.

**You may break the match while you work.** The checksum is the acceptance test,
not a fence. If restructuring a type changes code generation, that is a normal
intermediate state, and restoring it is part of your job rather than a reason to
stop. Adjust the declaration, the casts, or the shape of the expression until the
target matches again. Finish with the match intact *and* the change made — never
with the change abandoned because it first broke the build.

If after real effort a restructuring cannot be made to match, say so, say what
you tried, and leave that part alone. That is a result. Silently downgrading the
task to a rename is not.

## Trust nothing that is already written

The comments, the current name and the declared type are each an earlier
reader's hypothesis, and the project's standing rule is that they are wrong until
the code says otherwise.

- **The name** is the most dangerous, because it supplies the vocabulary for your
  description without ever reading as a claim. A field called `idMap` invites you
  to describe an id map even when every write stores something else. Read the
  uses, decide what the thing is, and only then ask whether the old name survives.
- **The declared type** is next. If the users have to cast, the type is wrong. The
  reference listing marks each use that sits in a cast; count them.
- **The comments** are last, and are rewritten from your own reading, never
  edited in place.

## Finish what you find

Noticing something is not the deliverable; the change it implies is. Every step
so far has been sent back for stopping one move short, so treat each of these as
part of the acceptance test rather than as advice.

- **Leave no field both described and unnamed.** A comment stating what
  `field_14` does contradicts the name beside it, because the offset spelling is
  what says the role is unknown. Name it from that same reading, or drop the
  claim and say the role is unproven. Finishing with most of a struct still
  spelled `field_XX` is an unfinished step, not a conservative one.
- **Leave none of the old spellings behind, the struct tag included.** A tag is
  carried only where C forces it - the type is self-referential, or something
  refers to it as `struct Tag` - and is otherwise dropped, leaving
  `typedef struct { ... } Name;`. Where it is required it is spelled exactly
  like the type, with no leading underscore, since that marker means private and
  a public type wearing it reads as a contradiction. Check before deciding:

      grep -rn 'struct _Name\b' src include | grep -v typedef

  This is not cosmetic. The driver treats an item as still outstanding while its
  old name appears anywhere under `src` or `include`, so one surviving tag makes
  the pass re-pick the same item forever and the sweep never advances. Your step
  is finished only when the old spelling is gone from the tree.
- **Follow an alias to its source.** When a field turns out to cache, mirror or
  index another object's field, that other field is the thing to go and read;
  the name and the comment come from what the value *is*, not from where it was
  copied. "A cached copy of another struct's `field_22`" is a lead you stopped
  on.
- **Re-grep the notes for the access path you changed.** `rename_item.py`
  propagates a *rename* everywhere, markdown included, but restructuring is not
  a rename: nesting a field, folding a type into a union, or merging a duplicate
  leaves the old *path* spelled out in prose that no rename ever touches. Those
  mentions are not cosmetic - the next agent greps for a technique, finds
  `owner->oldPath`, and rebuilds the shape you just removed. After the
  declaration settles, search the notes for every spelling it retired:

      grep -n 'gOwner->oldField\|OldType\|Owner\.oldField' DECOMPILATION_LEARNINGS.md

  Update them to the current spelling rather than annotating them, since the
  finding each entry records is still true; leave its measurements and narrative
  alone. A single moved field can strand a hundred mentions, so this is checked
  every time, not only when a type disappears.

- **Enumerate every overlay and duplicate before merging any of them.** There is
  rarely only one, and the first one found is often not the most used. Before
  deciding, list them:

      grep -rhoE '\(\s*[A-Za-z_][A-Za-z0-9_]*\s*\*\s*\)\s*&?\s*gOwner(->member)?' src include | sort | uniq -c | sort -rn

  Where the uses agree that they mean the same thing, they *are* one type: the
  member takes that single type, the duplicates go, and the surviving name is
  chosen on the evidence - usually the one with the most uses, not the one you
  happened to open first. A shared layout alone is never the reason; two types
  that differ in meaning stay apart. `NAMING.md` defers a merge only for a pair
  with no relationship to the item you are processing, so it is not a licence to
  defer the duplicates of your own member's type.
- **An overlay that will not reconcile is pointing at a union.** If folding a
  view into its owner changes code generation and cannot be made to match, the
  run has two simultaneous readings - typically an aggregate that is also copied
  wholesale, where taking the member's address rebases the copy and moves the
  offsets. Write the union of both views as named members
  (`session->at4.loc.stage` beside `session->at4.raw`) rather than restoring the
  cast. Reverting and reporting it impossible stops one step short of what the
  failure was telling you.

## Names

lowerCamelCase, one identifier, no separators, opening with the module or
package that owns the symbol. A leading marker carries the rest:

| | |
|---|---|
| no marker | a function — `fsLoadFile`, `gunbladeFireRound` |
| leading `g` | a global — `gFsFileTable` |
| leading `_` | private to its translation unit — `_fsReadSector`, `_gSectorCache` |
| PascalCase | a type — `FsCdfFile`; a private one is `_SectorCache` |

A private symbol is the public name with `_` prepended. A struct tag has the
same spelling as its type. The module or package part is derived, never
invented: for core code the module prefix lowercased, for an overlay the
manifest key camelCased (`mine_mesa` → `mineMesa…`), never abbreviated.

**A field you can describe is a field you can name.** Writing a comment that
states a field's role and leaving it called `field_14` contradicts itself — the
offset name is what says the role is unknown. Name it from the same reading.
Keep `field_XX` only where the role really is unproven, and say so.

**Name the parameters too**, in the prototype and the definition alike. A
signature reading `(s32 arg0, s32 arg1)` tells a caller nothing, and naming them
in one place only leaves one function with two signatures.
`rename_item.py <file>/<function>::<param> <newName>` matches by position and
rewrites both. Member renames take the same form,
`<header>/<Type>::<field> <newName>`.

Generated placeholders — `func_<package>_<VRAM>`, `D_<VRAM>` — keep their form
only while the body is still assembly, which the step's `state` tells you.
`generated` means exactly that: nothing to read, so the placeholder stays.
**`unnamed` is the opposite**: the function has been decompiled and simply never
named, so its body, its callers and its parameters are all in front of you and
the placeholder is what you are there to replace. Most placeholders in this tree
are `unnamed`, so treating the two alike leaves the bulk of the work undone.

## Every rename goes through the tool

Make every rename with `rename_item.py` - symbols, types, fields and parameters
alike - and never with a hand edit, a `sed`, or a script of your own:

    venv/bin/python3 tools/refactor/rename_item.py <file>/<oldName> <newName> --sidecars

Two reasons, and the second is the one that bites. It resolves references
through the C parser, so it cannot miss a use or rewrite an unrelated one - a
field name that several unrelated types also declare, or a mention that only
exists after macro expansion, are both cases a textual pass gets wrong. And it
appends the old and new spelling to `local/renames.tsv`.

**That file is now a source of truth.** The pass decides what has already been
handled by reading it together with the driver's step ledger; nothing is
inferred from what a name looks like any more. So a rename made by any other
means leaves no row, the item reads as untouched and is queued again, and the
change cannot be propagated or audited. Renaming by hand does not just skip some
bookkeeping - it puts the worklist out of step with the tree.

If the rename is one the tool cannot express, do it by hand and say so plainly
in your report, naming the old and new spelling, so the row can be added.

## Documentation

`///` immediately above the declaration, opening with one summary sentence, with
anything further after a blank `///` line. Cross-references in backticks.
`include/main/mem.h` is the worked example for functions and types.

**Struct fields take aligned trailing `//` comments**, not a `///` block each, so
the declaration stays readable as a table. Where a field takes a small set of
values, enumerate them — `(0 none, 1 TMD model, 2 2D display)` — because nothing
else says what a 2 means. **Never annotate a field with its offset**: the layout
is already in the types and fixed by `STATIC_ASSERT_SIZEOF`, and the annotation
sits in exactly the column the comment needs. `include/main/task.h` is the
worked example.

Say **what** something is and **why** it exists. Not how you worked it out —
that belongs in the commit message, not in a header that outlives it. Keep it as
general as the subject allows and only as specific as it needs: naming the
mechanism is the usual mistake, and it dates the comment. A field comment
naming a generated symbol - `func_800AD5B8`, `D_8010EB94` - is that mistake in
its sharpest form: it explains the field by pointing at something the reader has
to decode as well. "Frames left before
the body is released" is the field; "counted down by `Task_CountdownCallback`"
adds a function name the reader can find and that will not survive a
reorganisation. The test: would the sentence still be true and useful if a
neighbouring function were renamed?

A symbol whose role is unproven is left undocumented, or its comment says only
what was observed and that the role is unproven. An unmarked comment reads as
established fact, so never promote a guess to a description.

Documentation follows visibility: a public symbol is documented once at its
declaration in the module header, a private one at its definition in the `.c`,
never both. Mentions of an old name elsewhere are rewritten by
`rename_item.py`, including in markdown — re-read them afterwards, since a
renamed mention can leave a sentence describing the old idea.

## What this compiler allows

GCC 2.8.1, `-O2`, no `-finline-functions`.

- **Anonymous struct and union members do not work.** The declaration is
  accepted and every access to it is then rejected. Nest with *named* members:
  `owner->at4.loc`, not `owner->loc`. `include/main/gpuext.h` looks like a
  counter-example; the line that would read through its anonymous struct is
  commented out in `src/main/gpuext.c`.
- `static` is safe more often than it looks, since nothing is inlined unless
  explicitly marked.
- `.text` and `.rodata` are emitted in source order, so functions keep their
  sequence and a position-sensitive global cannot be hoisted.

## Finishing

`./tools/build-and-verify.sh` must end with `BUILD SUCCEEDED` and the
matched-function count must not drop. Do not commit; the driver commits.

Report what the item turned out to be, what you changed beyond the name, and
anything you could not make match.
