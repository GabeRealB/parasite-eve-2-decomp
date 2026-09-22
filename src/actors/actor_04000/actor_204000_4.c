#include "common.h"

#include "actors/actor_104000.h"

void Actor04000_Fn00E6C(Actor104000* arg0);

INCLUDE_ASM("actors/nonmatchings/actor_04000/actor_204000_4", Actor04000_Fn06878);

void Actor04000_Fn06994(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                      = arg1->field_2C;
        arg1->field_20->field_14 = 0;
        obj->field_C             = 0;
        work->field_170          = 2;
        work->field_176          = 0x10;
        work->field_178          = 0;
        work->field_174          = 0xF;
        work->obj270.flags       = (u16)(work->obj270.flags | 0x4000);
        Actor04000_Fn00E6C(arg1);
        work->field_6   = 0;
        work->field_47A = (u8)(work->field_47A + 1);
    }
    Actor04000_Fn00E6C(arg1);
    if (work->field_58 & 1) {
        work->field_0 = 4;
    }
    if ((s8)work->field_47A >= 4) {
        work->field_0 = 5;
    }
}
