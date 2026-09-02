#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_back_street_8017F964[];
extern TaskDesc   D_dryfield_back_street_8017F98C[];

extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

INCLUDE_ASM("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", func_dryfield_back_street_8017D5D0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", func_dryfield_back_street_8017D748);

s32 func_dryfield_back_street_8017D89C(void)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8A4(void)
{
    return 0;
}

s32 func_dryfield_back_street_8017D8AC(void)
{
    return 0;
}

void func_dryfield_back_street_8017D8B4(Task* arg0)
{
    arg0->field_24 = D_dryfield_back_street_8017F964;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_back_street_8017F98C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_back_street_8017D910(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", D_dryfield_back_street_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", func_dryfield_back_street_8017D918);

void func_dryfield_back_street_8017D970(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758  = 0x60296;
        D_8011572C  = 0x60297;
        D_80115750  = 0x60298;
        arg0->state = 1;
    }
    Gp_State1C->field_A = 2;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_back_street/dryfield_back_street", func_dryfield_back_street_8017D9D0);
