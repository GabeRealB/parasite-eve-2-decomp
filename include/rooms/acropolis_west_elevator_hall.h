#ifndef ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H
#define ROOMS_ACROPOLIS_WEST_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>

/// 0x14 scratch block `func_acropolis_west_elevator_hall_8017FFE4` takes from
/// `G_SCRATCH_HEAD` to project one billboard sprite. `pos` is the effect
/// coordinate's `workm` translation truncated to s16 and fed to the `RTPS`,
/// `sx` / `sy` receive the projected centre and `otz` is `SZ3 >> 2`; anything
/// nearer than `otz == 0x11` is dropped instead of drawn. `radius` is the
/// perspective-divided half-extent `0x6700 / otz`, stored as a word but only
/// ever read back as its low halfword.
typedef struct AwehSpriteScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     radius;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ u16     sx;
    /* 0x12 */ u16     sy;
} AwehSpriteScratch;
STATIC_ASSERT_SIZEOF(AwehSpriteScratch, 0x14);

#endif
