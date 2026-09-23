#ifndef WEAPONS_SHARED_8011D468_H
#define WEAPONS_SHARED_8011D468_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgs.h>

/// 0x18-byte scratch block the muzzle-flash core takes from `G_SCRATCH_HEAD`
/// for the muzzle flash's core sprite. `vec` is the muzzle coordinate's world
/// position (`workm.t[]`) truncated to s16 and projected by one `RTPS`; `sxy`
/// is the `gte_stsxy` of that projection and `otz` its `gte_stszotz`, which
/// both rejects the sprite when it is closer than 0x11 and picks the OT bucket
/// the `POLY_FT4` is linked into. `dx` / `dy` are the half-extents
/// `(size * 55 / otz) * rsin|rcos(angle) >> 12`, applied once at the flash's
/// own angle and once at 90 degrees to it to give the quad its four corners.
typedef struct _WeaponFlashCoreScratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     dx;
    /* 0x08 */ s32     dy;
    /* 0x0C */ SVECTOR vec;
    /* 0x14 */ DVECTOR sxy;
} WeaponFlashCoreScratch;
STATIC_ASSERT_SIZEOF(WeaponFlashCoreScratch, 0x18);

#endif // WEAPONS_SHARED_8011D468_H
