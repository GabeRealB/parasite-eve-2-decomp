#ifndef INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H
#define INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"
#include "gameplay/view.h"

#include "main/task_types.h"

// dryfield_motel_lobby
extern WorldCollisionRoomResources D_dryfield_motel_lobby_8017F838[];

extern u8* D_dryfield_motel_lobby_8017F848[];

extern ViewCount D_dryfield_motel_lobby_8017F84C[];

extern WorldCoordRoomLighting D_dryfield_motel_lobby_8017F850[];

extern DirectionWarpEntry D_dryfield_motel_lobby_8017F858[];

extern ViewCamera D_dryfield_motel_lobby_8017FC08[];

extern SpriteView D_dryfield_motel_lobby_80180AB0[];

extern WorldCollisionSurfaceProperties* D_dryfield_motel_lobby_80181044[];

/// Updates the motel-lobby telephone's save menu and optional play statistics.
///
/// `task->spawnArg2.pointer` must hold its live UI object; save and statistics
/// children remain linked until their answers are consumed. Requires the lobby
/// overlay and telephone UI resources to remain loaded throughout the menu.
void dryfieldMotelLobbyTelephoneMenuTask(Task* task);

/// Dispatches the motel lobby's room-message task.
///
/// `task->state` must be 0 (register receiver), 1 (idle), or 2 (teardown);
/// dispatch does not check the index. State 0 borrows this overlay's message
/// table and registers the task in `GAME_TASK_SLOT_ROOM`, then advances to 1.
/// Keep the overlay loaded while the task can receive messages. Registration
/// does not retain the task, and teardown does not clear the session slot.
void dryfieldMotelLobbyTask(Task* task);

#endif // INCLUDE_ROOMS_DRYFIELD_MOTEL_LOBBY_H
