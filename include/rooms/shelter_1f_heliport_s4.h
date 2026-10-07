#ifndef INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H
#define INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// shelter_1f_heliport_s4
extern u8* D_shelter_1f_heliport_s4_8017D6F8[];

extern ViewCount D_shelter_1f_heliport_s4_8017D6FC[];

extern DirectionWarpEntry D_shelter_1f_heliport_s4_8017D700[];

extern WorldCollisionGrid D_shelter_1f_heliport_s4_8017D9C8;

extern ViewCamera D_shelter_1f_heliport_s4_8017D9EC[];

extern SpriteView D_shelter_1f_heliport_s4_8017DAF0[];

extern WorldCoordRoomLights D_shelter_1f_heliport_s4_8017DE6C;

extern WorldCollisionTrigger D_shelter_1f_heliport_s4_8017DE84[];

extern WorldCollisionSurfaceProperties* D_shelter_1f_heliport_s4_8017E060[];

/// Maintains the heliport room's message receiver for the loaded room overlay.
///
/// State 0 installs and publishes the receiver, state 1 idles, and state 2
/// releases the task. Call with a live task whose state is in 0..2. Spawn
/// arguments are unused; map_shelter selects this callback for stage 4, area 38.
void shelter1fHeliportS4RoomTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_1F_HELIPORT_S4_H
