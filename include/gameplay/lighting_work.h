#ifndef GAMEPLAY_LIGHTING_WORK_H
#define GAMEPLAY_LIGHTING_WORK_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

/// Scratch-stack workspace for blending an actor's light-colour matrix.
///
/// Reserved for one colour-mode update and released before return. While the
/// blend countdown is positive, `previousColor` holds the light-colour 3x3
/// remapped to the previous colour mode and the live matrix holds the current
/// mode; each light column is then replaced by a weighted average of the two.
/// The matrix translation, the ambient colour, is neither copied nor blended,
/// and is left uninitialized in `previousColor`. The fields are left
/// untouched once the countdown has expired.
///
/// `currentColumn` and `previousColumn` pack one column for that average.
/// x is red, y is green and z is blue: the three rows of the column. They
/// are `SVECTOR`s so the average can load and store them through the GTE
/// short-vector operations; the fourth word of each is unused. Channel
/// values stay on the light-colour scale, 12 fractional bits, where `ONE`
/// is full strength. The blended column is written back through
/// `currentColumn`. The weights use the same 12-bit fraction: the previous
/// mode's weight is the countdown shifted up by eight, and the current
/// mode's weight is what remains of `ONE`.
///
/// Reserve one complete block and release it in scratch-stack order.
/// Initialize only the words the blend reads. A pointer into the block
/// must not be used after its release.
typedef struct {
    MATRIX  previousColor;  // Previous colour mode's 3x3; translation left uninitialized
    SVECTOR currentColumn;  // Current-mode column, then the blended column (x red, y green, z blue)
    SVECTOR previousColumn; // Previous-mode column packed the same way
} WorldCoordActorColorScratch;
STATIC_ASSERT_SIZEOF(WorldCoordActorColorScratch, 0x30);

#endif // GAMEPLAY_LIGHTING_WORK_H
