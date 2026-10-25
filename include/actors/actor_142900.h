#ifndef INCLUDE_ACTORS_ACTOR_142900_H
#define INCLUDE_ACTORS_ACTOR_142900_H

#include "main/task_types.h"

#include "gameplay/evs.h"

/// First-visit Shelter B2 elevator scene with player/companion choreography and screen shake.
///
/// The room starts this with HUD hide/restore and the paired skip script.
/// Installs scene clips, follows CAP cues, operates the door, then reloads the
/// elevator hall and releases both actors. All 87 commands include END.
/// Keep actor_142900, the elevator room, both actors and CAP resources loaded
/// while the interpreter borrows commands, callbacks and animation data.
extern EvsCommand gActor142900ElevatorFirstVisitScript[87];

/// Recovery path for skipping the first-visit Shelter B2 elevator scene.
///
/// Under the primary fade, stops shaking, runs the short CAP continuation,
/// reloads the hall, restores player placement and both actors' weapon effects,
/// and releases scripted control. All 18 commands include END. The same room,
/// actor and CAP resources as the normal path must remain loaded through END.
extern EvsCommand gActor142900ElevatorFirstVisitSkipScript[18];

/// Elevator scene spawn table: unused entry 0, vertical screen shake at entry 1.
///
/// Both entries are bodyless at priority `TASK_PRIORITY_CONTROL`, with unused spawn arguments and
/// no terminator. Keep actor_142900 loaded while the shake task runs, with at
/// most one live instance. The room loads this table with an empty placement
/// list. Entry 0 retains a two-argument enemy callback; do not spawn it.
extern TaskDesc gActor142900TaskTable[2];

#endif // INCLUDE_ACTORS_ACTOR_142900_H
