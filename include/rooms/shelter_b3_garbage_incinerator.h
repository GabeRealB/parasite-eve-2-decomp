#ifndef INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
#define INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H

#include "types.h"
#include "overlay.h"

#include "gameplay/area.h"
#include "gameplay/area_flags.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern OverlayEncounterSpot D_shelter_b3_garbage_incinerator_801874C4[16];

extern u16 D_shelter_b3_garbage_incinerator_801855DE;

extern TaskDesc D_shelter_b3_garbage_incinerator_80187150[4];

extern u8 D_shelter_b3_garbage_incinerator_80187328[40];

extern AreaApplyRec D_shelter_b3_garbage_incinerator_8018FB6C[23];

extern u16 D_shelter_b3_garbage_incinerator_8018FBC8[2];

extern AreaVariant D_shelter_b3_garbage_incinerator_8018FA58[13];

// shelter_b3_garbage_incinerator
extern WorldCoordRoomLighting D_shelter_b3_garbage_incinerator_80187280[];

extern WorldCollisionRoomResources D_shelter_b3_garbage_incinerator_801872B8[];

extern u8* D_shelter_b3_garbage_incinerator_801873F0[];

extern ViewCount D_shelter_b3_garbage_incinerator_8018740C[];

extern DirectionWarpEntry D_shelter_b3_garbage_incinerator_8018741C[];

extern ViewCamera D_shelter_b3_garbage_incinerator_801883AC[];

extern SpriteView D_shelter_b3_garbage_incinerator_8018D100[];

extern WorldCollisionSurfaceProperties* D_shelter_b3_garbage_incinerator_8018FB4C[];

void func_shelter_b3_garbage_incinerator_8017DC7C(Task* task);

void func_shelter_b3_garbage_incinerator_80180FE4(s16 arg0, s16 arg1, s16 arg2);

void func_shelter_b3_garbage_incinerator_8018507C(void);

void func_shelter_b3_garbage_incinerator_80185220(void);

void func_shelter_b3_garbage_incinerator_8018110C(Task* task);

void shelterB3GarbageIncineratorEffectSpriteDriftTaskAimed(Task* task);

/// Animates one eight-cell debris sprite from the incinerator's effect task slot.
///
/// Requires a live counted effect with owned `EffectWork` in `spawnArg2.pointer`
/// and a single-coordinate body. The coordinate's `workm` must be composed for
/// drawing. Start at state 0 and cell `index` 0..7; states 1/2 draw chip/billboard.
/// Initialization takes one running update without drawing, moving or advancing.
///
/// `spawnArg1` bits 0..11 set perspective size (0..4095); bits 12..15 give
/// running updates per cell (0 means 1); bits 16..23 give launch speed in parent
/// coordinate units per update (0 means 64). Bits 24..27 choose direction:
/// 0 stationary, 1 random upward, 2 random on all axes, 3 narrow upward, 5 from
/// `work->pos`. Other kinds leave the zero direction for SDK normalization.
/// Any set bit in 28..31 selects the billboard. Spin is randomized once in
/// 4096 units per turn. A nonzero initial `move` is retained as velocity and
/// enables movement regardless of the packed speed or direction.
///
/// Running updates draw, move by signed halfword velocity, then add 6 to Y
/// velocity and advance the cell every period. Stationary sprites skip both
/// movement and gravity. Suspension redraws a chip without advancing; cancellation
/// redraws that chip once and releases the counted work and task. Finishing cell
/// 7 also releases them. Drawers require initialized GTE, scratch and packet state.
void shelterB3GarbageIncineratorEffectSpriteDebrisTask(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_B3_GARBAGE_INCINERATOR_H
