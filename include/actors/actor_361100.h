#ifndef INCLUDE_ACTORS_ACTOR_361100_H
#define INCLUDE_ACTORS_ACTOR_361100_H

#include "types.h"

#include "main/task_types.h"

/// Water presentation recipe for the pod-bottom scene while actor data streams.
///
/// Creates a coordinate-body task at priority 192. Requires actor_361100 and
/// actor_403600 loaded, initialized graphics and the slot-2 streaming window.
/// View 11 refracts the framebuffer, 12 draws the ripple, and 10 ends the task.
/// The stream's free tail is borrowed for packets through GPU consumption.
extern TaskDesc gActor361100StreamWaterTaskDesc;

/// Spawn recipe of the pod scene's nineteen-part tentacle, room resource 45.
///
/// Runs at priority 192 with a TMD body and deferred primitive-buffer allocation.
/// The scene supplies a live Enemy in spawnArg2; the task owns its rig/work and
/// borrows this package's model and animation data until teardown.
extern TaskDesc gActor361100TentacleTaskDesc;

/// Spawn recipe of Aya's nineteen-part scripted body, room resource 132.
///
/// Runs at priority 192 with a TMD body and deferred primitive-buffer allocation.
/// The scene supplies a live Enemy in spawnArg2; the task owns its rig/work and
/// borrows this package's model and animation data until teardown.
extern TaskDesc gActor361100AyaBreaTaskDesc;

/// Clears the retained scene head-aim task handle before starting the pod event.
///
/// actor_361100 must be loaded. Call before spawning its head-aim task or after
/// prior tasks have been torn down: this only clears the handle and does not kill
/// a live task or free its work. unused is ignored; the room passes 0.
void actor361100ClearHeadAimTaskHandle(s32 unused);

#endif // INCLUDE_ACTORS_ACTOR_361100_H
