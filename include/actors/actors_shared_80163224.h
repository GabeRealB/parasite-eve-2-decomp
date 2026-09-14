#ifndef ACTORS_SHARED_80163224_H
#define ACTORS_SHARED_80163224_H

#include "common.h"

#include "main/task.h"

/// Writes the draw/alloc bits of the task's `TmdObject` extra (`field_C`):
/// mode 0 hides the model (set 0x80) and allows buffer alloc (clear 0x4),
/// mode 1 shows it and allows alloc, mode 2 hides it and skips alloc.
///
/// Shared by `actor_121300`, `actor_341900` and `actor_342000`, where the only
/// reference to each copy is a `.word` in that overlay's trailing data - a
/// dispatch table entry, so `arg2` is the state the table was entered with.
/// The same 0x7C bytes are also matched separately as `Room_Util19`; a copy in
/// another family is a separate promotion, because the span goes into that
/// family's manifest and the symbol into that family's map.
void ActorsShared80163224(Task* arg0, s32 arg1, s32 arg2);

#endif
