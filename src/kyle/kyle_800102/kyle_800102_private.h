#ifndef SRC_KYLE_KYLE_800102_KYLE_800102_PRIVATE_H
#define SRC_KYLE_KYLE_800102_KYLE_800102_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

/// Launch offset per attachment index, in the muzzle coordinate's local space.
extern SVECTOR gGrenadeShellMuzzleOffsets[2];

/// Launch speed per attachment index, shifted left 16 into `field_88`.
extern u8 gGrenadeShellSpeeds[4];

/// Collision radius the shell takes on detonation, per attachment, indexed by
/// `sfx - 0xA`.
extern u16 gGrenadeShellBlastRadii[4];

#endif // SRC_KYLE_KYLE_800102_KYLE_800102_PRIVATE_H
