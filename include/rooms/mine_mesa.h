#ifndef ROOMS_MINE_MESA_H
#define ROOMS_MINE_MESA_H

#include "common.h"
#include "rooms/room.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/collision.h"
#include "gameplay/direction.h"

#include "main/task_types.h"

extern GpGridParams D_mine_mesa_801864A4;
extern GpGridParams D_mine_mesa_8018700C;

extern GpSaveLoc        D_mine_mesa_80189B40;
extern s8               D_mine_mesa_80189B48;
extern RoomLatchedEvent D_mine_mesa_80189B60;

/// The room's task descriptor table; its spawners pick an entry by index.
extern TaskDesc D_mine_mesa_801842F4;

/// Handle of the task spawned from entry 1 or 3 of `D_mine_mesa_801842F4`, or
/// NULL while none runs.
extern Task* D_mine_mesa_80189B54;

/// Handle of the task spawned from entry 4 of `D_mine_mesa_801842F4`, or NULL
/// while none runs.
extern Task* D_mine_mesa_80189B5C;

#endif // ROOMS_MINE_MESA_H
