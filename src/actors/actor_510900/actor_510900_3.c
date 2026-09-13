#include "common.h"

#include "actors/actor_510900.h"

/// Rolled per draw by the state 1 handler; gameplay/3A34.c owns the definition.
extern s32 Gp_LcgState;

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

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BA58);

INCLUDE_ASM("actors/nonmatchings/actor_510900/actor_510900_3", func_actor_510900_8013BB20);
