#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_3_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_3_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_room_3_80180D24[13];

// dryfield_night_motel_room_3
extern WorldCoordRoomLighting D_dryfield_night_motel_room_3_8017DA9C[];

extern WorldCollisionRoomResources D_dryfield_night_motel_room_3_8017DAA4[];

extern u8* D_dryfield_night_motel_room_3_8017DAB4[];

extern ViewCount D_dryfield_night_motel_room_3_8017DAB8[];

extern DirectionWarpEntry D_dryfield_night_motel_room_3_8017DABC[];

extern ViewCamera D_dryfield_night_motel_room_3_8017E1A4[];

extern SpriteView D_dryfield_night_motel_room_3_80180118[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_room_3_80180DC4[];

/// Draws the night motel room 3's flickering textured flares for the current view.
///
/// Bank-6 effect 0x109 uses the room-local camera view: view 3 draws one flare,
/// view 4 draws that flare then a second, views 10 and 11 draw only the second,
/// and view 8 draws a third with a different texture column and radius scale.
/// Other views reserve no packets. `task` supplies the callback signature and
/// is unused; this handler allocates no work and does not advance task state.
///
/// Requires the room overlay, live session, current view matrices and flare
/// textures, initialized scratch space, and frame arena/ordering-table storage.
/// Each selected flare reserves one packet even when rejected at camera Z / 4
/// below 17. Queued packets remain in the frame arena until GPU completion.
void dryfieldNightMotelRoom3DrawFlaresTask(Task* task);

void func_dryfield_night_motel_room_3_8017D6E0(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_ROOM_3_H
