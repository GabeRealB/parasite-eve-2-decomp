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

void func_shelter_b6_growth_room_8017D7D4(Task* task);

// Called by the actor overlay's event scripts while this room is loaded.
void func_shelter_b6_growth_room_8017D82C(s32 arg0);

void func_shelter_b6_growth_room_8017D9D8(Task* task);

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
