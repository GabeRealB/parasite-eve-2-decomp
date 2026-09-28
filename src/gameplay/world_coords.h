#ifndef GAMEPLAY_PRIVATE_WORLD_COORDS_H
#define GAMEPLAY_PRIVATE_WORLD_COORDS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

struct GpLinkNode;

/// Current `Gp_LinkList` node whose lock-on reticle `Gp_DrawTargetCursor` is
/// drawing. Cleared when the walk finds no live target.
extern struct GpLinkNode* D_80115260;

/// Lerp / settle counter for that reticle. `< 5` eases `D_8010F9EC` /
/// `D_8010F9F0` toward the projected coords (small sprite); `0xFF` snaps.
/// Reset to `0` on target change and when the list is empty.
extern s32 D_80115264;

void Gp_SetOverrideVec2(SVECTOR* arg0);

#endif // GAMEPLAY_PRIVATE_WORLD_COORDS_H
