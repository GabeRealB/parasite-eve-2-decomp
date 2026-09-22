#include "common.h"

#include "actors/actor_202300.h"
#include "gameplay/1BC.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/wipsys.h"

#include <psyq/abs.h>

/// First frame of each animation, indexed by `Actor202300Work::field_694`.
extern s16 D_actor_202300_8014DD64[];
/// The gameplay LCG state the lunge tick advances for its coin flip.
extern u32 Gp_LcgState;

/// The enemy's three state handlers - spawn/setup, per-frame tick and
/// teardown - dispatched through by state.
extern GpEnemyTaskFuncTable3 D_actor_202300_80149E80;
INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_80149EA4);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014A90C);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014AAF0);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014AC2C);

INCLUDE_RODATA("actors/nonmatchings/actor_202300/actor_202300", D_actor_202300_80149E80);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014B008);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014B100);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014B32C);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014B4B8);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014B6C4);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014B840);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014BC10);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014C0B0);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014C338);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014C6CC);

INCLUDE_ASM("actors/nonmatchings/actor_202300/actor_202300", func_actor_202300_8014CCC0);

/// The lunge's own tick, run out of a 0x10-byte `G_SCRATCH_HEAD` block. State
/// 0 is the wind-up: it holds `field_69C` at 0 until the animation reaches its
/// start frame, aims `field_6A4` at the player and compares it with the
/// enemy's own facing `field_6A2` - past 0x581 apart it gives up and turns
/// (animation 3, or animation 4 when `field_6DC` says it has already turned
/// once), within 0x80 it raises the body node's 0xC000 flags and commits as
/// soon as `field_6B2` reports contact. State 1 picks what to do next: inside
/// 0x8CA of the player it lunges (animation 8), otherwise it draws from
/// `Gp_LcgState` through a mask that widens by a bit each cycle and either
/// circles (animation 0xA) or walks in (animation 5). State 2 waits out the
/// recovery and state 3 the turn.
void func_actor_202300_8014D09C(Actor202300* actor)
{
    s16              yaw;
    s16              yaw2;
    s16              state;
    s16              deltaYaw;
    s16              deltaYaw2;
    s16              speed;
    s32              magnitude;
    s32              magnitude2;
    s16              wrapped;
    s16              wrapped2;
    s16              angle;
    s32              dx;
    s32              dz;
    u32              random;
    u16              flags;
    u16              flags2;
    u8*              head;
    VECTOR*          delta;
    Actor202300Work* work;
    GsCOORDINATE2*   coord;

    head                  = *(u8**)G_SCRATCH_HEAD;
    delta                 = (VECTOR*)(head - 0x10);
    *(u8**)G_SCRATCH_HEAD = (u8*)delta;
    work                  = actor->field_1C;
    state                 = work->field_6A8;
    coord                 = actor->field_2C->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= D_actor_202300_8014DD64[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = 0x3C;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = (u16)work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | 0xC000;
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & 0x3FFF);
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            dz              = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x8CA) {
                work->field_6A6 = 4;
                work->field_6A8 = 0;
                work->field_694 = 8;
            } else {
                random      = (Gp_LcgState * 5) + 0x71357911;
                Gp_LcgState = random;
                if (!((random >> 0x10) & ((1 << (work->field_6C0 + 1)) - 1)) && !(Player_Status.peStateFlags & 0x10) &&
                    work->field_6C4 != 0) {
                    work->field_6A6 = 5;
                    work->field_6A8 = 0;
                    work->field_694 = 0xA;
                    work->field_6AE = 0;
                    work->field_6C2 = 1;
                    work->field_6B6 = 0;
                    work->field_6C0++;
                } else {
                    work->field_6A6 = 3;
                    work->field_6A8 = 0;
                    work->field_694 = 5;
                    work->field_6AE = 0;
                }
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | 0xC000;
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & 0x3FFF);
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = (u16)work->field_6A4 - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
            }
            break;
    }
    *(s32*)G_SCRATCH_HEAD += 0x10;
}
