#ifndef ROOMS_SHELTER_B2_ELEVATOR_HALL_H
#define ROOMS_SHELTER_B2_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room.h"
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

/// Empty middle state of the room's state table.
void func_shelter_b2_elevator_hall_8017DD00(Task* task);

/// Runs the handler for the task's state from the room's state table.
void func_shelter_b2_elevator_hall_8017DD08(Task* task);

/// Projects `arg0[0]` and `arg0[1]` through `Gfx_ViewWorldMtx` and, when both
/// project, sweeps half a turn of gouraud `POLY_G4` wedges around the line
/// between them: a cap on each end and a band joining the two, with radii
/// `(s16)arg1 * 64 / otz` at each end. The lit vertices take the colour packed
/// in `arg2`'s low twelve bits (4 bits per channel, moved into the high
/// nibble), with bit 0 of the frame counter blended in as 8.
void func_shelter_b2_elevator_hall_8017DFB8(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Projects `arg0` through `Gfx_ViewWorldMtx` and, when it projects, queues a
/// sixteen-wedge gouraud disc plus a four-pointed inner cross. On-screen radii
/// are `(s16)arg1 * 64 / otz` (outer) and `(s16)arg1 * 8 / otz` (inner). `arg2`
/// packs the tint as four nibbles `[shift][r][g][b]`; bit 0 of the frame
/// counter, shifted by the top nibble, is added to every channel, so the disc
/// flickers on alternate frames. Each outer wedge draws at half brightness and
/// full size, then at full brightness and half size; the cross uses the halved
/// colour.
void func_shelter_b2_elevator_hall_8017E7FC(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues one semi-transparent textured quad centred on it, of
/// on-screen half-size `(arg2 & 0xFFF) * 23 / (otz + 1)`. `arg2`'s top nibble
/// and `arg1`'s low two bits pick the 24-texel cell; `arg3`'s low byte is the
/// grey level and its top nibble picks the CLUT.
void func_shelter_b2_elevator_hall_8017F4A4(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, u16 arg3);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a ring of sixteen gouraud `POLY_G4` wedges between radii
/// `(s16)arg1 * 64 / (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`,
/// black on the first and `rgb` on the second. Callers truncate `arg1` to 16
/// bits themselves.
void func_shelter_b2_elevator_hall_8017F768(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a disc of sixteen gouraud `POLY_G4` wedges of on-screen
/// radius `arg1 * 64 / (otz + 1)`, coloured `arg2` at the centre and black at
/// the rim.
void func_shelter_b2_elevator_hall_8017FB8C(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// Glow at a coordinate: two camera-facing textured quads, an inner one of
/// half-extent `size` and an outer one of `size * 3 / 2`, plus the ground quad
/// `func_shelter_b2_elevator_hall_80180990` draws under it. Also feeds the
/// `Gp_RoomCoords[2]` light a flickering intensity at the coordinate's position.
/// Draws nothing when the point fails to project.
void func_shelter_b2_elevator_hall_80180464(GsCOORDINATE2* coord, s16 size);

/// Scales the unit quad `D_80111E38` by `arg1`, rotates it flat by
/// `Gfx_ViewWorldMtx`, offsets it by `arg0->workm.t` and projects the corners
/// through `GsWSMATRIX`. When they project, queues one semi-transparent
/// `POLY_FT4` (tpage 0x28, clut 0x428C, colour `(0x30, 0x20, 0x20)`) whose
/// texture column alternates between two 32-texel frames with the frame
/// counter.
void func_shelter_b2_elevator_hall_80180990(GsCOORDINATE2* arg0, s32 arg1);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a gouraud starburst: a sixteen-wedge disc of radius
/// `arg1 * 64 / (otz + 1)` at half the colour `arg2`, the same disc at half
/// size and full colour, and a four-pointed cross at half colour, all fading
/// to black at the rim.
void func_shelter_b2_elevator_hall_80180D08(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// The same ring as `func_shelter_b2_elevator_hall_8017F768`, built in a
/// scratch block with its fields in a different order. Callers truncate `arg1`
/// to 16 bits themselves.
void func_shelter_b2_elevator_hall_80181AA0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// A second, instruction-for-instruction copy of
/// `func_shelter_b2_elevator_hall_8017FB8C`.
void func_shelter_b2_elevator_hall_80181ECC(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// Draws a trail of seven gouraud quads between two eight-slot rings of
/// coordinates, walking back from slot `arg2` and fading with age. `arg3`
/// packs the colour as 2-bit channel multipliers at bits 8, 4 and 0.
void func_shelter_b2_elevator_hall_80182750(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);

/// A second, instruction-for-instruction copy of
/// `func_shelter_b2_elevator_hall_80180D08`.
void func_shelter_b2_elevator_hall_80182DD0(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B2_ELEVATOR_HALL_H
