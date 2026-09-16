#ifndef ACTORS_SHARED_8014ADFC_H
#define ACTORS_SHARED_8014ADFC_H

#include "common.h"

#include "actors/actor_104000.h"

/// Shared by `actor_104000` and `actor_204000`: in states 2 and 3, reports
/// 0x40280001 the first time the animation id in `field_4A` reaches one of the
/// state's trigger ids (latched in `field_474`); in state 5, 0x400C0005 while
/// bit 2 of `field_58` is set. Returns 0 otherwise.
s32 ActorsShared8014adfc(Actor104000Work* arg0);

#endif
