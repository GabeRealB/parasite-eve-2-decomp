#ifndef ACTORS_SHARED_8016B500_H
#define ACTORS_SHARED_8016B500_H

#include "common.h"

#include "main/task.h"

#include "actors/actors_shared_80168d3c.h"

/// Unless `ActorsShared8016945c` claims the frame, and on the same `flags_EC`
/// test as `ActorsShared8016b370`, clear the frame counter, set `field_438`,
/// request animation 4 at speed 0x10 and advance `field_422`. Shared by
/// `actor_341700` and `actor_342400`, whose work blocks are the same
/// `ActorsShared80168d3cWork`.
void ActorsShared8016b500(Task* arg0);

#endif
