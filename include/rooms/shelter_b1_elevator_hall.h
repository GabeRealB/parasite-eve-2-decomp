#ifndef ROOMS_SHELTER_B1_ELEVATOR_HALL_H
#define ROOMS_SHELTER_B1_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Projects `arg0[0]` and `arg0[1]` through `Gfx_ViewWorldMtx` and, when both
/// project, sweeps half a turn of gouraud `POLY_G4` wedges around the line
/// between them: a cap on each end and a band joining the two, with radii
/// `(s16)arg1 * 64 / otz` at each end. The lit vertices take the colour packed
/// in `arg2`'s low twelve bits (4 bits per channel, moved into the high
/// nibble), with bit 0 of the frame counter blended in as 8.
void func_shelter_b1_elevator_hall_8017DEB0(SVECTOR* arg0, s32 arg1, s32 arg2);

void func_shelter_b1_elevator_hall_8017E9C0(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a ring of sixteen gouraud `POLY_G4` wedges between radii
/// `(s16)arg1 * 64 / (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`,
/// black on the first and `rgb` on the second.
void func_shelter_b1_elevator_hall_8017EC84(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

void func_shelter_b1_elevator_hall_8017F0A8(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_elevator_hall_8017F980(GsCOORDINATE2* coord, s16 frame);

/// Scales the unit quad `D_80111E38` by `arg1`, rotates it flat by
/// `Gfx_ViewWorldMtx`, offsets it by `arg0->workm.t` and projects the corners
/// through `GsWSMATRIX`. When they project, queues one semi-transparent
/// `POLY_FT4` (tpage 0x28, clut 0x428C, colour `(0x30, 0x20, 0x20)`) whose
/// texture column alternates between two 32-texel frames with the frame
/// counter.
void func_shelter_b1_elevator_hall_8017FEAC(GsCOORDINATE2* arg0, s32 arg1);

void func_shelter_b1_elevator_hall_80180224(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// The same ring as `func_shelter_b1_elevator_hall_8017EC84`, built in a
/// scratch block with its fields in a different order.
void func_shelter_b1_elevator_hall_80180FBC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

void func_shelter_b1_elevator_hall_801813E8(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void func_shelter_b1_elevator_hall_801822EC(GsCOORDINATE2* coord, s16 frame, u8* rgb);

#endif // ROOMS_SHELTER_B1_ELEVATOR_HALL_H
