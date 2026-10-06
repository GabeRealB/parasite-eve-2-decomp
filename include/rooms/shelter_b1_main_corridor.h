#ifndef INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
#define INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b1_main_corridor_80185C30[22];

// shelter_b1_main_corridor
extern u8* D_shelter_b1_main_corridor_801831F8[];

extern ViewCount D_shelter_b1_main_corridor_801831FC[];

extern DirectionWarpEntry D_shelter_b1_main_corridor_80183200[];

extern WorldCollisionGrid D_shelter_b1_main_corridor_801840F0;

extern ViewCamera D_shelter_b1_main_corridor_80184114[];

extern SpriteView D_shelter_b1_main_corridor_80185128[];

extern WorldCoordRoomLights D_shelter_b1_main_corridor_801853E0;

extern WorldCollisionTrigger D_shelter_b1_main_corridor_801853F8[];

extern WorldCollisionTrigger D_shelter_b1_main_corridor_801858B8[];

extern WorldCollisionSurfaceProperties* D_shelter_b1_main_corridor_80185D04[];

void func_shelter_b1_main_corridor_8017DD98(Task* task);

/// Installs the room's effect IDs once and draws the lights in the active view.
///
/// Starts in state 0 and remains in state 1 after installing the IDs. Mapped
/// view indices 2..10 select fixed world-space beams and flares; others draw none.
/// Requires current view matrices, frame packet storage and the glow drawers'
/// scratch capacity. The task and room overlay stay live while it is dispatched.
void shelterB1MainCorridorDrawViewLightsTask(Task* task);

/// Runs the room's charging pink flash, peak screen tint and fading star.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` supplied through
/// `spawnArg2.pointer` by `Gp_SpawnEff`. `spawnArg1.value` is a positive charge
/// duration in active ticks, consumed as a countdown. Nonzero room effect
/// control pauses the task; values 4 and above cancel it. Completion or
/// cancellation releases the counted work and task; state 3 requests release.
void shelterB1MainCorridorRoomVisualEffectsFlashTask(Task* task);

/// Records two moving endpoints and draws their fading twin-trail beam.
///
/// Requires a coordinate body, zeroed, counted `EffectWork` from `Gp_SpawnEff`
/// and `Task::work` initially NULL. Borrows the spawn parent for its lifetime
/// and owns two eight-coordinate histories in `Task::work`; allocation failure
/// retries initialization. `spawnArg1.value` is zero for an unlimited lifetime,
/// or 2..32767 for the age in active ticks at which to release. Room effect
/// control values 2 and above pause updates. Normal teardown releases both
/// the counted work and histories.
void shelterB1MainCorridorRoomVisualEffectsTwinTrailTask(Task* task);

void func_shelter_b1_main_corridor_80182444(Task* task);

/// Runs the room's vertically drifting animated mote until its brightness fades.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `Gp_SpawnEff`. `spawnArg1.value` packs world-unit
/// half-extent in bits 0..11, palette in bits 12..15, unsigned vertical speed
/// in bits 16..23 and signed lifetime in active ticks in bits 24..31.
/// Bits 0..1 overlap the extent: either selects steady motion, with bit 1
/// upward; neither selects a rising mote with added random speed. Nonzero room
/// effect control pauses it; values 4 and above cancel it. Completion or
/// cancellation releases the counted work and task.
void shelterB1MainCorridorRoomVisualEffectsMoteTask(Task* task);

/// Runs the room's expanding tinted halo and shrinking ring, then a fading star.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` from
/// `Gp_SpawnEff`; its parent stays live and `pos` is the parent-space offset.
/// The unsigned low half of `spawnArg1` holds an expansion duration of 1..65535
/// active ticks; the signed high half selects tint index 0..2. Initialization
/// replaces the word with the remaining duration. Nonzero room effect control
/// pauses it; values 4 and above cancel it. State 3 requests release; teardown
/// frees the counted work.
void shelterB1MainCorridorRoomVisualEffectsHaloTask(Task* task);

/// Runs the room's growing orange disc and glow within an expanding, fading ring.
///
/// Requires a coordinate body and zeroed, counted `EffectWork` in
/// `spawnArg2.pointer` from `Gp_SpawnEff`; `spawnArg1` is unused. The ring
/// fades before the central burst. Nonzero room effect control pauses it;
/// values 4 and above cancel it. Completion or cancellation frees the counted
/// work and task.
void shelterB1MainCorridorRoomVisualEffectsHaloOrangeBurstTask(Task* task);

void func_shelter_b1_main_corridor_80180FC4(Task* arg0);

#endif // INCLUDE_ROOMS_SHELTER_B1_MAIN_CORRIDOR_H
