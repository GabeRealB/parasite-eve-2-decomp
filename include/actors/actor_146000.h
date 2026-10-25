#ifndef INCLUDE_ACTORS_ACTOR_146000_H
#define INCLUDE_ACTORS_ACTOR_146000_H

#include "main/task_types.h"

/// Night water-hole scene spawn table: unused entry 0, scene controller at entry 1.
///
/// Both entries are bodyless, without a terminator. Entry 1 runs at priority `TASK_PRIORITY_SCENE`
/// with unused spawn arguments and selects the burner-dependent scene before
/// reloading the night driveway. Keep actor_146000 and its room/CAP resources
/// loaded through handoff. The empty room placement never spawns entry 0, whose
/// priority-192 callback retains a two-argument enemy address; do not spawn it.
extern TaskDesc gActor146000WaterHoleSceneTasks[2];

#endif // INCLUDE_ACTORS_ACTOR_146000_H
