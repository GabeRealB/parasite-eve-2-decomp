#ifndef ACTORS_SHARED_80137220_H
#define ACTORS_SHARED_80137220_H

#include "actors/actor_107000.h"
#include "main/task.h"

/// Advances the revealed second-form specimen, updates its animation and
/// reaction rotation, and plays its landing sound. Shared by actors 107000
/// and 207000, whose second-form work layouts are identical.
void ActorsShared80137220(Actor107000Ctx* arg0, Task* arg1);

#endif
