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
