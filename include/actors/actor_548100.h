#ifndef ACTORS_ACTOR_548100_H
#define ACTORS_ACTOR_548100_H

#include "main/task_types.h"

/// Spawn recipe for the mine refuge's interactive power-panel task.
///
/// Spawns a bodyless task at priority 192. Its first update allocates panel
/// work and a child action cursor; the room polls the panel's exit request
/// and relays battery-item use while it is live. The actor overlay and the
/// refuge's caption resources must remain loaded until the task is released.
extern TaskDesc gActor548100PanelTaskDesc;

#endif // ACTORS_ACTOR_548100_H
