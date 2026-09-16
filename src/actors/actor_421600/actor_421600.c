#include "common.h"

#include "actors/actor_421600.h"
#include "main/gfx.h"
#include "main/task.h"

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132310);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013285C);

INCLUDE_RODATA("actors/nonmatchings/actor_421600/actor_421600", D_actor_421600_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132A00);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80132EC0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133334);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133444);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801335BC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133B30);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80133CAC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80134604);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80134AD4);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801350BC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801354D8);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80135F6C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80136138);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801366F4);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801369A0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80136C88);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801373D4);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013848C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80138750);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80138D24);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013903C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_801392A8);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013947C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_80139718);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013A404);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013A554);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013B00C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013B4C4);

void func_actor_421600_8013B8E0(Actor421600* arg0)
{
    Actor421600Work* temp_s1;
    TmdObject*       temp_a0;

    temp_s1 = arg0->field_1C;
    if (temp_s1->field_4 != 0) {
        temp_a0                      = arg0->field_2C;
        arg0->field_20->node.field_4 = 0;
        temp_a0->field_C             = 0;
        Tmd_AllocBuffers(temp_a0);
        temp_s1->field_832                  = 0x10;
        temp_s1->field_82E                  = 0x11;
        temp_s1->field_828                  = 2;
        temp_s1->field_B6C.flags           |= 0x4000;
        arg0->field_2C->field_8->coord.t[0] = 0;
        arg0->field_2C->field_8->coord.t[1] = 0;
        arg0->field_2C->field_8->coord.t[2] = 0;
        arg0->field_2C->field_8->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0, 1);
        func_actor_421600_80134604(arg0);
    }
    func_actor_421600_80134604(arg0);
    if (temp_s1->field_68 & 0x100) {
        arg0->field_2C->field_8->coord.t[0] = -0x334;
        arg0->field_2C->field_8->coord.t[1] = 0;
        arg0->field_2C->field_8->coord.t[2] = -0x4C4;
        arg0->field_2C->field_8->flg        = 0;
        Gfx_RotMatrixY(&arg0->field_2C->field_8->coord, 0x400, 1);
        temp_s1->field_828 = 2;
        temp_s1->field_82E = 0;
        func_actor_421600_80134604(arg0);
        func_actor_421600_80134604(arg0);
        temp_s1->field_0 = 0x27;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013BA70);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013C8E0);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013CD3C);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013D1DC);

INCLUDE_ASM("actors/nonmatchings/actor_421600/actor_421600", func_actor_421600_8013D658);

void func_actor_421600_8013E424(void)
{
}

INCLUDE_RODATA("actors/nonmatchings/actor_421600/actor_421600", ActorsShared80135df4Table);
