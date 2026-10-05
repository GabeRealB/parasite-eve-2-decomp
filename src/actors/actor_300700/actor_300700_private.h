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

extern TmdSource gActor300700RatBody;

extern DamageAttack gRatAttack;

extern EnemyParams gRatParams;

extern AnimationSet* gRatAnimSets[11];

/// Second variant's spawn: allocates its 0x39C-byte work block, binds the two
/// pose matrices into the TMD object, then hangs the four render nodes on
/// their global lists with the record tables `worldCollisionInitContacts` zeroes.

#endif // SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H
