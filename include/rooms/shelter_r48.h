#ifndef INCLUDE_ROOMS_SHELTER_R48_H
#define INCLUDE_ROOMS_SHELTER_R48_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern WorldCollisionGrid D_shelter_r48_80183EEC[1];

extern TaskDesc D_shelter_r48_80182FAC;

extern AreaVariant D_shelter_r48_8018BC10[13];

// shelter_r48
extern WorldCoordRoomLighting D_shelter_r48_80183014[];

extern WorldCollisionRoomResources D_shelter_r48_8018301C[];

extern u8* D_shelter_r48_8018302C[];

extern ViewCount D_shelter_r48_80183030[];

extern DirectionWarpEntry D_shelter_r48_80183034[];

extern ViewCamera D_shelter_r48_80183F10[];

extern SpriteView D_shelter_r48_80189FB4[];

extern WorldCollisionSurfaceProperties* D_shelter_r48_8018BE10[];

void func_shelter_r48_8017E224(Task* task);

void func_shelter_r48_8017E27C(u8 arg0);

void func_shelter_r48_8017E3B8(Task* task);

void func_shelter_r48_8017E4C4(Task* arg0);

void func_shelter_r48_8017EC18(Task* task);

void func_shelter_r48_8017F6C0(Task* task);

/// Advances Shelter R48's drifting sprite with sign-selected palettes and fixed Y acceleration.
///
/// Requires a live coordinate-body task with initialized `EffectWork` in
/// `spawnArg2.pointer`. Bits 0..11 of `spawnArg1.value` give size, 12..14 give
/// period (1 only when bits 12..15 are all zero), 16..23 give speed (0 means 64),
/// and 24..27 select movement kind. A nonzero period nibble must encode a
/// nonzero period. Any bit in 28..31 selects the ten-cell alternate running
/// drawer; otherwise the twelve-cell banked drawer runs. Bit 31 selects palette
/// 1 (CLUT 0x428F), with palette 0 otherwise. Spin uses 4096 units per turn.
/// Initializes without drawing; moving updates advance position in coordinate
/// units then subtract 2/1 from banked/alternate Y velocity, even for kind 7.
/// Nonzero effect control freezes updates and selects the redraw's drawer from
/// bit 31 alone; control >= 4 frees work and task after the redraw. Drawing
/// requires composed coordinates, initialized scratch and primitive-packet space.
void shelterR48SpriteDriftTask(Task* task);

void func_shelter_r48_8017E704(Task* arg0);

void func_shelter_r48_8017E9B8(Task* arg0);

void func_shelter_r48_8017EFD8(Task* task);

void func_shelter_r48_801810B0(Task* task);

void func_shelter_r48_8018147C(Task* task);

void func_shelter_r48_80181704(Task* task);

#endif // INCLUDE_ROOMS_SHELTER_R48_H
