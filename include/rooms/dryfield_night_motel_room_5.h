#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_5_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_5_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_5_80181190[13];

// dryfield_night_motel_room_5
extern WorldCoordRoomLighting D_dryfield_night_motel_room_5_8017DA70[];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_5_8017DA80[];

extern u8* D_dryfield_night_motel_room_5_8017DAAC[];

extern ViewCount D_dryfield_night_motel_room_5_8017DAB4[];

extern DirectionWarpEntry D_dryfield_night_motel_room_5_8017DAB8[];

extern ViewCamera D_dryfield_night_motel_room_5_8017E060[];

extern SpriteView D_dryfield_night_motel_room_5_80180994[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_room_5_80181230[];

/// Draws the night motel room 5 light flare selected by the logical camera view.
///
/// Effect-bank 6 callback for slot 0x118. Views 3/8, 2/7 and 4/9 select
/// separate world points; every other view draws nothing. The task is unused.
/// Requires the room overlay and flare texture to remain loaded, a composed
/// view matrix, scratch space and a current GPU packet arena and ordering table.
/// One selected flare reserves one packet even when rejected by near clipping;
/// queued geometry borrows the frame arena until GPU completion.
void dryfieldNightMotelRoom5DrawFlareTask(Task* unusedTask);

void func_dryfield_night_motel_room_5_8017D6D0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_5_H
