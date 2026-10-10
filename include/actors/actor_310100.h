#ifndef INCLUDE_ACTORS_ACTOR_310100_H
#define INCLUDE_ACTORS_ACTOR_310100_H

#include "gameplay/animation.h"

#include "main/task_types.h"

/// Player choreography clips used by the Acropolis police-officer scenes.
///
/// Indices 0..9 and 11 hold clips; 10 and 12 are NULL and cannot be played.
/// Actor-310100 officer controllers and Acropolis Plaza borrow this table for
/// animation messages to the player. Both this actor overlay's table and the
/// room overlay's clip resources must remain loaded through playback.
extern AnimationSet* gActor310100PlayerAnimationSets[13];

/// Police officer 1 spawn recipes: 0 movie controller, 1 animated body, 2 culled body.
///
/// Priority 192. Indices 0..2 have no terminator.
/// Both TMD model entries skip automatic primitive buffers.
/// The controller has no body; its room spawn carries a live `Enemy` in
/// `Task::spawnArg2`. Model entries use `Task::spawnArg1` as a body animation id and ignore
/// `Task::spawnArg2`. Their work is owned by the model task; model swaps release
/// the previous model before starting another.
/// Keep actor_310100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor310100Officer1TaskDescriptors[3];

/// Police officer 2 spawn recipes: 0 movie controller, 1 animated body, 2 culled body.
///
/// Priority 192. Indices 0..2 have no terminator.
/// Both TMD model entries skip automatic primitive buffers.
/// The controller has no body; its room spawn carries a live `Enemy` in
/// `Task::spawnArg2`. Model entries use `Task::spawnArg1` as a body animation id and ignore
/// `Task::spawnArg2`. Their work is owned by the model task; model swaps release
/// the previous model before starting another.
/// Keep actor_310100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor310100Officer2TaskDescriptors[3];

#endif // INCLUDE_ACTORS_ACTOR_310100_H
