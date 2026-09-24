#include "common.h"

#include "actors/actor_104000.h"

void Actor04000_Fn06878(Actor104000Ctx* arg0, Actor104000* arg1)
{
    Actor104000Work*  work;
    Actor104000Obj2C* obj;

    work = arg1->field_1C;
    if (work->field_4 != 0) {
        obj                = arg1->field_2C;
        arg0->field_14     = 0;
        obj->field_C       = 0;
        work->field_176    = 0x10;
        work->field_178    = 0;
        work->obj270.flags = (u16)(work->obj270.flags | 0x4000);
        Actor04000_Fn00E6C(arg1);
        work->field_6 = 0;
        return;
    }
    Actor04000_Fn00E6C(arg1);
    if (!(work->field_6 & 7)) {
        Gp_SpawnPadLerp(3, 0xFF, 8);
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F9, Gp_PackObjPair((GpObj50*)arg0, 0), 0);
    }
    work->field_6++;
    if ((s16)work->field_6 > 0x28) {
        work->field_490 = 0x270F;
        if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F8, (s32)work->field_47C, 0) == 0) {
            work->field_0 = 5;
        } else {
            work->field_0 = 0xD;
        }
    }
}

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
