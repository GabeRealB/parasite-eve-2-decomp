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

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues one semi-transparent textured quad centred on it, of
/// on-screen half-size `(arg2 & 0xFFF) * 23 / (otz + 1)`. `arg2`'s top nibble
/// and `arg1`'s low two bits pick the 24-texel cell; `arg3`'s low byte is the
/// grey level and its top nibble picks the CLUT.
void func_shelter_b1_elevator_hall_8017E9C0(GsCOORDINATE2* arg0, u16 arg1, u16 arg2, u16 arg3);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a ring of sixteen gouraud `POLY_G4` wedges between radii
/// `(s16)arg1 * 64 / (otz + 1)` and `(s16)(arg1 + arg2) * 64 / (otz + 1)`,
/// black on the first and `rgb` on the second. Callers truncate `arg1` to 16
/// bits themselves.
void func_shelter_b1_elevator_hall_8017EC84(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a disc of eight gouraud `POLY_G4` quads of on-screen
/// radius `arg1 * 64 / (otz + 1)`, coloured `arg2` at the centre and black at
/// the rim.
void func_shelter_b1_elevator_hall_8017F0A8(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// Glow at a coordinate: two camera-facing textured quads, an inner one of
/// half-extent `size` and an outer one of `size * 3 / 2`, plus the ground quad
/// `func_shelter_b1_elevator_hall_8017FEAC` draws under it. Also feeds the
/// gameplay light slot a flickering intensity at the coordinate's position.
/// Draws nothing when the point fails to project.
void func_shelter_b1_elevator_hall_8017F980(GsCOORDINATE2* coord, s16 size);

/// Scales the unit quad `D_80111E38` by `arg1`, rotates it flat by
/// `Gfx_ViewWorldMtx`, offsets it by `arg0->workm.t` and projects the corners
/// through `GsWSMATRIX`. When they project, queues one semi-transparent
/// `POLY_FT4` (tpage 0x28, clut 0x428C, colour `(0x30, 0x20, 0x20)`) whose
/// texture column alternates between two 32-texel frames with the frame
/// counter.
void func_shelter_b1_elevator_hall_8017FEAC(GsCOORDINATE2* arg0, s32 arg1);

/// Projects `arg0`'s world position through `GsWSMATRIX` and, when it
/// projects, queues a gouraud starburst: an eight-quad disc of radius
/// `arg1 * 64 / (otz + 1)` at half the colour `arg2`, the same disc at half
/// size and full colour, and a four-pointed cross at half colour, all fading
/// to black at the rim.
void func_shelter_b1_elevator_hall_80180224(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// The same ring as `func_shelter_b1_elevator_hall_8017EC84`, built in a
/// scratch block with its fields in a different order. Callers truncate `arg1`
/// to 16 bits themselves.
void func_shelter_b1_elevator_hall_80180FBC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// An instruction-for-instruction copy of
/// `func_shelter_b1_elevator_hall_8017F0A8`.
void func_shelter_b1_elevator_hall_801813E8(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

/// An instruction-for-instruction copy of
/// `func_shelter_b1_elevator_hall_80180224`.
void func_shelter_b1_elevator_hall_801822EC(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B1_ELEVATOR_HALL_H
