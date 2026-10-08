#ifndef INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
#define INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_observatory_80180DBC[2];

extern AreaVariant D_neo_ark_observatory_8018786C[13];

// neo_ark_observatory
extern WorldCollisionRoomResources D_neo_ark_observatory_80181594[];

extern WorldCoordRoomLighting D_neo_ark_observatory_801815B4[];

extern u8* D_neo_ark_observatory_801815DC[];

extern ViewCount D_neo_ark_observatory_801815E4[];

extern DirectionWarpEntry D_neo_ark_observatory_801815E8[];

extern ViewCamera D_neo_ark_observatory_80181FC8[];

extern SpriteView D_neo_ark_observatory_801860E8[];

extern WorldCollisionSurfaceProperties* D_neo_ark_observatory_80187A08[];

/// Sets the base grey intensity of the observatory's light beams.
///
/// Stores the signed low halfword without clamping; room entry and scenes use
/// 0..160. The drawer adds a four-frame pulse of -4..4 and skips negative
/// results. This changes beams only; the view's glow discs retain their colours.
/// The room overlay must be loaded. The glow task resets the value on its first tick.
void neoArkObservatorySetLightBeamIntensity(s32 intensity);

/// Updates the companion obstacle reserved at the start of the room collision grid.
///
/// Uses the companion's model root, falling back to the player's. At least one
/// must be a live model task with a root mapping local geometry into room space.
/// Applies the room-axis offset (0, 0, -200) while a companion exists and
/// `GAME_FLAG_0D7` is nonzero; otherwise raises it by 10000 room units. The grid's
/// cell lists and remaining geometry stay intact. Requires the observatory
/// overlay; changes GTE state. The event-script argument is unused.
void neoArkObservatoryUpdateCompanionObstacle(s32 unusedEventArg);

/// Draws the observatory's glow discs and light beams for the mapped camera view.
///
/// Effect-table slot 0x14E resets beam intensity once in state 0, then draws
/// each tick in state 1. Requires the room overlay, composed view matrices,
/// initialized scratch stack, and the current frame's packet arena and ordering
/// table. It retains no pointers into those frame resources.
void neoArkObservatoryGlowTask(Task* effectTask);

/// Runs the observatory's room-message task and companion presentation.
///
/// Requires a live task in state 0..2: initialize the receiver, companion
/// obstacle and light beam; update companion visibility each tick; then
/// teardown. The room overlay must remain loaded through dispatch.
void neoArkObservatoryRoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
