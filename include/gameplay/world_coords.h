#ifndef GAMEPLAY_WORLD_COORDS_H
#define GAMEPLAY_WORLD_COORDS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/enemy.h"
#include "gameplay/light.h"

#include "main/coord.h"
#include "main/task.h"
#include "main/tmd.h"

struct GpEnemy;

extern GpCoord64 Gp_RoomCoords[8];

void func_800D7A9C(TmdObject* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

/// Rebuilds the actor color matrix via `func_800D7A9C`, then remaps it
/// from `field_4E` lighting mode (`Gp_RemapActorColor`). While `field_4F` is
/// a positive blend timer, GPF/GPL-interpolates the previous mode
/// (`field_4E` bits 2-3) toward the current mode (bits 0-1). Skips work
/// when `gGameSession->field_65 == 1` unless `TmdObject.flags` bit
/// 0x80 is clear and `field_18` is set. `Gp_StateF0.field_4` freezes the timer.
void Gp_UpdateActorColor(struct GpEnemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

void Gp_SetLightMode(struct GpEnemy* arg0, s32 arg1);

/// How far a coordinate's origin lies from the current view's projection plane,
/// in the form the sound events take their depth argument: saturated to ±0x7FFF
/// and scaled down by 256, which lands in the signed byte they read.
///
/// `Gp_GetObjPan` is the pan that goes with it.
s32 gpGetObjDepth(GpCoord* coord);

/// Pan of the object's world origin, for the sound events that carry one: the
/// origin is projected through the coordinate's world matrix, and the screen X
/// it lands on is clamped to the screen's half width and scaled down by ten.
/// 0 when the projection reports an error.
s32 Gp_GetObjPan(GpCoord* coord);

void Gp_SetOverrideVec(SVECTOR* arg0);

/// Sets the back colour a model is lit with: the translation of its colour
/// matrix, which the lighting adds to every vertex as the ambient term.
void Gp_SetObjTrans(TmdObject* arg0, s16 arg1, s16 arg2, s16 arg3);

void Gp_UpdateRoomCoords(Task* task);

void func_800D96C8(Task* arg0);

void func_800D9CC8(Task* arg0);

#endif // GAMEPLAY_WORLD_COORDS_H
