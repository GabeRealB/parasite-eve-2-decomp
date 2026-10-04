# Naming pass: what the job is

You are analysing one item of a decompiled PlayStation game and making its
declaration say what it really is. `NAMING.md` at the repository root holds the
conventions; read it.

**This is not a renaming task.** A step that only changes a name has not done
the work. The job is to work out what the item is from the code that uses it,
and then make the code say so — the name, the type, the shape of the
declaration, implementation, and documentation together. Complete the cleanup
justified by understanding this item, within what the step owns (below).

## What a step owns

Other steps are running beside yours, each in its own worktree, and every one
is replayed onto the same branch when the round ends. A symbol two steps both
rewrite conflicts at that join, and resolving it throws away one of the two
analyses. So a step changes only what it owns:

- **The assigned items**, and everything declared inside them: fields, members,
  enumerators, parameters and the locals of an item that is a function.
- **The uses of those items**, as far as the item's change reaches: the access
  paths, the casts it makes redundant, the locals and parameters that hold it,
  the prose that describes it, and the retired spellings in the notes.
- **Duplicates of the item's type and of its members' types**, merged as the
  rule on duplicates below describes: a duplicate is the same type, so merging
  it is part of establishing the item, not a second item.

Everything else is another step's: functions, types, globals and macros the
item's users also touch, the struct holding the item, the function it is passed
to. Do not rename, retype, restructure or re-document them, even when your
analysis makes the right answer obvious. That analysis is exactly what their
own step needs, so put it in the review file as a `followup` issue - what the
symbol is, the evidence, the change you would make. Spell the symbol's current
name in it: that symbol's brief collects earlier follow-ups by searching for its
name, so its step starts from your analysis rather than from the old name.

The one exception is a change the item's own change cannot compile or match
without, such as a parameter whose type is the item's type. Make the minimum
such change, keep the other symbol's name, and list it in the report.

## Asset steps

