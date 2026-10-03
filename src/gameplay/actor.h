#ifndef GAMEPLAY_PRIVATE_ACTOR_H
#define GAMEPLAY_PRIVATE_ACTOR_H

#include "common.h"

/// Start-up choices for a player or companion actor, given beside its
/// `ActorSpawnTransform`.
///
/// The spawn routine copies `initialAnimationId` into the new actor, whose
/// first task state consumes it. A companion resets its animation slots to
/// that animation. The player plays it only when `startScripted` is set: the
/// actor then begins in `GAME_ACTOR_MODE_SCRIPTED`, playing the animation from
/// its first frame with world collision dropped, so that the room's script can
/// take it from there. Otherwise the player starts under normal control and
/// the animation id goes unused.
///
/// Companion spawning reads only `initialAnimationId`; `startScripted` has no
/// effect there. Set both members: the record is normally a stack local. It is
/// read before the spawn call returns. The task keeps the pointer as its second
/// spawn argument, but no reader of it has been found, so the record is not
/// known to need a lifetime beyond the call.
typedef struct {
    u16 initialAnimationId; // Animation id within the actor's current bank, played or reset to at start-up
    u8  startScripted;      // Player only (0 normal control, nonzero start in scripted mode)
} ActorSpawnOptions;
STATIC_ASSERT_SIZEOF(ActorSpawnOptions, 0x4);

#endif // GAMEPLAY_PRIVATE_ACTOR_H
