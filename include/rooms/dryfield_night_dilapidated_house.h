#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_dilapidated_house_80189FA4[22];

// dryfield_night_dilapidated_house
extern WorldCoordRoomLighting D_dryfield_night_dilapidated_house_8018738C[];

extern WorldCollisionRoomResources D_dryfield_night_dilapidated_house_80187394[];

extern u8* D_dryfield_night_dilapidated_house_801873A4[];

extern ViewCount D_dryfield_night_dilapidated_house_801873A8[];

extern DirectionWarpEntry D_dryfield_night_dilapidated_house_801873AC[];

extern ViewCamera D_dryfield_night_dilapidated_house_80187D68[];

extern SpriteView D_dryfield_night_dilapidated_house_8018921C[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_dilapidated_house_8018A0E4[];

/// Draws the nighttime dilapidated house's view-selected additive light prisms.
///
/// Bank-6 effect 0x106 requires a live `TASK_BODY_COORD` task and the loaded
/// room overlay, with a logical view ID in 1..11. Refreshes the task's composed
/// transform and selects the three prisms by logical view, before camera mapping.
/// Each selected prism queues five Gouraud quads and additive blend commands.
/// The view projection, scratch stack, depth ordering table and frame packet
/// arena must be ready; queued packet storage must survive GPU drawing.
void dryfieldNightDilapidatedHouseDrawLightPrismsTask(Task* task);

void func_dryfield_night_dilapidated_house_8017DA18(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_DILAPIDATED_HOUSE_H
