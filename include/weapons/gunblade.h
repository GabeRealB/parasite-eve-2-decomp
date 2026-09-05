#ifndef WEAPONS_GUNBLADE_H
#define WEAPONS_GUNBLADE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3FB8.h"
#include "main/task.h"

/// Translation of the gunblade beam's two coordinate frames inside the muzzle
/// frame: `[0]` is the near end (`0, 0x60, 0x80`) and `[1]` the far end
/// (`0, 0x60, 0x380`). `func_gunblade_8011D1E4` seeds the task's own coord
/// from `[0]` and the stack coord from `[1]`.
///
/// `D_gunblade_8011E70C` is that same far-end vector under its own name. State
/// 0 reaches it as `D_gunblade_8011E704[1]`, so the address is derived from the
/// array base already in a register; state 1 loads the symbol on its own. Both
/// spellings are needed to match.
extern SVECTOR D_gunblade_8011E704[2];
extern SVECTOR D_gunblade_8011E70C;

/// The eight-segment beam trails, one array per end of the blade. Every entry
/// is parented to `Gfx_ViewCoord`.
extern GsCOORDINATE2 D_gunblade_8012E254[8];
extern GsCOORDINATE2 D_gunblade_8012E4D4[8];

/// The running beam task and its `GpEffWork`, cached on entry to state 0 so
/// `func_gunblade_8011E008` can reach them from outside the task. The work
/// pointer is cleared again when the `Gp_State1C` block is released.
extern Task*      D_gunblade_8012E244;
extern GpEffWork* D_gunblade_8012E248;

/// Draws one segment of the beam: `slot` is the index into the trail arrays,
/// `flags` the primitive/blend selector.
void func_gunblade_8011D70C(s32 slot, s32 flags);

void func_gunblade_8011E008(s32 arg0);

#endif
