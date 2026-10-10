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

/// Lighting for the two room variants of night motel room 5.
///
/// Indexed by the 1-based room minus one. Both entries borrow the same four
/// point lights and one cone light; NULL ambient tables select the default
/// ambient minima. The mutable lights remain valid while this overlay is loaded.
extern WorldCoordRoomLighting gDryfieldNightMotelRoom5RoomLighting[2];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_5_8017DA80[];

/// Logical-view mappings for the two room variants of night motel room 5.
///
/// Indexed by room minus one, then logical view minus one, for views 1..11.
/// Room 1 uses the gameplay identity map; room 2 uses 1, 7..11, 2..6. Values
/// select 1-based camera, image and sprite slots. Keep gameplay and this room
/// overlay loaded while borrowing the maps; neither map has a terminator.
extern u8* gDryfieldNightMotelRoom5RoomViewMaps[2];

extern ViewCount D_dryfield_night_motel_room_5_8017DAB4[];

extern DirectionWarpEntry D_dryfield_night_motel_room_5_8017DAB8[];

extern ViewCamera D_dryfield_night_motel_room_5_8017E060[];

extern SpriteView D_dryfield_night_motel_room_5_80180994[];

/// Collision policies and footstep cues indexed by room-local surface class 0..7.
///
/// Classes 0 and 4..7 share silent blocking properties; classes 1 and 2 add
/// distinct footstep cues. Class 3 passes probes and ignores weapon impacts.
/// Every class applies pushback. Records and cues borrow this overlay's lifetime.
extern WorldCollisionSurfaceProperties* gDryfieldNightMotelRoom5SurfacePropertiesByClass[8];

/// Draws the night motel room 5 light flare selected by the logical camera view.
///
/// Effect-bank 6 callback for slot 0x118. Views 3/8, 2/7 and 4/9 select
/// separate world points; every other view draws nothing. The task is unused.
/// Requires the room overlay and flare texture to remain loaded, a composed
/// view matrix, scratch space and a current GPU packet arena and ordering table.
/// One selected flare reserves one packet even when rejected by near clipping;
/// queued geometry borrows the frame arena until GPU completion.
void dryfieldNightMotelRoom5DrawFlareTask(Task* unusedTask);

/// Runs night motel room 5's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldNightMotelRoom5RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_5_H
