#ifndef INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H
#define INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_neo_ark_power_plant_1_80181AF8[13];

// neo_ark_power_plant_1
extern WorldCollisionRoomResources D_neo_ark_power_plant_1_8017F1C8[];

extern WorldCoordRoomLighting D_neo_ark_power_plant_1_8017F1D8[];

extern u8* D_neo_ark_power_plant_1_8017F1E0[];

extern ViewCount D_neo_ark_power_plant_1_8017F1E4[];

extern DirectionWarpEntry D_neo_ark_power_plant_1_8017F1E8[];

extern ViewCamera D_neo_ark_power_plant_1_801800B4[];

extern SpriteView D_neo_ark_power_plant_1_801814F0[];

extern WorldCollisionSurfaceProperties* D_neo_ark_power_plant_1_80181BE0[];

/// Switches the first power plant's Life Support scenery batches off or on.
///
/// Uses the low byte of hidden: 0 shows batch 1 in views 6 and 7, 1 hides it,
/// and other byte values leave both intact. Requires the current area's sprite
/// table in its first variant; the generator hides these while its part is live
/// and restores them at teardown. Borrows room-owned sprite storage for the call.
void neoArkPowerPlant1SetLifeSupportSpritesHidden(s32 hidden);

/// Draws the first power plant's view-specific light glows and generator flashes.
///
/// Mapped views 2..8 select fixed world-space emitters; other low-byte views draw
/// nothing. In views 6 and 7 destruction of the generator's support part replaces
/// its glow with a flash on a random one-in-eight running tick until the plant clears.
/// Requires live room-effect state, view matrices, scratch stack and frame packet resources.
/// The task is unused; spawned effects borrow a point owned by this room overlay.
void neoArkPowerPlant1DrawLightGlowsTask(Task* unusedTask);

/// Runs the Neo Ark Power Plant 1 room controller for one tick.
///
/// `task` must be live with state 0..2 and this room overlay loaded.
/// Initialize room messages and battle state, update events and sound, then kill.
/// State 0 registers the borrowed task in `GAME_TASK_SLOT_ROOM`; state 2
/// requests teardown. The controller allocates no work or body of its own.
void neoArkPowerPlant1RoomTask(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_POWER_PLANT_1_H
