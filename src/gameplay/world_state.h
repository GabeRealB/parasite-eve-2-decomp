#ifndef GAMEPLAY_PRIVATE_WORLD_STATE_H
#define GAMEPLAY_PRIVATE_WORLD_STATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// GTE screen coordinates, written as one word and read as two halfwords.
typedef union GpLockScreenPos {
    DVECTOR xy;
    s32     packed;
} GpLockScreenPos;
STATIC_ASSERT_SIZEOF(GpLockScreenPos, 4);

#endif // GAMEPLAY_PRIVATE_WORLD_STATE_H
