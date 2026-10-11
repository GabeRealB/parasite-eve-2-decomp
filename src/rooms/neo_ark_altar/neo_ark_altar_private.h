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

/// Bodyless priority-32 recipe for the altar tile-sequence controller.
///
/// Address at index 0 with zero spawn arguments. Spawning copies this single
/// descriptor, with no terminator, and creates a task owning its puzzle work.
/// Keep the room's callbacks, tile and movie resources loaded through teardown.
extern TaskDesc gNeoArkAltarTileSequenceTaskDesc;

/// First accepted altar-switch updates for Power Plant 1 and the Savanna Zone.
///
/// Two updates plus AREA_APPLY_END, applied only before Power Plant 2 is cleared.
/// In every save mode they set layouts 2/3 and map marks, discarding saved enemy
/// poses. The synchronous apply call borrows these room-owned records.
extern const AreaApplyRec gNeoArkAltarFirstSwitchAreaUpdates[3];

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
