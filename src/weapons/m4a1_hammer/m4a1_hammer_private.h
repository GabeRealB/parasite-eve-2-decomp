#ifndef SRC_WEAPONS_M4A1_HAMMER_M4A1_HAMMER_PRIVATE_H
#define SRC_WEAPONS_M4A1_HAMMER_M4A1_HAMMER_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "main/task_types.h"

/// Parent task the hammer effect re-attaches itself to each time it restarts.
extern Task* D_m4a1_hammer_8012D660;

/// Offset vector handed to the `func_m4a1_hammer_8011E29C` sprite draw.
extern SVECTOR D_m4a1_hammer_8012D668;

/// Jitter table for the eight sparks the charged hammer throws: `[0..7]` are
/// the spin angles, `[8..15]` the heights and `[16..23]` the radii. Reseeded
/// from `gRandomLcgState` on the first charge frame and walked every other frame.
extern s16 D_m4a1_hammer_8012D630[24];

#endif // SRC_WEAPONS_M4A1_HAMMER_M4A1_HAMMER_PRIVATE_H
