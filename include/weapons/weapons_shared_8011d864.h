#ifndef WEAPONS_SHARED_8011D864_H
#define WEAPONS_SHARED_8011D864_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// 0x24-byte scratch block the muzzle-flash quad takes from `G_SCRATCH_HEAD`
/// for one muzzle-flash quad. `v` is built in muzzle-local space, rotated by
/// the muzzle coordinate's `workm` and translated by its `t`, then projected
/// through `GsWSMATRIX`; `otz` is the `gte_stszotz` of that projection, which
/// both rejects the quad when it is closer than 0x11 and picks the OT bucket
/// the `POLY_G4` is linked into.
typedef struct WeaponQuadScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ SVECTOR v[4];
} WeaponQuadScratch;
STATIC_ASSERT_SIZEOF(WeaponQuadScratch, 0x24);

#endif // WEAPONS_SHARED_8011D864_H
