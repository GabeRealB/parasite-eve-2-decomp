#ifndef ACTORS_SHARED_8016BEF0_H
#define ACTORS_SHARED_8016BEF0_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// While `field_41E` is 1, consumes the pending request in `field_448`:
/// 3 jumps the work block's state machine to state 8 and 5 to state 9, both
/// at sub-state 0, and any other value is just cleared. Returns 1 when
/// `field_41E` is 1 and 0 otherwise. Shared by `actor_341700` and
/// `actor_342400`, whose work blocks are the same `ActorsShared80168d3cWork`;
/// `Actor04400_Fn08DBC` is the same source inside `actor_104400_text`.
s32 ActorsShared8016bef0(Task* arg0);

#endif
