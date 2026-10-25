#ifndef INCLUDE_ACTORS_ACTOR_342400_H
#define INCLUDE_ACTORS_ACTOR_342400_H

#include "main/task_types.h"

/// Spawn recipe indices for the incinerator encounter controller and its row spawners.
enum {
    ACTOR_342400_ENCOUNTER_CONTROLLER_DESCRIPTOR = 0,
    ACTOR_342400_MAD_CHASER_SPAWNER_DESCRIPTOR   = 1,
    ACTOR_342400_SLOUCH_SPAWNER_DESCRIPTOR       = 2,
    ACTOR_342400_PAIR_SPAWNER_DESCRIPTOR         = 3,
    ACTOR_342400_ENCOUNTER_DESCRIPTOR_COUNT      = 4,
};

/// Bodyless tasks driving the incinerator's mixed-enemy encounter.
///
/// Index 0 starts the controller at priority `TASK_PRIORITY_SCENE`; 1 Mad Chaser, 2 Slouch and
/// 3 Sucklerceph-pair spawners run at priority 97. Spawner spawnArg1 packs a row
/// 0..16 in the high halfword and an appear command in the low halfword.
/// There is no terminator. Keep actor_342400 and the selected enemy/room
/// resources loaded until the controller and spawners finish.
extern TaskDesc gActor342400EncounterTaskDescs[ACTOR_342400_ENCOUNTER_DESCRIPTOR_COUNT];

/// Mad Chaser spawn recipes: 0 ordinary, 1 waits for a scripted emerge command.
///
/// Both create the nine-part TMD at priority 96. Index 1 skips automatic model
/// buffers; emergence allocates them explicitly. Supply a live Enemy as the
/// second spawn argument, normally through enemySpawnFromTable. The low nibble
/// of the first argument selects the spawn kind; scripted bit 16 cancels spawn.
/// Keep actor_342400's code and borrowed model/animation resources loaded while
/// tasks use them. Only indices 0..1 are exposed here; there is no terminator.
extern TaskDesc gActor342400MadChaserTaskDescs[2];

#endif // INCLUDE_ACTORS_ACTOR_342400_H
