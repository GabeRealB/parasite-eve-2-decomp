#ifndef WEAPONS_M249_H
#define WEAPONS_M249_H

#include "common.h"

#include <psyq/libgs.h>

/// 0x68-byte scratch block `func_m249_8011D1DC` takes from `G_SCRATCH_HEAD`.
/// Only the trailing coordinate is used: `Gp_PickNearestRec18` writes the
/// chosen impact point into its `workm.t`, and that same coordinate is then
/// handed to `Gp_PlayObjSfx` as the sound source.
typedef struct _M249Scratch {
    /* 0x00 */ byte          pad_0[0x18];
    /* 0x18 */ GsCOORDINATE2 coord;
} M249Scratch;
STATIC_ASSERT_SIZEOF(M249Scratch, 0x68);

#endif
