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

#endif // SRC_ACTORS_ACTOR_300700_ACTOR_300700_PRIVATE_H
