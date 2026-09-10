#ifndef ACTORS_SHARED_8016974C_H
#define ACTORS_SHARED_8016974C_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Returns 1 when bit 0 of the work block's `flags_EC` halfword or any of
/// bits 0x102 of the whole word is set, 0 otherwise. Shared by `actor_341700`
/// and `actor_342400`, whose work blocks are the same
/// `ActorsShared80168d3cWork`; `actor_341700` calls it unprototyped so the
/// caller's own `Task*` stays in `$a0` without a copy.
s16 ActorsShared8016974c(Task* arg0);

#endif
