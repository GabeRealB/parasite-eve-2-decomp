#ifndef ROOMS_SHELTER_B2_ELEVATOR_HALL_H
#define ROOMS_SHELTER_B2_ELEVATOR_HALL_H

#include "gameplay/area.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017e4f8.h"

#include "gameplay/message.h"

#include "main/task_types.h"

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

/// Copy of the request that fired a gated event, whose cap command and voice
/// lines the task `func_shelter_b2_elevator_hall_8017D774` plays.
extern RoomEventReq D_shelter_b2_elevator_hall_80184D88;

/// Event task: plays the recorded request's cap command and voice lines, then
/// copies the recorded message's area, warp and room into the save location,
/// spawns task 0x11 and ends.
void func_shelter_b2_elevator_hall_8017D774(Task* task);

void func_shelter_b2_elevator_hall_801817FC(Task* arg0);
void func_shelter_b2_elevator_hall_80182260(Task* task);
void func_shelter_b2_elevator_hall_80182B48(Task* task);
void func_shelter_b2_elevator_hall_8017F1D8(Task* task);
void func_shelter_b2_elevator_hall_8017FF20(Task* arg0);
void func_shelter_b2_elevator_hall_801802B8(Task* arg0);
void func_shelter_b2_elevator_hall_801816C8(Task* arg0);
extern GpAreaVariant D_shelter_b2_elevator_hall_80184C7C[22];

void func_shelter_b2_elevator_hall_8017DD60(Task* arg0);

#endif // ROOMS_SHELTER_B2_ELEVATOR_HALL_H
