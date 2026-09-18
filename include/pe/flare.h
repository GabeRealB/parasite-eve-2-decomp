#ifndef PE_FLARE_H
#define PE_FLARE_H

#include "common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// Working space for projecting one spark and building its quad.
///
/// Taken from the scratchpad for the duration of a single draw. The same shape
/// as the gameplay `GpFxQuadScratch`; whether the two are one type is an open
/// question.
typedef struct FlareQuadScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ s32     otz;
    /* 0x0C */ s32     flag;
    /* 0x10 */ s32     dx;
    /* 0x14 */ s32     dy;
    /* 0x18 */ s16     sx;
    /* 0x1A */ s16     sy;
} FlareQuadScratch;
STATIC_ASSERT_SIZEOF(FlareQuadScratch, 0x1C);

/// Draws one frame of a spark's sprite at a world position.
///
/// The position is projected to the screen and the quad dropped if it lands
/// behind the camera. `arg1` picks one of the eight texture frames, `arg3`
/// spins the quad and `arg2` sizes it, with the size divided by depth so the
/// sprite shrinks into the distance.
void flareDrawSparkQuad(GsCOORDINATE2* arg0, u16 arg1, s16 arg2, s16 arg3);

#endif /* PE_FLARE_H */
