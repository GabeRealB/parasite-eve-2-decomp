#ifndef GAMEPLAY_PRIVATE_WORLD_COORDS_H
#define GAMEPLAY_PRIVATE_WORLD_COORDS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

struct WorldTargetNode;

/// Current `gWorldTargetListHead` node whose lock-on reticle `Gp_DrawTargetCursor` is
/// drawing. Cleared when the walk finds no live target.
extern struct WorldTargetNode* D_80115260;

/// Lerp / settle counter for that reticle. `< 5` eases `D_8010F9EC` /
/// `D_8010F9F0` toward the projected coords (small sprite); `0xFF` snaps.
/// Reset to `0` on target change and when the list is empty.
extern s32 D_80115264;

/// Copies RGB multipliers for the model light-colour matrix, or disables the override.
///
/// `colorScales` supplies an entire readable SVECTOR: vx/vy/vz hold raw
/// unsigned Q12 bits for red/green/blue (zero suppresses, ONE is unity). The
/// full eight-byte value is copied, including its unused final halfword; no
/// caller pointer is retained. NULL disables use of the stored value. Scaling
/// affects the three colour rows and preserves the ambient translation.
void worldCoordSetLightColorScaleOverride(const SVECTOR* colorScales);

#endif // GAMEPLAY_PRIVATE_WORLD_COORDS_H
