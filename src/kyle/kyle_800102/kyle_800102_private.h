#ifndef SRC_KYLE_KYLE_800102_KYLE_800102_PRIVATE_H
#define SRC_KYLE_KYLE_800102_KYLE_800102_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

/// Launch offset per attachment index, in the muzzle coordinate's local space.
extern SVECTOR D_kyle_800102_80177424[2];

/// Launch speed per attachment index, shifted left 16 into `field_88`.
extern u8 D_kyle_800102_8017743C[4];

/// Impact clip id per attachment, indexed by `sfx - 0xA`.
extern u16 D_kyle_800102_80177434[4];

#endif // SRC_KYLE_KYLE_800102_KYLE_800102_PRIVATE_H
