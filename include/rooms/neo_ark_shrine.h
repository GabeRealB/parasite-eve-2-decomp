#ifndef ROOMS_NEO_ARK_SHRINE_H
#define ROOMS_NEO_ARK_SHRINE_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// A pair of 16-bit coordinates used by the shrine's sliding-tile puzzle: the
/// screen position of a tile's quad, or the texture origin it samples from.
/// The puzzle tables index this by tile number or by board position.
typedef struct {
    u16 x;
    u16 y;
} NeoArkShrineSlot;

/// State table of the shrine's cap script task, indexed by `Task::state`.
extern const TaskFuncTable16 D_neo_ark_shrine_8017D5D0;

/// State table of the shrine's first falling prop, indexed by `Task::state`.
extern TaskFuncTable4 D_neo_ark_shrine_8017D610;

void func_neo_ark_shrine_8017E528(Task* task);
s32  func_neo_ark_shrine_8017EC10(RoomHotspot* table, s16 x, s16 y);
void func_neo_ark_shrine_8017F80C(Task* task);
void func_neo_ark_shrine_80180144(GsCOORDINATE2* coord, s32 inner, s32 width, u8* rgb);
void func_neo_ark_shrine_80180570(GsCOORDINATE2* coord, s32 radius, u8* rgb);
void func_neo_ark_shrine_80181474(GsCOORDINATE2* coord, s16 radius, u8* rgb);

#endif // ROOMS_NEO_ARK_SHRINE_H
