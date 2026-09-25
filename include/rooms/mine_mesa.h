#ifndef ROOMS_MINE_MESA_H
#define ROOMS_MINE_MESA_H

#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "rooms/room.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

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

/// Draws one semi-transparent textured quad centred on the projection of the
/// world-space point `pos`; `column` picks the texture column and `size` is a
/// signed half-extent scaled by depth.
void func_mine_mesa_8017EFA8(SVECTOR* pos, s32 column, s32 size);

/// Draws a gouraud ring of sixteen wedges around the projection of `coord`,
/// black at the half-extent `inner` and tinted by `rgb` at `inner + width`,
/// both scaled by depth.
void func_mine_mesa_8017F4D4(GsCOORDINATE2* coord, s32 inner, s32 width, u8* rgb);

/// Draws a gouraud disc of eight wedges around the projection of `coord`, lit
/// by `rgb` at the centre and fading to black; `radius` is a signed
/// half-extent scaled by depth.
void func_mine_mesa_8017F900(GsCOORDINATE2* coord, s32 radius, u8* rgb);

/// Draws a two-ring glow of gouraud wedges around the projection of `coord`,
/// tinted by `rgb`; `radius` is a signed half-extent scaled by depth.
void func_mine_mesa_80180804(GsCOORDINATE2* coord, s16 radius, u8* rgb);

#endif // ROOMS_MINE_MESA_H
