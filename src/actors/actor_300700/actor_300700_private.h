#ifndef SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H
#define SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/collision.h"
#include "gameplay/effects.h"
#include "gameplay/enemy.h"
#include "gameplay/geometry.h"
#include "gameplay/enemy_params.h"

#include "main/coord.h"
#include "main/session_types.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

#include "../../shared/creatures/rat/rat.h"

extern TmdSource gRatAndMothRatBody;

/// Bite attack shared by the rat parameters and the briefly enabled attack sphere.
///
/// Base power 6; the poison reaction is packed into the category-4 contact key.
extern DamageAttack gRatAttack;

/// Shared rat health, battle rewards and hit-reaction tuning, borrowed by live enemies.
extern EnemyParams gRatParams;

/// Rat clips indexed by `RAT_ANIM_*`: entry 0 empty, entries 1..10 drive slots 1..6.
///
/// The animation context borrows the table and its package-local clip data
/// until the rat's task is destroyed.
extern AnimationSet* gRatAnimSets[RAT_ANIM_SET_COUNT];

#endif // SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H
