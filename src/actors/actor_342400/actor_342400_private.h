#ifndef SRC_ACTORS_ACTOR_342400_ACTOR_342400_PRIVATE_H
#define SRC_ACTORS_ACTOR_342400_ACTOR_342400_PRIVATE_H

#include "types.h"

#include "gameplay/enemy_params.h"

#include "main/tmd_types.h"

/// Mad Chaser tuning shared by this overlay's data and spawn translation units.
///
/// Spawned enemies borrow the record and its attack table for the overlay's
/// lifetime; ordinary and hidden setup seed current/maximum HP to 110.
/// Release rewards are 20 EXP, 40 BP and 1 MP.
extern EnemyParams gActor342400MadChaserEnemyParams;

/// Selects this overlay's cross-TU record for the included spawn fragments.
#define MAD_CHASER_ENEMY_PARAMS gActor342400MadChaserEnemyParams

extern TmdSource gMadChaserChunkModel0;

/// Detached Mad Chaser arm thrown from body part 8 during a burst death.
///
/// One rigid part, with 34 textured triangles and nine textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
extern TmdSource gMadChaserBurstArm;

/// Selects this overlay's cross-TU arm source for the included burst fragment.
#define MAD_CHASER_BURST_ARM_SOURCE gMadChaserBurstArm

extern TmdSource gMadChaserChunkModel2;

extern TmdSource gActor342400MadChaserBody;

/// Next sequential placement index assigned to an incinerator encounter enemy.
///
/// Reset to zero when the encounter starts, then incremented for each successful
/// Mad Chaser, Slouch or Sucklerceph spawn, never on death. The 17 rows can assign
/// 23 identities. Packing into `Enemy::placeKey` retains the low four index bits;
/// this u16 counter is not the number of live enemies.
extern u16 gActor342400EncounterNextPlaceIndex;

#endif // SRC_ACTORS_ACTOR_342400_ACTOR_342400_PRIVATE_H
