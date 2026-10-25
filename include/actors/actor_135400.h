#ifndef INCLUDE_ACTORS_ACTOR_135400_H
#define INCLUDE_ACTORS_ACTOR_135400_H

#include "main/task_types.h"

/// Gary Douglas spawn recipes: 0 body, 1 head and hat, 2 carried model.
///
/// All three create TMD bodies at priority `TASK_PRIORITY_CONTROL`; there is no terminator. Entry 0
/// skips automatic model buffers and expects a live owned `Enemy` in spawn
/// argument 2. The body spawns entries 1 and 2 with itself in argument 2 and
/// parent coordinate indices 4 and 8 respectively in argument 1. The children
/// borrow its coordinates and lighting. Keep actor_135400 loaded until teardown.
extern TaskDesc gActor135400GaryDouglasTasks[3];

/// Flint spawn recipe for the night garage scene.
///
/// Priority 192. Spawn at index 0.
/// TMD bodies skip automatic primitive buffers.
/// Body `Task::spawnArg2` borrows its live `Enemy`; actor-control holds suspend dispatch.
/// Keep actor_135400 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor135400FlintTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_135400_H
