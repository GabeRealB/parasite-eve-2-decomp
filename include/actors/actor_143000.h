#ifndef ACTOR_143000_H
#define ACTOR_143000_H

#include "common.h"

/// Spawn argument the actor hands to the task it starts once the code is
/// entered (`D_actor_143000_80135C08`); that task sets `field_1` when it starts.
typedef struct Actor143000Spawn {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} Actor143000Spawn;
STATIC_ASSERT_SIZEOF(Actor143000Spawn, 4);

extern Actor143000Spawn D_actor_143000_80135C08;

#endif
