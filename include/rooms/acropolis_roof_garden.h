#ifndef ROOMS_ACROPOLIS_ROOF_GARDEN_H
#define ROOMS_ACROPOLIS_ROOF_GARDEN_H

#include "common.h"

/// One grey level per sprite variant, indexed by the variant the roof garden's
/// sprite task picked out of `Task::spawnArg1` (bits 8..9). The overlay holds
/// one of these, `D_acropolis_roof_garden_8017D5D0`, and it is copied onto the
/// stack so the variant index can subscript it.
typedef struct RgSpriteLevels {
    /* 0x0 */ u8 v[3];
} RgSpriteLevels;

#endif // ROOMS_ACROPOLIS_ROOF_GARDEN_H
