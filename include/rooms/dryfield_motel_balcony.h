#ifndef ROOMS_DRYFIELD_MOTEL_BALCONY_H
#define ROOMS_DRYFIELD_MOTEL_BALCONY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// The scratch block of the room's two fan-drawing routines, laid out like
/// `RoomDraw04Scratch`.
typedef RoomDraw04Scratch RoomFanScratch;

/// The room task's three states: setup, idle and exit.
extern const TaskFuncTable3 D_dryfield_motel_balcony_8017D5DC;

/// Set by the event gate when its last call latched a request and spawned the
/// event task; every call clears it first.
extern u8 D_dryfield_motel_balcony_8018672C;

/// The balcony's event gate. A request whose flag nibble already records the
/// event (a set nibble, or a clear one for a negative `flagId`) answers 1. One
/// whose prerequisite item has not been collected runs the request's CAP
/// command and answers 0. Otherwise the gate answers 2 and - unless the
/// message's `field_5` asks for a dry run - latches the message and the
/// request, writes the flag nibble and spawns the event task.
s32 func_dryfield_motel_balcony_8017D5E8(RoomEventReq* req, RoomEventMsg* msg);

void func_dryfield_motel_balcony_8017DB84(Task* task);
void func_dryfield_motel_balcony_8017DBC8(Task* arg0);

void func_dryfield_motel_balcony_8017DF84(GsCOORDINATE2*, s32, s32, s32);
void func_dryfield_motel_balcony_8017E66C(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
void func_dryfield_motel_balcony_8017EF44(GsCOORDINATE2* arg0, s16 arg1);
void func_dryfield_motel_balcony_8017F7E8(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
void func_dryfield_motel_balcony_801809AC(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);
void func_dryfield_motel_balcony_801818B0(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif
