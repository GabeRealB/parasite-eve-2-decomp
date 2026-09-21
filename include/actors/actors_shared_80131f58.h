#ifndef ACTORS_SHARED_80131F58_H
#define ACTORS_SHARED_80131F58_H

#include "common.h"

#include "actors/actor_104000.h"
#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Pushes `coord` away from the obstacles in `recs`. Records of kind 0x10000
/// (which also raises the returned `blocked` flag) or 0x30000 each give a
/// bearing, at most eight; bearings more than 0x400 apart cancel each other.
/// Each survivor becomes a 10-unit step added to `push` and to the translation.
///
/// Shared by `actor_101200`, `actor_104000`, `actor_301200` and `actor_304000`.
/// First-function form of the walk in `func_actor_204000_8014A06C`: splat cut
/// the hoisted `gGameSession` load, so the body reads `viewReady` through an
/// uninitialized session pointer that GCC keeps in `$v0`.
s32 ActorsShared80131f58(GsCOORDINATE2* coord, GpRec18* recs, s16 count, SVECTOR* push);

#endif
