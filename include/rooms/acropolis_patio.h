#ifndef INCLUDE_ROOMS_ACROPOLIS_PATIO_H
#define INCLUDE_ROOMS_ACROPOLIS_PATIO_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_patio_80184A90[12];

// acropolis_patio
extern WorldCollisionRoomResources D_acropolis_patio_80182E68[];

extern u8* D_acropolis_patio_80182EC0[];

extern ViewCount D_acropolis_patio_80182ECC[];

extern WorldCoordRoomLighting D_acropolis_patio_80182ED4[];

extern DirectionWarpEntry D_acropolis_patio_80182EEC[];

extern SpriteView D_acropolis_patio_80186360[];

extern ViewCamera D_acropolis_patio_80186D5C[];

extern WorldCollisionSurfaceProperties* D_acropolis_patio_8018703C[];

void func_acropolis_patio_8017E100(Task* task);

/// Draws a view-gated, flickering additive sprite for one patio fountain anchor.
///
/// `task` requires a coordinate body and a live `EffectWork` in `spawnArg2.pointer`.
/// Start in state 0. `spawnArg1.value` packs anchor 0..13 in bits 0..3, texture
/// cell 0..2 in bits 8..9, and size in bits 16..27 (zero selects 640). The
/// screen half extent is size * 39 / (SZ3 / 4). Initialization retains only
/// the anchor word and stores size/cell/resting grey in `scale`/`angle`/`period`.
/// Camera views are 1-based. Cancellation or an invisible view suspends the
/// task without freeing it. Requires the patio and gameplay overlays, scratch
/// space and the frame's GPU packet arena; no scratch pointer is retained.
void acropolisPatioFountainJetTask(Task* task);

/// Moves and draws one shimmering grey point of the patio fountain's mist.
///
/// Start in state 0 with a coordinate body, zeroed `EffectWork` in
/// `spawnArg2.pointer`, and an anchor index 0..2 in `spawnArg1.value`. `move`
/// stores signed per-tick velocity; `index` selects drift (0) or gather (1).
/// Gather aims toward the anchor's numeric coordinates in the effect's view
/// parent frame. Drawing uses the composed position cached before this tick's
/// movement, narrows it to s16, and queues an average-blended 1x1 tile at
/// SZ3 / 4 >= `EFFECT_POINT_TILE_MIN_DEPTH`. Invisible views and cancellation
/// suspend motion and drawing without freeing the task. Requires the patio
/// and gameplay overlays, scratch space and the frame's GPU packet arena.
void acropolisPatioFountainMistTask(Task* task);

/// Runs the patio's room-message receiver and initializes its story interactions.
///
/// State 0 registers the receiver and configures actors and scenes for the
/// current story progress; state 1 idles; state 2 releases the task.
/// Start with a live bodyless task in state 0. The state must remain in 0..2;
/// dispatch performs no bounds check. Keep the room overlay and gameplay
/// resources loaded through the selected handler, which may release the task.
void acropolisPatioRoomTask(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_PATIO_H
