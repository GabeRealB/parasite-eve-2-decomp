#ifndef PEPPER_SPRAY_PEPPER_SPRAY_H
#define PEPPER_SPRAY_PEPPER_SPRAY_H

#include <engine/task_types.h>

/// A Pepper Spray spark, sized by `spawnArg1`: it flies out along the player's facing, spreading, through its cells.
void pepperSpraySparkTask(Task* task);

/// The Pepper Spray: plays its cue and sprays a spark each frame for a while, applying the item partway; a held
/// attachment or any pause of the room's PE stops the cue and frees it.
void pepperSprayEffectTask(Task* task);

#endif // PEPPER_SPRAY_PEPPER_SPRAY_H
