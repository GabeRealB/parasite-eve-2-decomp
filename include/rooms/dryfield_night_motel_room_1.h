#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_1_8018075C[22];

// dryfield_night_motel_room_1
extern WorldCoordRoomLighting D_dryfield_night_motel_room_1_8017DA64[];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_1_8017DA74[];

extern u8* D_dryfield_night_motel_room_1_8017DAA0[];

extern ViewCount D_dryfield_night_motel_room_1_8017DAA4[];

extern DirectionWarpEntry D_dryfield_night_motel_room_1_8017DAA8[];

extern ViewCamera D_dryfield_night_motel_room_1_8017E0BC[];

extern SpriteView D_dryfield_night_motel_room_1_8017FE98[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_room_1_80180844[];

/// Draws night motel room 1's flickering light flares for the mapped camera.
///
/// Camera indices 2, 3, 8 and 9 select the flare with the larger radius scale;
/// 5 and 6 select the smaller one. Other indices draw nothing. Gameplay dispatches
/// this callback through effect-bank slot 0x107; `task` is unused.
/// The room overlay and view map must remain loaded, with the current view
/// matrices, scratch stack, packet arena and depth ordering table ready for
/// drawing. A selected flare reserves one packet even when depth-clipped.
void dryfieldNightMotelRoom1DrawGlowsTask(Task* task);

/// Runs night motel room 1's room-message task for one update.
///
/// Requires a live task: state 0 initializes, 1 idles and 2 releases the task.
/// The state index is unchecked. Keep this room overlay loaded while the task
/// or its registered message handlers can run; the final state ends its lifetime.
void dryfieldNightMotelRoom1RoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_1_H
