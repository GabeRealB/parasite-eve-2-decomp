#ifndef INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b6_corridor_80180304[13];

// shelter_b6_corridor
extern u8* D_shelter_b6_corridor_8017F8B4[];

extern ViewCount D_shelter_b6_corridor_8017F8B8[];

extern DirectionWarpEntry D_shelter_b6_corridor_8017F8BC[];

extern WorldCollisionGrid D_shelter_b6_corridor_8017FA90;

extern ViewCamera D_shelter_b6_corridor_8017FAB4[];

extern SpriteView D_shelter_b6_corridor_8018004C[];

extern WorldCoordRoomLights D_shelter_b6_corridor_801800E8;

extern WorldCollisionTrigger D_shelter_b6_corridor_80180100[];

extern WorldCollisionTrigger D_shelter_b6_corridor_8018036C[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_corridor_801804E8[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_corridor_80180548[];

/// Runs the corridor's initialization, background-mask upkeep or teardown state.
///
/// The map overlay selects this callback for stage 5, area 24. `task->state`
/// must be 0 (initialize), 1 (keep future image decodes masked), or 2 (kill).
/// The room overlay, session and loaded image workspace must remain available.
void shelterB6CorridorRoomTask(Task* task);

void func_shelter_b6_corridor_8017EBA4(Task* task);

/// Scenery states accepted by `shelterB6CorridorSetPartDestroyedSprites`.
enum {
    SHELTER_B6_CORRIDOR_PART_INTACT    = 0,
    SHELTER_B6_CORRIDOR_PART_DESTROYED = 1,
};

/// Shows or hides the destroyed scenery for one of the corridor's three Eve parts.
///
/// `partSlot` is 0..2 and `destroyed` is 0 (intact, hidden) or 1 (destroyed,
/// visible); other byte values do nothing. Slot 0 uses sprite-view element 1,
/// slot 1 elements 1 and 2, and slot 2 element 2 (zero-based). Requires the
/// current stage and area's corridor sprite tables to remain loaded.
void shelterB6CorridorSetPartDestroyedSprites(u8 partSlot, u8 destroyed);

/// Draws the corridor's yellow wall-light glows for the current mapped view.
///
/// Bank-6 slot 0x15E resets the player-hit-glow generation on its first tick.
/// Mapped views 2, 3 and 4 draw four, six and one capsule glows respectively;
/// other views draw none. Drawing continues regardless of effect-control state.
/// Requires the loaded corridor overlay, current view transform, initialized
/// scratch stack and a ready frame arena and ordering table.
void shelterB6CorridorDrawViewGlowsTask(Task* task);

/// Draws the newest expanding yellow player-hit disc and retires older instances.
///
/// Bank-6 slot 0x299 owns a zeroed `EffectWork` in `spawnArg2.pointer` and a
/// composed coordinate body. `scale` holds brightness; `angle` is the base
/// radius numerator (the disc uses four times it), scaled to pixels by
/// 64 / (OTZ + 1). `spawnArg1.value` records this instance's generation on its
/// first running tick.
/// Pauses while effects are stopped; cancellation, supersession and fade-out
/// release the counted work and task. Fading follows drawing from running tick
/// 9, ending at tick 16. Requires the room overlay and frame drawing resources.
void shelterB6CorridorPlayerHitGlowTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_CORRIDOR_H
