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

/// Binds both Mad Chaser spawn paths to this overlay's shared parameter object.
///
/// Include this header before `mad_chaser.h` and retain the binding through both
/// spawn fragments. This stable `EnemyParams` lvalue selects the record defined
/// in the data translation unit and borrowed by enemies in the behavior unit.
#define MAD_CHASER_ENEMY_PARAMS (gActor342400MadChaserEnemyParams)

/// Detached Mad Chaser head thrown from body part 6 during a burst death.
///
/// One rigid part, with 62 textured triangles and 15 textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
extern TmdSource gChaserMadBurstHead;

/// Selects this overlay's detached-head source for Mad Chaser burst effects.
///
/// This stable, writable `TmdSource` lvalue names the data unit's object borrowed
/// by chunks spawned in the behavior unit. Include before `mad_chaser.h` to
/// suppress its TU-local default, and retain through `mad_chaser_spawn_gibs.inc.c`.
/// The source, geometry and writable stream must outlive every borrowing model.
#define MAD_CHASER_BURST_HEAD_SOURCE (gChaserMadBurstHead)

/// Detached Mad Chaser arm thrown from body part 8 during a burst death.
///
/// One rigid part, with 34 textured triangles and nine textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
extern TmdSource gChaserMadBurstArm;

/// Selects this overlay's detached-arm source for Mad Chaser burst effects.
///
/// This writable `TmdSource` lvalue names the data unit's object borrowed by
/// chunks spawned in the behavior unit. Include before `mad_chaser.h` to
/// suppress its TU-local default, and retain through `mad_chaser_spawn_gibs.inc.c`.
/// The source, geometry and stream must outlive every borrowing chunk model.
#define MAD_CHASER_BURST_ARM_SOURCE (gChaserMadBurstArm)

/// Detached Mad Chaser tail thrown from body part 2 during a burst death.
///
/// One rigid part, with 18 textured triangles and 14 textured quads in each
/// primitive-buffer half; there is no pre-transformed packet region. Instances
/// borrow the writable geometry and resolved stream for this overlay's lifetime
/// and inherit texture placement from the body after spawning.
extern TmdSource gChaserMadBurstTail;

/// Selects this overlay's detached-tail source for Mad Chaser burst effects.
///
/// This stable, writable `TmdSource` lvalue names the data unit's object borrowed
/// by chunks spawned in the behavior unit. Include before `mad_chaser.h` to
/// suppress its TU-local default, and retain through `mad_chaser_spawn_gibs.inc.c`.
/// The source, geometry and writable stream must outlive every borrowing model.
#define MAD_CHASER_BURST_TAIL_SOURCE (gChaserMadBurstTail)

extern TmdSource gChaserMadBody;

/// Next sequential placement index assigned to an incinerator encounter enemy.
///
/// Reset to zero when the encounter starts, then incremented for each successful
/// Mad Chaser, Slouch or Sucklerceph spawn, never on death. The 17 rows can assign
/// 23 identities. Packing into `Enemy::placeKey` retains the low four index bits;
/// this u16 counter is not the number of live enemies.
extern u16 gActor342400EncounterNextPlaceIndex;

#endif // SRC_ACTORS_ACTOR_342400_ACTOR_342400_PRIVATE_H
