#ifndef ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
#define ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// The room task's three states, run from a stack copy by
/// `func_shelter_b1_sleeping_quarters_8017D888`: the entry tick, an idle
/// state, then `taskKill`.
extern const TaskFuncTable3 D_shelter_b1_sleeping_quarters_8017D5C4;

/// Draws a flickering gouraud glow spanning the projected points `arg0[0]`
/// and `arg0[1]`, of radius `arg1` turned by the angle `arg2`, tinted by the
/// channel bits of `arg3`.
void func_shelter_b1_sleeping_quarters_8017DB50(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);

/// Draws a flickering gouraud disc at the projected point `arg0`, of radius
/// `arg1`, tinted by the channel bits of `arg2`.
void func_shelter_b1_sleeping_quarters_8017E338(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Draws a gouraud ring around the projected world position of `arg0`, black
/// at radius `arg1` and shaded `rgb` at radius `arg1 + arg2`.
void func_shelter_b1_sleeping_quarters_8017F0DC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);

/// Draws a gouraud disc around the projected world position of `arg0`, shaded
/// `rgb` at the centre and black at radius `arg1`.
void func_shelter_b1_sleeping_quarters_8017F500(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);

/// Draws a flat textured quad of half-size `arg1` lying at the world position
/// of `arg0`.
void func_shelter_b1_sleeping_quarters_8017FF6C(GsCOORDINATE2* arg0, s32 arg1);

#endif // ROOMS_SHELTER_B1_SLEEPING_QUARTERS_H
