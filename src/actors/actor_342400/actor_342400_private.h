#ifndef SRC_ACTORS_ACTOR_342400_ACTOR_342400_PRIVATE_H
#define SRC_ACTORS_ACTOR_342400_ACTOR_342400_PRIVATE_H

#include "types.h"

#include "gameplay/enemy_params.h"

#include "main/tmd_types.h"

extern TmdSource gMadChaserChunkModel0;

extern TmdSource gMadChaserChunkModel1;

extern TmdSource gMadChaserChunkModel2;

extern TmdSource gActor342400MadChaserBody;

extern EnemyParams gMadChaserEnemyParams;

/// Next sequential placement index assigned to an incinerator encounter enemy.
///
/// Reset to zero when the encounter starts, then incremented for each successful
/// Mad Chaser, Slouch or Sucklerceph spawn, never on death. The 17 rows can assign
/// 23 identities. Packing into `Enemy::placeKey` retains the low four index bits;
/// this u16 counter is not the number of live enemies.
extern u16 gActor342400EncounterNextPlaceIndex;

#endif // SRC_ACTORS_ACTOR_342400_ACTOR_342400_PRIVATE_H
