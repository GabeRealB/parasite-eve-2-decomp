#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern u8       D_80115598;
extern s32      D_acropolis_promenade_80180E74;
extern TaskDesc D_acropolis_promenade_80180EA4;
extern Task*    D_acropolis_promenade_801862D8;

s32 func_acropolis_promenade_8017D8E0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 5) {
        if (Gp_GetCurBit2Flag(0x15) != 2) {
            Gp_StartCapSlot(5, 1, 0);
        } else {
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_acropolis_promenade_8017D930(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017D938);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017D988);

void func_acropolis_promenade_8017D9E0(Task* arg0)
{
    arg0->field_24 = &D_acropolis_promenade_80180E74;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_promenade_801862D8 = Task_SpawnFromTable(&D_acropolis_promenade_80180EA4, 0, 0, 0);
    arg0->state                    = (s32)(arg0->state + 1);
    D_80115598                     = 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017DA4C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017DAA4);

void func_acropolis_promenade_8017DB48(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    if (Gp_GetViewIndex() == 5) {
        obj->field_C = 0x80;
    } else {
        obj->field_C = 0;
    }
    coord->flg = 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_promenade/acropolis_promenade_2", func_acropolis_promenade_8017DB9C);
