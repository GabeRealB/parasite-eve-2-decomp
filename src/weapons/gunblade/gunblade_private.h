#ifndef SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H
#define SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H

#include "types.h"

#include "gameplay/effects.h"

#include "main/coord.h"
#include "main/task_types.h"

/// The eight-segment beam trails, one array per end of the blade. Every entry
/// is parented to `gGfxViewCoord`.
extern GfxCoord D_gunblade_8012E254[8];

extern GfxCoord D_gunblade_8012E4D4[8];

/// The running beam task and its `GpEffWork`, cached on entry to state 0 so
/// `func_gunblade_8011E008` can reach them from outside the task. The work
/// pointer is cleared again when the `Gp_State1C` block is released.
extern Task* D_gunblade_8012E244;

extern GpEffWork* D_gunblade_8012E248;

void func_gunblade_8011E008(s32 arg0);

#endif // SRC_WEAPONS_GUNBLADE_GUNBLADE_PRIVATE_H
