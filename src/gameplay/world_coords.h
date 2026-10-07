#ifndef GAMEPLAY_PRIVATE_WORLD_COORDS_H
#define GAMEPLAY_PRIVATE_WORLD_COORDS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/room.h"

/// Stage-to-area-to-room model-lighting lookup tables borrowed from loaded overlays.
///
/// Index with `GameLocationKey.stage - 1` (stages 1..5: Akropolis, Dryfield day,
/// Dryfield night, Mine/Shelter, Shelter/Neo Ark), then `area - 1`, then `room - 1`.
/// The stage area counts are 20, 38, 38, 49 and 33; each area fixes its room
/// extent. Stage and area pointers may be NULL, yielding no descriptor. Other
/// indices must be valid: the lookup performs no range checks.
/// Gameplay owns these five pointers. The selected map overlay must be loaded
/// for its tables/local descriptors; room-light records and arrays require the
/// corresponding room overlay and must not survive its unload.
extern WorldCoordRoomLighting** gWorldCoordRoomLightingTables[5];

struct WorldTargetNode;

/// Current `gWorldTargetListHead` node whose lock-on reticle `worldTargetDrawOverlay` is
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
