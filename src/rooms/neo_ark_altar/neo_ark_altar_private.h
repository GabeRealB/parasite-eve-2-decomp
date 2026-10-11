#ifndef SRC_ROOMS_NEO_ARK_ALTAR_NEO_ARK_ALTAR_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_ALTAR_NEO_ARK_ALTAR_PRIVATE_H

#include "types.h"

#include "gameplay/area_flags.h"

#include "main/task_types.h"

/// Two bodyless priority-192 recipes for altar movie launch and playback.
///
/// Entry 0 transfers presentation to entry 1 on the display-task list.
/// spawnArg1 selects movie 0 (set switch), 1 (clear switch) or 2 (second tile
/// sequence); spawnArg2 is unused. No terminator; keep movie resources and
/// room code loaded until playback restores game presentation.
extern TaskDesc gNeoArkAltarMovieTaskDescs[2];

extern TaskDesc D_neo_ark_altar_8017F088[1];

extern AreaApplyRec D_neo_ark_altar_801800A0[3];

/// Advances the altar's switch sprites one animation step towards the saved choice.
///
/// The low byte of `switchChoice` is 0 for the clear choice (towards step 6),
/// 1 for set (towards step 0); other values do nothing. Valid shared steps are
/// 0..6. At either endpoint the side-view scenery is still updated, without
/// changing the animation frame. Each actual step selects batch 6 minus the
/// lower of the old/new steps in sprite view index 4, leaving batch 0 intact.
/// Requires the current altar area's loaded map directory and writable sprite
/// views at indices 3, 4 and 6, with batches 1..6 present in view index 4.
void neoArkAltarStepSwitchSprites(s32 switchChoice);

#endif // SRC_ROOMS_NEO_ARK_ALTAR_NEO_ARK_ALTAR_PRIVATE_H
