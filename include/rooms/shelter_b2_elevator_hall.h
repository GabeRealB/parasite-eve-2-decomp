#ifndef ROOMS_SHELTER_B2_ELEVATOR_HALL_H
#define ROOMS_SHELTER_B2_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"

/// The room's three-entry task state table, dispatched by
/// `func_shelter_b2_elevator_hall_8017DD08` from a stack copy.
extern const TaskFuncTable3 D_shelter_b2_elevator_hall_8017D5F0;

/// Task descriptor `func_shelter_b2_elevator_hall_8017D610` spawns when a
/// gated event fires.
extern TaskDesc D_shelter_b2_elevator_hall_80183790;

/// Message table `func_shelter_b2_elevator_hall_8017DCBC` installs on its task.
extern GpMsgEntry D_shelter_b2_elevator_hall_801837A8[];

/// Per-index channel shifts `func_shelter_b2_elevator_hall_8017FF20` applies to
/// its brightness to colour the flash.
extern RoomsShared8017e4f8Shade D_shelter_b2_elevator_hall_801838B8[];

/// Positions `func_shelter_b2_elevator_hall_80182260` places its two trail
/// anchors at; the second entry is also named on its own below.
extern SVECTOR D_shelter_b2_elevator_hall_801838CC[];
extern SVECTOR D_shelter_b2_elevator_hall_801838D4;

/// Copy of the message that fired a gated event, kept for the task
/// `func_shelter_b2_elevator_hall_8017D774` to warp from.
extern RoomEventMsg D_shelter_b2_elevator_hall_80184D7C;

/// Set by `func_shelter_b2_elevator_hall_8017D610` when the event it gates has
/// just fired, clear otherwise.
extern u8 D_shelter_b2_elevator_hall_80184D84;

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `func_shelter_b2_elevator_hall_8017D774` plays.
extern RoomEventReq D_shelter_b2_elevator_hall_80184D88;

/// Gates an event on a game-flag nibble and a collected item: returns 1 when
/// the nibble already shows the event done, 0 (running the request's refusal
/// cap command) when the item is missing, and 2 when it fires, which unless
/// `msg` is a dry run records the request, sets the nibble and spawns the
/// event task.
s32 func_shelter_b2_elevator_hall_8017D610(RoomEventReq* req, RoomEventMsg* msg);

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.
void func_shelter_b2_elevator_hall_8017D774(Task* task);

/// Installs the room's message table on `task` and advances it.
void func_shelter_b2_elevator_hall_8017DCBC(Task* task);

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b2_elevator_hall_8017DD08(Task* task);

/// Draws a flickering two-point gouraud glow between `arg0[0]` and `arg0[1]`,
/// sized by `arg1` and tinted by the packed nibbles of `arg2`.
void func_shelter_b2_elevator_hall_8017DFB8(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a flickering gouraud disc at `arg0`, sized by `arg1` and tinted by the
/// packed nibbles of `arg2`.
void func_shelter_b2_elevator_hall_8017E7FC(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_shelter_b2_elevator_hall_8017F4A4(GsCOORDINATE2* coord, s32 frame, s32 arg2, s32 arg3);
void func_shelter_b2_elevator_hall_8017FB8C(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b2_elevator_hall_80180464(GsCOORDINATE2* coord, s16 frame);
void func_shelter_b2_elevator_hall_80180D08(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b2_elevator_hall_80181ECC(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b2_elevator_hall_80182DD0(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// Draws a trail of seven gouraud quads between two eight-slot rings of
/// coordinates, walking back from slot `arg2` and fading with age. `arg3`
/// packs the colour as 2-bit channel multipliers at bits 8, 4 and 0.
void func_shelter_b2_elevator_hall_80182750(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);

#endif // ROOMS_SHELTER_B2_ELEVATOR_HALL_H
