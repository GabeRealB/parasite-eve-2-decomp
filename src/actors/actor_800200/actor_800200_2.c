#include "common.h"

#include "actors/actor_800200.h"

#include <psyq/rand.h>

extern s32 func_80103DD4(VECTOR3*, VECTOR3*);

extern s32 func_8010BC70(GsCOORDINATE2*);

extern void func_actor_800200_801653A0(GpActorWork*);

extern GpActorPathStep D_actor_800200_80169FE0[];

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162750);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162990);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_2", func_actor_800200_80162BFC);

void func_actor_800200_80162E0C(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    s32            delay;

    coord  = arg0->extra->field_8;
    target = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    actor  = arg0->actor;
    d4     = actor->field_910;
    switch (actor->field_960) {
        case 0:
            actor->field_20 = D_actor_800200_80169FE0[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_80169FE0[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 2) {
                    actor->field_960 = 3;
                    actor->field_95E = 0;
                    actor->field_95C = 7;
                    Gp_AnimPlayChildSlotsEx(arg0, 7, 0, 3);
                    func_actor_800200_80165408(arg0, 6);
                    return;
                }
                if (func_8010BC70(coord) >= 0xE00) {
                    actor->field_960 = 1;
                    actor->field_934 = 0;
                    actor->field_90C = 0;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->field_CE++;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            if ((func_8010BC70(coord) < 0xA01) || (coord->coord.t[0] < target->coord.t[0])) {
                d4->field_CE++;
                actor->field_960 = 0;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                d4->field_CC     = 1;
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_80165434(arg0, 0);
            }
            return;
        case 3:
            if (actor->field_95E != 0) {
                actor->field_960++;
            }
            return;
        case 4:
            actor->field_95C = 0;
            actor->field_960++;
            Gp_AnimResetChildSlots(arg0, 9);
            return;
        default:
        case 2:
        case 5:
            return;
    }
}
