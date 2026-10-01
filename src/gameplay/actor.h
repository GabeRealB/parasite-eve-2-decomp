#ifndef GAMEPLAY_PRIVATE_ACTOR_H
#define GAMEPLAY_PRIVATE_ACTOR_H

#include "common.h"

/// Companion spawn record for `Gp_SpawnPlayer`. `field_0` is copied to
/// `GameActor.actionArgument`. Nonzero `field_2` sets `GameActor.mode` to 2.
typedef struct _GpActorFlags {
    /* 0x0 */ u16  field_0;
    /* 0x2 */ u8   field_2;
    /* 0x3 */ byte pad_3;
} GpActorFlags;
STATIC_ASSERT_SIZEOF(GpActorFlags, 0x4);

#endif // GAMEPLAY_PRIVATE_ACTOR_H
