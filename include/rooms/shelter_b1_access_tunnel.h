#ifndef ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
#define ROOMS_SHELTER_B1_ACCESS_TUNNEL_H

#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

/// The room's three-entry task state table, dispatched by
/// `func_shelter_b1_access_tunnel_8017DD08` from a stack copy.
extern const TaskFuncTable3 D_shelter_b1_access_tunnel_8017D5F0;

/// Task descriptor `func_shelter_b1_access_tunnel_8017D5FC` spawns when a
/// gated event fires.
extern TaskDesc D_shelter_b1_access_tunnel_8017E704;

/// Message table `func_shelter_b1_access_tunnel_8017DCBC` installs on its task.
extern GpMsgEntry D_shelter_b1_access_tunnel_8017E71C[];

/// Copy of the message that fired a gated event, kept for the task
/// `func_shelter_b1_access_tunnel_8017D760` to warp from.
extern RoomEventMsg D_shelter_b1_access_tunnel_8017FF54;

/// Set by `func_shelter_b1_access_tunnel_8017D5FC` when the event it gates has
/// just fired, clear otherwise.
extern u8 D_shelter_b1_access_tunnel_8017FF5C;

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `func_shelter_b1_access_tunnel_8017D760` plays.
extern RoomEventReq D_shelter_b1_access_tunnel_8017FF70;

/// Gates an event on a game-flag nibble and a collected item: returns 1 when
/// the nibble already shows the event done, 0 (running the request's refusal
/// cap command) when the item is missing, and 2 when it fires, which unless
/// `msg` is a dry run records the request, sets the nibble and spawns the
/// event task.
s32 func_shelter_b1_access_tunnel_8017D5FC(RoomEventReq* req, RoomEventMsg* msg);

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.
void func_shelter_b1_access_tunnel_8017D760(Task* task);

/// Installs the room's message table on `task` and advances it.
void func_shelter_b1_access_tunnel_8017DCBC(Task* task);

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b1_access_tunnel_8017DD08(Task* task);

/// Draws a flickering two-point gouraud glow between `arg0[0]` and `arg0[1]`,
/// sized by `arg1` and tinted by the packed nibbles of `arg2`.
void func_shelter_b1_access_tunnel_8017DEC0(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B1_ACCESS_TUNNEL_H
