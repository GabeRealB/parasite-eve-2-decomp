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

/// Updates the night motel lobby's telephone save menu and statistics panels.
///
/// Requires the lobby overlay and a live telephone UI task whose
/// `spawnArg2.pointer` owns its UI object. State 0 initializes the menu;
/// states 1..3 process save, statistics and notice dismissal. Normal play
/// before a clear opens saving directly; a clear or demo scene 1 enables
/// the four-entry menu. Child UI objects supply results through this task.
void dryfieldNightMotelLobbyTelephoneMenuTask(Task* task);

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

/// Runs the night motel lobby's room receiver and masterkey objective monitor.
///
/// Requires the loaded lobby and state 0 (initialize), 1 (monitor), or 2 (kill).
/// Initialization registers the room receiver and suppresses an objective
/// update for an already collected Bronco masterkey. The monitor records
/// objective 20 only when that persistent collection bit changes from 0 to 1.
void dryfieldNightMotelLobbyRoomTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_NIGHT_MOTEL_LOBBY_H
