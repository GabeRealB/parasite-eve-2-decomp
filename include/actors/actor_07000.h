#ifndef INCLUDE_ACTORS_ACTOR_07000_H
#define INCLUDE_ACTORS_ACTOR_07000_H

#include "main/task_types.h"

/// Sucklerceph spawn descriptors: 0 standing, 1 hidden/drop, both with TMD bodies.
///
/// Indices are 0..1 with no terminator, at priority 96. The hidden/drop entry
/// skips automatic model buffers and waits for a room command to reveal it.
/// Enemy spawns supply the owning Enemy in spawnArg2; spawnArg1's low halfword
/// selects the sound variant. A signed high halfword of 1 cancels a hidden spawn.
/// Keep the callbacks and borrowed model geometry loaded while tasks use them.
extern TaskDesc gActor07000SucklercephTaskDescs[2];

/// Slouch spawn descriptors: 0 standing TMD body, 1 coordinate-body projectile, 2 hidden/drop TMD body.
///
/// Indices are 0..2 with no terminator, at priority 96. The hidden entry skips
/// automatic model buffers and waits for a room command. Enemy spawns supply
/// the owning Enemy in spawnArg2; a signed high halfword of 1 in spawnArg1
/// cancels a hidden spawn. Projectile spawnArg1 is its aim yaw, 4096 units per turn.
/// Keep the callbacks and borrowed model geometry loaded while tasks use them.
extern TaskDesc gActor07000SlouchTaskDescs[3];

/// Slot export of `gActor07000SucklercephTaskDescs` from actor_207000, with the same descriptor contract.
extern TaskDesc gActor207000SucklercephTaskDescs[2];

/// Slot export of `gActor07000SlouchTaskDescs` from actor_207000, with the same descriptor contract.
extern TaskDesc gActor207000SlouchTaskDescs[3];

/// Slot export of `gActor07000SucklercephTaskDescs` from actor_107000, with the same descriptor contract.
extern TaskDesc gActor107000SucklercephTaskDescs[2];

/// Slot export of `gActor07000SlouchTaskDescs` from actor_107000, with the same descriptor contract.
extern TaskDesc gActor107000SlouchTaskDescs[3];

#endif // INCLUDE_ACTORS_ACTOR_07000_H
