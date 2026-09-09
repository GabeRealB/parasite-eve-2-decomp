#ifndef ROOMS_ACROPOLIS_ROOF_GARDEN_H
#define ROOMS_ACROPOLIS_ROOF_GARDEN_H

#include "common.h"

#include <psyq/libgte.h>

/// One grey level per sprite variant, indexed by the variant the roof garden's
/// sprite task picked out of `Task::spawnArg1` (bits 8..9). The overlay holds
/// one of these, `D_acropolis_roof_garden_8017D5D0`, and it is copied onto the
/// stack so the variant index can subscript it.
typedef struct RgSpriteLevels {
    /* 0x0 */ u8 v[3];
} RgSpriteLevels;

/// Projected flare centre, depth, and radii in the 0x18-byte scratch block.
typedef struct RgFlareScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     radius;
    /* 0x08 */ s32     inner;
    /* 0x0C */ SVECTOR vec;
    /* 0x14 */ u16     sx;
    /* 0x16 */ u16     sy;
} RgFlareScratch;

#endif // ROOMS_ACROPOLIS_ROOF_GARDEN_H
