#ifndef INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H
#define INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_1f_airlock_8017F758[12];

// shelter_1f_airlock
extern WorldCollisionRoomResources D_shelter_1f_airlock_8017E59C[];

extern WorldCoordRoomLighting D_shelter_1f_airlock_8017E5AC[];

extern u8* D_shelter_1f_airlock_8017E5B4[];

extern ViewCount D_shelter_1f_airlock_8017E5B8[];

extern DirectionWarpEntry D_shelter_1f_airlock_8017E5BC[];

extern ViewCamera D_shelter_1f_airlock_8017E85C[];

extern SpriteView D_shelter_1f_airlock_8017F07C[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_airlock_8017F84C[];

/// Runs the Airlock room controller for synchronous room messages.
///
/// Requires a live task with state 0 (register handlers), 1 (idle), or 2 (kill).
/// Keep the room and Neo Ark map overlays loaded while its task and borrowed
/// message table remain available.
void shelter1fAirlockRoomTask(Task* task);

/// Draws the airlock's flickering disc and capsule glows for the current mapped view.
///
/// Gameplay's room-effect task slot 0x14D calls this once per update. Views 3
/// and 4 draw grey discs and cyan capsules; view 5 draws a red disc. Other
/// views emit nothing. The task argument is unused, and the task remains live.
/// Requires this room overlay, composed view matrices, an initialized scratch
/// stack and space in the current frame's packet arena and depth ordering table.
/// Emits at most 80 Gouraud quads plus their additive blend-mode packets.
/// Packets remain borrowed from that arena until GPU completion.
void shelter1fAirlockDrawViewGlowsTask(Task* unusedTask);

#endif // INCLUDE_ROOMS_SHELTER_1F_AIRLOCK_H
