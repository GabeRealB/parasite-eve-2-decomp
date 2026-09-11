#ifndef ACTORS_SHARED_8016B104_H
#define ACTORS_SHARED_8016B104_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Counts `field_412` up against the `field_446` hold. Once the hold runs
/// out, flips the global LCG and enters state 4 or 1 on its bit 16; before
/// that, enters state 3 when `field_43A` (the distance to the nearer player
/// actor) is under 0xDAC, and otherwise advances the sub-state while it is
/// under 0x1388. Shared by `actor_341700` and `actor_342400`, whose work blocks
/// are the same `ActorsShared80168d3cWork`.
void ActorsShared8016b104(Task* arg0);

#endif
