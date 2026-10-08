#ifndef INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H
#define INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_shelter_b6_growth_room_80180338[13];

// shelter_b6_growth_room
extern u8* D_shelter_b6_growth_room_8017F378[];

extern ViewCount D_shelter_b6_growth_room_8017F37C[];

extern DirectionWarpEntry D_shelter_b6_growth_room_8017F380[];

extern WorldCollisionGrid D_shelter_b6_growth_room_8017FAF0;

extern ViewCamera D_shelter_b6_growth_room_8017FB14[];

extern SpriteView D_shelter_b6_growth_room_8017FEB8[];

extern WorldCoordRoomLights D_shelter_b6_growth_room_8017FF78;

extern WorldCollisionTrigger D_shelter_b6_growth_room_8017FF90[];

extern WorldCollisionTrigger D_shelter_b6_growth_room_801803A0[];

extern WorldCoordRoomAmbientEntry D_shelter_b6_growth_room_80180730[];

extern WorldCollisionSurfaceProperties* D_shelter_b6_growth_room_801807A8[];

/// Runs growth-room entry setup, message service and teardown.
///
/// Requires the room and its actor_450900 scene resources to remain loaded.
/// State 0 installs room messages and starts the entry scene, state 1 waits
/// for messages, and state 2 tears down the task. No other state is valid.
void shelterB6GrowthRoomTask(Task* task);

/// Restores the growth room's reserved collision box with an optional Y displacement.
///
/// Requires the growth-room overlay and writable live grid pools. Zero
/// useYOffset restores the template's room coordinates; nonzero adds 2000 game
/// units to Y. Replaces the leading four faces/normals and eight vertices,
/// preserving vector fourth components, later geometry and all cell lists.
/// Repeated calls restore before shifting. Actor scene scripts call this while
/// the room's collision grid remains active; no resource is allocated or freed.
void shelterB6GrowthRoomResetCollisionBox(s32 useYOffset);

/// Draws the growth room's glows and emits its mist and drifting puffs.
///
/// Gameplay spawns bank-6 slot 0x15D with a coordinate body, state 0 and
/// spawnArg1 zero. That word grows to 304 every eighth animation frame while
/// room effects run; it sets the bottom-glow height in pixels and mist's
/// vertical spread in coordinate units. State becomes the mist-source count,
/// 1..6, and stays at six once reached. Requires the room overlay, loaded
/// particle textures, current view matrices, initialized scratch stack and
/// frame packet arena/OT.
/// Continues drawing and emitting particles when the ramp is paused.
void shelterB6GrowthRoomAmbientEffectsTask(Task* task);

/// Animates a ten-cell, fading mist sprite with random horizontal drift.
///
/// Requires the growth-room overlay, a coordinate body and the counted
/// `EffectWork` installed in spawnArg2 by the effect spawner. spawnArg1 packs
/// size in bits 0..11, frame ticks in 12..14, and speed in 16..23 (zero selects
/// 64 coordinate units/tick). A zero 12..15 field selects one tick; otherwise
/// bits 12..14 must be 1..7. The room supplies size 1280, six ticks and speed 16.
/// Ramps brightness in four-unit steps, draws before moving, then advances the
/// texture frame; frees the counted work and task after cell 9.
void shelterB6GrowthRoomMistTask(Task* task);

/// Animates a ten-cell puff with a fixed random rotation and downward acceleration.
///
/// Requires the growth-room overlay, a coordinate body and the counted
/// `EffectWork` installed in spawnArg2 by the effect spawner. spawnArg1 has the
/// size, frame-tick and speed encoding of `shelterB6GrowthRoomMistTask`; the room
/// supplies size 640, three ticks and speed 24. Samples a positive-X horizontal
/// direction, draws before moving, and adds two coordinate units/tick to Y
/// velocity after each move. Frees the counted work and task after cell 9.
void shelterB6GrowthRoomDriftPuffTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B6_GROWTH_ROOM_H
