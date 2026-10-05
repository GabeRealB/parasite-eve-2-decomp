#ifndef INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
#define INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern TaskDesc D_neo_ark_observatory_80180DBC[2];

extern AreaVariant D_neo_ark_observatory_8018786C[13];

// neo_ark_observatory
extern WorldCollisionRoomResources D_neo_ark_observatory_80181594[];

extern WorldCoordRoomLighting D_neo_ark_observatory_801815B4[];

extern u8* D_neo_ark_observatory_801815DC[];

extern ViewCount D_neo_ark_observatory_801815E4[];

extern DirectionWarpEntry D_neo_ark_observatory_801815E8[];

extern ViewCamera D_neo_ark_observatory_80181FC8[];

extern SpriteView D_neo_ark_observatory_801860E8[];

extern WorldCollisionSurfaceProperties* D_neo_ark_observatory_80187A08[];

/// Sets the base grey intensity of the observatory's light beams.
///
/// Stores the signed low halfword without clamping; room entry and scenes use
/// 0..160. The drawer adds a four-frame pulse of -4..4 and skips negative
/// results. This changes beams only; the view's glow discs retain their colours.
/// The room overlay must be loaded. The glow task resets the value on its first tick.
void neoArkObservatorySetLightBeamIntensity(s32 intensity);

void func_neo_ark_observatory_8017FA98(s32 arg0);

/// Draws the observatory's glow discs and light beams for the mapped camera view.
///
/// Effect-table slot 0x14E resets beam intensity once in state 0, then draws
/// each tick in state 1. Requires the room overlay, composed view matrices,
/// initialized scratch stack, and the current frame's packet arena and ordering
/// table. It retains no pointers into those frame resources.
void neoArkObservatoryGlowTask(Task* effectTask);

void func_neo_ark_observatory_8017FDDC(Task* task);

#endif // INCLUDE_ROOMS_NEO_ARK_OBSERVATORY_H
