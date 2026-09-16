#include "common.h"

#include "actors/actor_510900.h"

/// Rolled per draw by the state 1 handler; gameplay/3A34.c owns the definition.
extern s32 Gp_LcgState;

/// 1BC.h keeps this out of scope on purpose: callers hand it a sign-extended
/// animation id, which a `u16` prototype would zero-extend.
void func_800B4114(void* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

void func_actor_510900_8013B988(Actor510900* arg0)
{
    Actor510900Work* work;
    s32              state;
    s32              rng;

    work  = arg0->field_1C;
    state = work->field_590;
    switch (state) {
        case 0:
            work->field_5A2 = 0;
            work->field_586 = 0x13;
            if (Gp_TickObjFlag2((GpObj5D*)arg0->field_20) != 0) {
                work->field_586 = 0x14;
                work->field_590 = 1;
                work->field_5B8 = 0;
            }
            break;
        case 1:
            if (work->field_58A >= 0x3B) {
                work->field_58E = state;
                work->field_590 = 0;
                work->field_586 = state;
                work->field_59C =
                    D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                Gp_LcgState = rng;
            }
            break;
    }
}

void func_actor_510900_8013BA58(Actor510900* arg0)
{
    Actor510900Work* work;
    s32              state;
    GpEnemy*         enemy;
    s32              rng;

    work  = arg0->field_1C;
    state = work->field_590;
    enemy = arg0->field_20;
    switch (state) {
        case 0:
            work->field_586 = 0x11;
            work->field_5A2 = 0;
            work->field_590 = 1;
            break;
        case 1:
            if (work->field_58A >= 0x50) {
                if (enemy->field_40 <= 0) {
                    work->field_58E = 0xC;
                    work->field_590 = 0;
                    work->field_586 = 0x18;
                } else {
                    work->field_58E = state;
                    work->field_590 = 0;
                    work->field_586 = state;
                    work->field_59C =
                        D_actor_510900_801679F0[((u32)(rng = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
                    Gp_LcgState = rng;
                }
            }
            break;
    }
}

void func_actor_510900_8013BB20(Actor510900* arg0)
{
    Actor510900Work* work;
    s32              i;
    s32              value;

    work = arg0->field_1C;
    i    = 1;
    if (work->field_586 != work->field_588) {
        work->field_588 = work->field_586;
        work->field_58A = 0;
        value           = D_actor_510900_80167B38[work->field_586];
        for (; i < 0x13; i++) {
            func_800B4114(work, i, work->field_586, 0, value);
        }
    } else {
        TOUCH_REG(i);
        work->field_58A += i;
        do {
            Gp_AnimTickIndex((GpAnimCtx*)work, i);
            i++;
        } while (i < 0x13);
    }
}
