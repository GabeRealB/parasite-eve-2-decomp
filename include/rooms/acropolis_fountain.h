#ifndef INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
#define INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_acropolis_fountain_8017FC9C[11];

// acropolis_fountain
extern WorldCollisionRoomResources D_acropolis_fountain_8017E814[];

extern u8* D_acropolis_fountain_8017E84C[];

extern ViewCount D_acropolis_fountain_8017E854[];

extern WorldCoordRoomLighting D_acropolis_fountain_8017E858[];

extern DirectionWarpEntry D_acropolis_fountain_8017E868[];

extern SpriteView D_acropolis_fountain_8018375C[];

extern ViewCamera D_acropolis_fountain_80183864[];

extern WorldCollisionSurfaceProperties* D_acropolis_fountain_80183B90[];

/// Draws the fountain spray as a flickering, semitransparent textured quad.
///
/// Requires a live coordinate body, the room's texture and palette, and space
/// for one `POLY_FT4` in the frame arena. Draws in raw camera views 7, 8, 15 and
/// 21 while room effects are active. Reserves and releases scratch storage and
/// consumes a packet even when projected depth (SZ3 / 4) is below 17.
void acropolisFountainSprayTask(Task* task);

void func_acropolis_fountain_8017DA78(s32 unused0, s32 unused1);

void func_acropolis_fountain_8017E014(Task* task);

/// Runs the player's turn, one-step ascent and walk to the room's fixed destination.
///
/// Spawned from task bank 2, slot 14 with state zero. The player task must remain
/// live through the six phases (0 turn, 1 await turn, 2 ascend, 3 await ascent,
/// 4 walk, 5 await arrival). Completion restores player control and kills this
/// task. Its state is an unchecked index maintained only by these callbacks.
void acropolisFountainClimbTask(Task* task);

void func_acropolis_fountain_8017D9C4(Task* task);

#endif // INCLUDE_ROOMS_ACROPOLIS_FOUNTAIN_H