A model, an animation set or a collision grid embedded in an overlay is one
step: its record (`TmdSource`, `AnimationSet`, the grid's `GpGridParams`)
together with the arrays whose initializers come from `assets/*.inc`. Code
reaches those arrays only through the record - except the few functions that
edit a live grid in place - so naming the asset is one decision. Name the record
for what the asset is, established from the code and descriptor tables that
use it, and name each array from the record and its role, the same way for
every asset of the kind: the include's last word says which role an array has.
An array nothing outside its translation unit reaches is private - `static`,
with the `_` prefix - where the build still matches; a grid array another
package edits stays public.

## Changing code is expected

Over the items done so far, "naming" an item has meant widening a field's type
because every user was casting, replacing a phantom overlay type with a nested
member, dropping casts that a corrected type made redundant, moving a private
declaration out of a header, and rewriting the prose that described any of it.
Edit callers, headers and sources as the finding requires.

**You may break the match while you work.** The image and individual-function
checks are acceptance tests. If restructuring a type changes code generation,
that is a normal intermediate state, and restoring it is part of your job rather than a reason to
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
- **The declared type** is next. Casts are evidence to investigate, not proof
  that a type is wrong. Distinguish incorrect declarations from byte views,
  hardware address encodings, alignment tests, SDK sentinels and other necessary
  conversions. Read the relevant audit findings and verify their reasoning
  against the current source before changing or retaining a cast.
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

  Sweep for stale spellings after a rename, including tags and configuration
  references. The review report and driver's ledger record completion; absence
  of an old spelling alone does not establish that the item was understood.
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
- **Choose aggregates and unions from the accesses.** A nested member expresses
  an established containing-object relationship. A union with named members can
  express genuinely different interpretations of the same storage. A failed
  matching attempt alone proves neither relationship: register allocation and
  expression shape can also explain it. Do not invent a union merely to hide a
  cast or make an audit count fall. Record the intended type and actual compiled
  obstruction when a justified change still needs rematching.

## Review the implementation and its consumers

- **Resolve meaningful literals now.** Name established states, flags, message
  IDs, sentinels and fixed-point scales, reusing existing constants. Replace
  allocation, clear and copy sizes with `sizeof` and array bounds with
  `ARRAY_SIZE` only when the object and full extent are established. A partial
  transfer or serialized format length may need a separate named constant.
  Preserve integer widths, signedness, promotions, evaluation and layout; an
  enum constant does not require changing a stored byte into an enum field.
  Ordinary arithmetic constants need no ceremonial names. Keep constants in
  the narrowest scope that covers their actual users.
- **Read the whole function body.** Name meaningful locals as well as parameters.
  Separate unrelated uses of a temporary and simplify decompilation scaffolding
  when matching permits. Preserve side effects, evaluation order, overflow and
  truncation behavior. Do not change an algorithm or retained behavior because
  it looks like a bug. If the rename tool cannot address a local, make a scoped
  edit after checking shadowing and report the old/new spelling.
- **Look for the helpers the original was written with.** When the item is a
  function, a long body often inlines smaller operations the original
  developers wrote once and reused, and a construct kept only to force a match
  is often one of them expanded by hand. Where a run of statements inside the
  item does one nameable thing - unlinking a node, packing a colour, stepping
  an animation cursor - try it as a `static inline` function or block-scoped
  macro in the item's own file, and keep it only if the function still
  matches. Test both forms, since they compile differently. Only a non-trivial
  sequence earns a helper: one wrapping a single expression, assignment or call
  adds a name to look up without explaining anything, so leave those inline.
  When the same sequence also appears in other functions, do not extract it
  across them - that is code other steps own, and a shared helper's name and
  home are a decision of their own. Record it as a `followup` naming the
  sequence and the functions it appears in.
- **Use the cast backlog.** The brief supplies relevant prior findings when
  available. Follow old/new names through `local/renames.tsv`; do not rerun
  historical mutation scripts. Run `tools/check_pointer_casts.py` and
  `tools/check_pointer_arithmetic.py` on affected source TUs, including carriers
  of shared source and users of a changed header. Inspect pointer/integer,
  pointer/pointer and callback conversions. Use `PARENT_OF` when evidence
  establishes the containing object and member. Correct callback declarations,
  definitions, dispatch tables and callers together. Absence from a supplied
  report is not proof that there are no suspicious accesses.
- **Establish contracts and bounds.** Identify bytes versus elements, frames,
  angle/coordinate units, fixed-point scales, index domains, sentinels, ownership
  and lifetime where relevant. Check aliases, indirect callers, complete access
  widths and bulk copy/clear lengths across overlays. Assume accesses must be
  in bounds; derive the range from code rather than from the existing array
  declaration. Observed accesses prove a minimum extent, not an object's end.
  Do not fill gaps, enlarge objects, change BSS alignment, or label unknown
  bytes as padding to make a proposed type match. Keep unproved bounds explicit.
- **Propagate through every instance.** Review all consumers and build variants
  of included shared source. Equal load addresses or layouts do not establish
  identity. Keep shared interfaces consistent across overlays and update symbol
  maps/imports as required. One-TU declarations belong in the source prologue;
  overlay-shared declarations in private headers beside the source; cross-overlay
  declarations in public headers under `include/`. Included-source interfaces
  can be shared while their per-instance functions retain static C linkage.
  Remove newly redundant declarations and includes, respecting `NAMING.md`'s
  include grouping and existing BSS first-declaration ordering exceptions.

Previously compiled failed attempts are useful evidence, not instructions to
repeat every experiment. Retry with a reasoned new approach; if a substantial
rematch or debugger observation remains necessary, retain the matching code and
record the specific next step in the structured review.

## Names

Functions and data use lowerCamelCase, one identifier, no separators, opening with the module or
package that owns the symbol. A leading marker carries the rest:

| | |
|---|---|
| no marker | a function — `fsLoadFile`, `gunbladeFireRound` |
| leading `g` | a global — `gFsFileTable` |
| leading `_` | private to its translation unit — `_fsReadSector`, `_gSectorCache` |
| PascalCase | a type — `FsCdfFile`; a private one is `_SectorCache` |

A private symbol is the public name with `_` prepended. A struct tag has the
same spelling as its type. Choose the owner using `NAMING.md`'s subsystem guide
and the item's actual implementation, interface and consumers:

- Main and gameplay use subsystem prefixes: `fs`, `cap`, `inventory`,
  `actorRender`, `worldCollision`, and so on. Gameplay has no blanket `gp`
  namespace. Do not mechanically convert `Gp_*` to `gp*` or substitute
  `gameplay*`; apply the same ownership reasoning to globals and types.
- A `current` classification checks spelling, not ownership. Reassess existing
  `gp...` names too. One TU or header may contain several subsystems, and one
  subsystem can span several TUs; choose the prefix per item, not per file.
- Package-specific routines use the manifest key camelCased
  (`mine_mesa` → `mineMesa…`), never abbreviated. Exported resident APIs keep
  their subsystem identity even when many packages call them.
- Included shared source uses its own subsystem (`actorContact`, `capCaption`,
  `planarReflection`, etc.), with `_` on static instances. Overlay wrappers use
  the package's prefix. Neither the first carrier nor a generic `Shared` marker
  defines the implementation's identity.

If the guide lacks a proven subsystem, establish and document its responsibility
and interface before choosing a new prefix, and check for collisions. The guide
does not require changing headers, TU splits or linkage merely to fit a name.

**Macros are first-class review items.** Use UPPER_SNAKE_CASE; any subsystem or
package prefix is spelled in full (`INVENTORY_`, `WORLD_COLLISION_`,
`ROOM_VISUAL_EFFECTS_`, `FILE_SYSTEM_`), never shortened like `GP_`, `INV_`,
`FX_` or `FS_`. Generic common helpers such as `ARRAY_SIZE`, `PARENT_OF` and
`ALIGN` need no prefix. **GTE macros are the exception:** retain established
PsyQ-style names such as `gte_RotTransLV`, including project wrappers. Macro
parameters can use local camelCase. Do not add private `_` or global `g` markers.
Macros have preprocessor scope, not C linkage; place them with their consumers
under the same source/private/public-header rules without applying `static`.

Review constants, helpers, alias/accessor macros and shared-source configuration
bindings: meaning, units, captured identifiers, argument evaluation, side
effects, parentheses, types/promotions and control flow. Inspect all definitions,
inactive branches, `#undef`, stringification and token pasting, and all carriers
and manifest variants. Document the contract beside the definition, including
configuration requirements. Convert to an enum/constant/inline function only
when its semantics and matching permit. SDK definitions, guards and compiler
plumbing are excluded; project GTE wrappers can still need semantic review.

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

Make every supported rename with `rename_item.py` - symbols, types, fields, parameters and macros
alike - and never with a hand edit, a `sed`, or a script of your own:

    venv/bin/python3 tools/refactor/rename_item.py <file>/<oldName> <newName> --sidecars

**`<file>` is where the item is *declared*, not where it is used.** For a
function with a C body that is its own `.c`; for one whose body is assembly, or
for anything else declared in a header, it is the header. Naming a file that
merely references the item cannot resolve it, and the failure is slow and
silent: resolution tries the named file, then every other translation unit in
the compilation database, one at a time, with no progress output, before
exiting `could not resolve`. On a loaded machine that is many minutes of what
reads as a hang. So a run that has printed nothing for a while is a spec to
re-check, not a run to wait out - and the first line of a healthy run names the
declaration it resolved, which is also the check that you asked about the item
you meant.

For C declarations, there are two reasons. It resolves references
through the C parser to distinguish declarations, including unrelated fields
with the same name. Macro expansion and non-C uses still need the separate
checks below. And it
appends the old and new spelling to `local/renames.tsv`.

**That file is now a source of truth.** The pass decides what has already been
handled by reading it together with the driver's step ledger; nothing is
inferred from what a name looks like any more. So a rename made by any other
means leaves no row, the item reads as untouched and is queued again, and the
change cannot be propagated or audited. Renaming by hand does not just skip some
bookkeeping - it puts the worklist out of step with the tree.

If the rename is one the tool cannot express, do it by hand and say so plainly
in your report, naming the old and new spelling, so the row can be added.

**For macro items, the assigned name is `<definition-file>/<MACRO>`.** Pass it
directly to `find_references.py` and `rename_item.py`, without prepending another
path. Keep that qualified identity in the review's `name`; `current_name` is the
final identifier. References are lexical candidates across conditional branches,
not resolved expansions. Inspect the full report under `local/refs/`, include
order, SDK collisions and `#`/`##` constructions. Run a rename dry run before
using `--macro-reviewed`; that flag acknowledges the reviewing agent's checks,
not a need for user approval. The tool refuses ambiguous shared bindings and
manifest keys: update those definitions, consumers and build configurations
together, then record each qualified old/new identity using
`rename_item.record_rename` as described in `NAMING.md`. Comments, strings and
sidecars are outside macro token edits and need a separate sweep. Do not remove
same-spelled macros in unrelated TUs as stale references.

**The parser-resolved pass cannot see every occurrence, so sweep after it.** The
step's completeness check - a word-boundary grep of `src` and `include` for the
old spelling - passes with these still in the tree, and each of them is a stale
reference the next reader will follow:

- a symbol named inside an inline-asm string, which is a relocation the parser
  never reads rather than a reference it resolves;
- a block comment's continuation lines, which count as comments only when they
  start with `//`, `///`, `*` or `/*`;
- a second whole-word mention on one markdown line, of which only the first is
  rewritten;

    grep -rnw <OldName> . --exclude-dir=.git --exclude-dir=build \
        --exclude-dir=asm --exclude-dir=linkers --exclude-dir=assets \
        --exclude-dir=rom --exclude-dir=venv

Fix each by hand and name the spellings in the report, as above.

**Renaming a placeholder-spelled name takes `--no-comments`.** The markdown
branch of the comment pass has no per-symbol filter, so it rewrites every
backticked use of the name in the learnings file and in `NAMING.md`, including
the generic ones in the conventions and the prose about unrelated types that
declare the same member. That is not limited to a parameter's `argN`: a dry run
on one `field_XX` member reported 243 comment edits outside the code, nearly all
of them other structs' fields. Rename with `--no-comments`, then update the
mentions that really are this item's by hand. `find_references.py` already
leaves prose out for a generated name, so a listing that reports no mentions is
not evidence that a rename would leave none - the two tools differ here on
purpose, and the rename is the one that needs telling.

## What the tools reach, and what they do not

The C resolver and the lexical macro inventory have different limits. Knowing where their edge is
saves both halves of the usual failure - trusting them with something they never
touched, and re-doing by hand what they already did. `NAMING.md`'s Tooling
section states the same boundary for readers outside this pass; keep the two in
step.

`rename_item.py` reaches:

- every declaration and reference the parser resolves to the item, across the
  whole compilation database, matched by USR rather than by spelling;
- mentions of the name in comments in `.c` and `.h`, and in the markdown at the
  repository root and under `doc/` - unless `--no-comments`;
- with `--sidecars`, whole-word hits in the version's `configs/` tree: symbol
  maps, splat configs, the overlay manifest;
- macro definitions and unambiguous lexical candidates after caller review, as
  described above; this does not use libclang or the C prose/sidecar pass;
- a ledger row in `local/renames.tsv`, for functions, globals, types and qualified macros. A field
  or a parameter gets none by design, so say in your report that you renamed
  one.

`find_references.py` reaches the same C references, and with `--asm` the
generated assembly under `asm/`. It answers for what the item contains as well:
asking about a type also reports every one of its fields, and asking about a
function also reports every one of its parameters, from a single scan. So the
step does not need a query per member - the counts are already in front of you,
and the individual sites are in the file the run names under `local/refs/`.
`--shallow` asks about the named symbol alone.

One gap still matters when the item is a type: a type no code spells by name is
reached only through the member that declares it, and the listing reports none
of those uses - it can name only a mention in prose while the functions that
touch that member use the type throughout. The `referrers` count above comes
from the dependency graph, which does see them, so a listing far smaller than
that count is the signal. Ask about the owner (`find_references.py
<header>/<Owner>`), whose answer now names the member carrying the type and
counts its uses directly; deleting or inlining an "unused" type is the wrong
move when its uses are simply filed under its owner.

Everything below is outside both, and is yours to do by hand and to name in the
report:

- **Handwritten assembly.** A `.s` under a source tree is a source file, not an
  artifact: its `glabel` and `alabel` lines, its header comment, and any branch
  to a sibling symbol are hand edits. `--asm` does not scan these, so grep them
  yourself. Generated assembly under `asm/` is the opposite case - never edit
  it; its names come from the symbol map and the next split.
- **A symbol that exists only in assembly.** With no C declaration there is
  nothing to resolve; the rename is a symbol-map entry, the `INCLUDE_ASM`
  argument, and a re-split. The tool says so when it recognises the case.
- **A symbol map's prose.** `--sidecars` rewrites the symbol being renamed. A
  neighbouring line whose note *names* it - an alabel described by the sibling
  it enters - is prose, and stays stale until you fix it.
- **Inline assembly in C**, which is a relocation the parser never reads.
- **C-symbol references reached through a macro**, which the tool lists rather than
  edits: the macro body is where the name is spelled.
- **Prose outside the scanned set** - the rules files, tool docstrings, anything
  under a directory the comment pass does not walk.
- **Generated linker scripts**, deliberately: they are rebuilt by the next
  split, and writing to them only desynchronises a revert.

After the tool and the hand edits, sweep for the old spelling as the section
above describes; what the sweep turns up is the measure of how far the tools
actually got.

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
the body is released" is the field; "counted down by `taskCountdownCallback`"
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

**Function bodies take sparse, coarse `//` comments at meaningful boundaries.**
Explain the purpose of non-obvious phases, relevant invariants, and why unusual
operations or ordering are necessary. State transitions, coordinate conversions,
packed formats, resource ownership and multi-stage calculations often warrant
one. For example: `// Transform the collision point into local coordinates.`
Do not narrate statements, repeat names, or comment every loop and branch.
Small, clear functions may need no internal comments. Mark uncertain
interpretations explicitly and keep investigation history in the review report.
The declaration explains the contract; body comments explain significant steps
and constraints. Document parameter/return units, ranges, sentinels and lifetime
requirements when a caller needs them.

## What this compiler allows

GCC 2.8.1, `-O2`, no `-finline-functions`.

- **Anonymous struct and union members do not work.** The declaration is
  accepted and every access to it is then rejected. Nest with *named* members:
  `owner->at4.loc`, not `owner->loc`. `_GpuStatusRegister` in
  `src/main/gpuext.c` is the worked example: its bitfields sit in a struct
  member named `bits`.
- `static` is safe more often than it looks, since nothing is inlined unless
  explicitly marked.
- `.text` and `.rodata` are emitted in source order, so functions keep their
  sequence and a position-sensitive global cannot be hoisted.

## Finishing

Run `venv/bin/python3 tools/refactor/verify_name_pass.py`. It checks the normal
matching build, declarations across images, symbol ownership, and every
individual function in a freshly generated objdiff report, then restores the
normal build configuration. All image checksums and individual functions must
match. Do not accept an aggregate percentage that hides an individual failure.
Do not introduce new implicit declarations or unresolved prototype conflicts.
Do not commit; the driver commits and independently runs the checks.

Fill the JSON review file named in the brief, with one entry per assigned item:
its current name, established meaning, evidence, changes and unresolved issues.
Use outcome `complete` only when the review has no outstanding questions or
required cleanup. Use `followup` for remaining rematching, runtime observations,
semantic uncertainty or unrelated discoveries. Each issue needs a kind, source
location, reason and concrete next step; retain existing audit IDs when known.
A rename or passing build alone does not complete a review. A fully reviewed
item needing no source change is valid when the report explains the evidence.
The driver preserves reports under `local/name-pass/reviews/` and records
`followup` separately from `ok`, so an accepted rename cannot erase unfinished
cleanup. Both outcomes finish this naming visit; follow-ups remain separate work.
Do not put case-specific backlogs in general documentation.

**The step ends when your session ends, so nothing may still be running.** The
driver builds the tree the moment you stop and reverts the step if that build
fails - it has no way to know you were waiting on something. A rename left in a
background job, a build you have not read the end of, or a half-applied change
whose other half was going to land next turn all cost the whole step, including
the analysis behind it. Run the last things in the foreground, and if a job is
genuinely too slow to wait for, abandon it and report that rather than signing
off over it.

Report what the item turned out to be, what you changed beyond the name, any
other symbol you had to touch and why, and anything you could not make match.
