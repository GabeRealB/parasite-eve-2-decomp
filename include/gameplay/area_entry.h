#ifndef GAMEPLAY_AREA_ENTRY_H
#define GAMEPLAY_AREA_ENTRY_H

#include "main/task_types.h"

/// Slot of the controlled character in `gPlayerActorTasks`.
#define PLAYER_ACTOR_TASK_PLAYER 0

/// Slot of the companion actor in `gPlayerActorTasks`.
#define PLAYER_ACTOR_TASK_COMPANION 1

/// Number of slots in `gPlayerActorTasks`.
///
/// The slots are the controlled character and the companion. An empty slot
/// still counts toward this length, and a full walk stops before it.
#define PLAYER_ACTOR_TASK_COUNT 2

/// Live tasks of the controlled character and the companion.
///
/// `PLAYER_ACTOR_TASK_PLAYER` is claimed when the player actor enters its first
/// state and cleared when that actor tears down. `PLAYER_ACTOR_TASK_COMPANION`
/// is claimed and cleared the same way by the companion actor, and stays NULL
/// when no companion is spawned. Area start clears both before either actor is
/// spawned again. Each task's `work` is that actor's `GameActor`. NULL means
/// that actor is not currently set up. Lock-on walks every entry. A category-2
/// contact selects an entry with bit 7 of its id. A sensor table that contains
/// a category-1 contact indexes with bit 7 of its first entry's id.
extern Task* gPlayerActorTasks[PLAYER_ACTOR_TASK_COUNT];

#endif // GAMEPLAY_AREA_ENTRY_H
