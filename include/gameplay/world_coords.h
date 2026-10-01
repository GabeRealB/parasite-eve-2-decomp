#ifndef GAMEPLAY_WORLD_COORDS_H
#define GAMEPLAY_WORLD_COORDS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/enemy.h"
#include "gameplay/light.h"

#include "main/coord.h"
#include "main/task_types.h"
#include "main/tmd_types.h"

struct Enemy;

extern GpCoord64 Gp_RoomCoords[8];

void func_800D7A9C(TmdObject* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

/// Rebuilds the actor color matrix via `func_800D7A9C`, then remaps it
/// from `colorMode` (`Gp_RemapActorColor`). While `colorBlend` is
/// a positive blend timer, GPF/GPL-interpolates the previous mode
/// (`colorMode` bits 2-3) toward the current mode (bits 0-1). Skips work
/// when `gGameSession->sceneUpdatesPaused == 1` unless `TmdObject.flags` bit
/// 0x80 is clear and `field_18` is set. `gSceneCombatState.actorControl` freezes the timer.
void Gp_UpdateActorColor(struct Enemy* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

void Gp_SetLightMode(struct Enemy* arg0, s32 arg1);

/// Returns the signed attenuation depth for spatial sound at a coordinate's local origin.
///
/// Subtracts the current view's projection distance from `coord->workm.t[2]`,
/// clamps to [-32767, 32767] game-coordinate units and shifts right by eight,
/// rounding negative values down. One depth unit spans 256 game-coordinate
/// units; the result is in [-128, 127]. Negative is nearer than the projection
/// plane, positive is farther, and zero supplies no sound attenuation.
///
/// `coord` must be live with its local-to-view matrix already composed. Reads
/// the cached matrix without updating it; changes no node, scratch or GTE state.
/// `worldCoordGetOriginAudioPan` supplies the corresponding spatial pan offset.
s32 worldCoordGetOriginAudioDepth(const GfxCoord* coord);

/// Returns the spatial sound pan offset for a coordinate node's local origin.
///
/// Projects with `coord->workm` and the current GTE projection settings, clamps
/// screen X to [-160, 159] pixels and divides by ten, rounding toward zero.
/// The result is in [-16, 15]: negative pans left, positive pans right and
/// zero leaves the sound's base pan unchanged. Sound events apply three SPU
/// pan steps per offset unit. A GTE summary error also returns zero.
///
/// `coord` must be live with its local-to-view matrix already composed. Requires
/// an initialized scratch stack with room for one projection record (24 bytes),
/// released before return. Replaces the GTE rotation/translation matrices and
/// projection results; it does not change the node or update its matrix.
s32 worldCoordGetOriginAudioPan(const GfxCoord* coord);

void Gp_SetOverrideVec(SVECTOR* arg0);

/// Sets the back colour a model is lit with: the translation of its colour
/// matrix, which the lighting adds to every vertex as the ambient term.
void Gp_SetObjTrans(TmdObject* arg0, s16 arg1, s16 arg2, s16 arg3);

void Gp_UpdateRoomCoords(Task* task);

void func_800D96C8(Task* arg0);

void func_800D9CC8(Task* arg0);

#endif // GAMEPLAY_WORLD_COORDS_H
