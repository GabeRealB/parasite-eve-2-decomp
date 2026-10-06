#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_2_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_2_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_2_801809A0[22];

// dryfield_night_motel_room_2
extern WorldCoordRoomLighting D_dryfield_night_motel_room_2_8017DA5C[];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_2_8017DA64[];

extern u8* D_dryfield_night_motel_room_2_8017DA74[];

extern ViewCount D_dryfield_night_motel_room_2_8017DA78[];

extern DirectionWarpEntry D_dryfield_night_motel_room_2_8017DA7C[];

extern ViewCamera D_dryfield_night_motel_room_2_8017E1A8[];

extern SpriteView D_dryfield_night_motel_room_2_80180110[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_room_2_80180A90[];

/// Draws night motel room 2's flickering textured flares for the current view.
///
/// Room-local view slots 2 and 3 draw two distinct world points; slots 5 and 6
/// draw one other point. Other slots draw nothing. The room overlay must remain
/// loaded, with the view matrix, scratch stack, packet arena and ordering table
/// ready for the frame. Reserves two flare packets or one, even if depth clipping
/// rejects them. `unusedTask` is ignored; no task state or work is accessed.
void dryfieldNightMotelRoom2DrawFlaresTask(Task* unusedTask);

void func_dryfield_night_motel_room_2_8017D6BC(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_2_H
