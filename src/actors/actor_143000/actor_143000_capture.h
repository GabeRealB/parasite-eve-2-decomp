#ifndef ACTOR_143000_CAPTURE_H
#define ACTOR_143000_CAPTURE_H

#include "common.h"

/// Spawn argument of `func_actor_143000_80133CF0`, the task that captures
/// successive horizontal image strips.
typedef struct Actor143000CaptureArgs {
    /* 0x0 */ u16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ u16 w;
    /* 0x6 */ s16 h;
    /* 0x8 */ s32 total;
    /* 0xC */ s32 count;
} Actor143000CaptureArgs;
STATIC_ASSERT_SIZEOF(Actor143000CaptureArgs, 0x10);

extern Actor143000CaptureArgs D_actor_143000_80135090;
extern Actor143000CaptureArgs D_actor_143000_801350A0;

#endif
