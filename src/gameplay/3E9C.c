#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/task.h"

void func_800F68AC(VECTOR3* arg0, s32 arg1, s32 arg2);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ECAA8);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ECEC0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ED198);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800ED42C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EDDFC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE210);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EE72C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EEC14);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EF0E0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EF4D0);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800EFBC4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F02B4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1364);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1594);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1638);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1A9C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1BEC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F1FF4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F289C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3414);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F3A78);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F4308);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F4D24);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F5184);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F52B4);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F59DC);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F5E1C);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F6560);

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F68AC);

void func_800F6C2C(Task* arg0)
{
    VECTOR3        vec;
    Task*          slot;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parent;

    slot  = Game_GetPtrSlot(3);
    coord = (GsCOORDINATE2*)((GameActorExt*)arg0->extra)->field_8;
    if (slot != NULL) {
        if (arg0->state == 0) {
            parent     = (GsCOORDINATE2*)((GameActorExt*)slot->extra)->field_8;
            coord->flg = 0;
            coord->sub = parent + 1;
            func_80098F58(coord);
            arg0->state = 1;
        } else if (D_80115740->field_8 >= 0) {
            if (!(((GameActorExt*)slot->extra)->field_C & 0x80)) {
                func_80098F58(coord);
                if (func_800EA1A8((VECTOR3*)coord->workm.t, &vec) != 0) {
                    func_800F68AC(&vec, 0x1C0, D_80115740->field_8);
                }
            }
        }
    }
}

INCLUDE_ASM("gameplay/nonmatchings/3E9C", func_800F6D18);
