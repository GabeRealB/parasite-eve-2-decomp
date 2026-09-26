#ifndef ROOMS_SHELTER_B2_ELEVATOR_HALL_H
#define ROOMS_SHELTER_B2_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "main/coord.h"

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"

/// Task descriptor `func_shelter_b2_elevator_hall_8017D610` spawns when a
/// gated event fires.
extern TaskDesc D_shelter_b2_elevator_hall_80183790;

/// Message table `func_shelter_b2_elevator_hall_8017DCBC` installs on its task.
extern GpMsgEntry D_shelter_b2_elevator_hall_801837A8[];

/// Per-index channel shifts `func_shelter_b2_elevator_hall_8017FF20` applies to
/// its brightness to colour the flash.
extern RoomHaloShade D_shelter_b2_elevator_hall_801838B8[];

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

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.
void func_shelter_b2_elevator_hall_8017D774(Task* task);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a disc of sixteen gouraud `POLY_G4` wedges of on-screen
/// radius `arg1 * 64 / (otz + 1)`, coloured `arg2` at the centre and black at
/// the rim.
void func_shelter_b2_elevator_hall_8017FB8C(GpCoord* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B2_ELEVATOR_HALL_H
