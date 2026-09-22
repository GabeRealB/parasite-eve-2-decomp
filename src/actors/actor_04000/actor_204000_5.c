#include "common.h"
#include "actors/actor_104000.h"
#include "gameplay/3A34.h"

void Actor04000_Fn00E6C(Actor104000* arg0);

void Actor04000_Fn06AC4(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    obj  = arg1->field_2C;
    if (work->field_4 != 0) {
        arg0->field_14      = 1;
        obj->field_C        = 0x80;
        work->field_174     = 1;
        work->field_170     = 2;
        work->obj350.flags &= 0x7FFF;
        work->obj388.flags &= 0x7FFF;
        work->obj3C0.flags &= 0x7FFF;
        work->obj270.flags &= 0xBFFF;
        return;
    }
    Actor04000_Fn00E6C(arg1);
    switch (arg0->field_8 >> 12) {
        case 6:
        case 7:
            arg0->field_14 = 0;
            obj->field_C   = 0;
            break;
        case 3:
        case 4:
        case 5:
        default:
            arg0->field_14 = 1;
            obj->field_C   = 0x80;
            break;
    }
    if (Gp_StateF0.field_0 == 1) {
        work->field_0 = 7;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_04000/actor_204000_5", Actor04000_Fn06BC8);

INCLUDE_ASM("actors/nonmatchings/actor_04000/actor_204000_5", Actor04000_Fn06C80);

INCLUDE_ASM("actors/nonmatchings/actor_04000/actor_204000_5", Actor04000_Fn06D38);
