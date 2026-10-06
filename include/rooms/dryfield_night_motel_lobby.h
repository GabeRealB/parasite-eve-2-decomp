#ifndef INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
#define INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H

#include "types.h"

#include "gameplay/area.h"
#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

extern AreaVariant D_dryfield_night_motel_lobby_801843C4[11];

// dryfield_night_motel_lobby
extern WorldCollisionRoomResources D_dryfield_night_motel_lobby_80182908[];

extern WorldCoordRoomLighting D_dryfield_night_motel_lobby_80182918[];

extern u8* D_dryfield_night_motel_lobby_80182920[];

extern ViewCount D_dryfield_night_motel_lobby_80182924[];

extern DirectionWarpEntry D_dryfield_night_motel_lobby_80182928[];

extern ViewCamera D_dryfield_night_motel_lobby_80182DD8[];

extern SpriteView D_dryfield_night_motel_lobby_80183D1C[];

extern WorldCollisionSurfaceProperties* D_dryfield_night_motel_lobby_8018448C[];

void func_dryfield_night_motel_lobby_8017EAE0(Task* task);

/// Draws the night motel lobby's light glows for the current logical view.
///
/// Bank-6 effect 0x10D; `unusedTask` is ignored. The room overlay must be loaded.
/// Views 2 and 4 draw a pulsing cyan diamond, view 5 a layered cyan disc;
/// views 2, 3 and 4 draw flickering textured flares. Other views queue nothing.
/// The view matrices, ordering table and current frame's packet arena must be
/// ready. Drawers reject negative GTE flags and otherwise require nonzero
/// camera Z / 4 for perspective sizing. Queued packets live in the frame arena.
/// Requires 64 free scratch bytes; each flare leaves 16 bytes reserved until
/// the frame reset, up to 48 bytes per update in view 2.
void dryfieldNightMotelLobbyDrawGlowsTask(Task* unusedTask);

void func_dryfield_night_motel_lobby_8017FE38(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
