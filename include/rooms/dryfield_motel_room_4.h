#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_4_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_4_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// dryfield_motel_room_4
extern WorldCollisionRoomResources D_dryfield_motel_room_4_8017D6DC[];

extern u8* D_dryfield_motel_room_4_8017D6EC[];

extern ViewCount D_dryfield_motel_room_4_8017D6F0[];

extern WorldCoordRoomLighting D_dryfield_motel_room_4_8017D6F4[];

extern DirectionWarpEntry D_dryfield_motel_room_4_8017D6FC[];

extern ViewCamera D_dryfield_motel_room_4_8017DE14[];

extern SpriteView D_dryfield_motel_room_4_8017DF4C[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_room_4_8017E470[];

/// Runs Dryfield motel room 4's room-message task.
///
/// `task` must be live with state 0..2: 0 registers its message table and room
/// slot, then advances to 1; 1 idles; 2 releases the task. No body or work is
/// allocated by this callback. The room overlay must remain loaded while its
/// callback or message table is used, and room-slot users require a live task.
void dryfieldMotelRoom4Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_4_H
