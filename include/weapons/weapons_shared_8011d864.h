#ifndef WEAPONS_SHARED_8011D864_H
#define WEAPONS_SHARED_8011D864_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// 0x24-byte scratch block `WeaponsShared8011d864` takes from `G_SCRATCH_HEAD`
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

/// Draws a gun's muzzle flash as one Gouraud quad: three corners on a 0x100
/// circle around `arg1` (at `-0xC0`, `0`, `+0xC0`) and one tip 0x600 out and
/// 0x200 towards the camera, all in the muzzle coordinate's frame. `arg2` is
/// the flash brightness; only the corner along `arg1` is lit, with half of
/// `arg2` in red and green and all of it in blue.
void WeaponsShared8011d864(GsCOORDINATE2* arg0, s16 arg1, s16 arg2);

#endif // WEAPONS_SHARED_8011D864_H
