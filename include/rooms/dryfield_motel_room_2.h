#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_motel_room_2_80180410[12];

// dryfield_motel_room_2
extern u8* D_dryfield_motel_room_2_8017D6E4[];

extern WorldCollisionRoomResources D_dryfield_motel_room_2_8017D6E8[];

extern ViewCount D_dryfield_motel_room_2_8017D6F8[];

extern WorldCoordRoomLighting D_dryfield_motel_room_2_8017D6FC[];

extern DirectionWarpEntry D_dryfield_motel_room_2_8017D704[];

extern ViewCamera D_dryfield_motel_room_2_8017DE30[];

extern SpriteView D_dryfield_motel_room_2_8017FCD0[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_room_2_801804B0[];

/// Inert room-effect callback for bank 6, slot 0xCA, selected for daytime Motel Room 2.
///
/// Ignores the task without drawing, advancing state or releasing resources.
/// Keep the Dryfield Motel Room 2 overlay loaded while this callback is scheduled.
void dryfieldMotelRoom2EffectNoopTaskCA(Task* unusedTask);

/// Runs the motel room 2's room-message receiver.
///
/// Start the borrowed task in state 0 to install this overlay's message table
/// and register `GAME_TASK_SLOT_ROOM`; state 1 idles and state 2 releases it.
/// The state index must be 0..2; dispatch is unchecked. Keep this overlay and
/// the registered task live while messages can arrive. Registration borrows
/// the task pointer and does not clear the slot on teardown.
void dryfieldMotelRoom2Task(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_ROOM_2_H
