#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/dryfield_breezeway.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017DEC0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E010);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E114);

void func_dryfield_breezeway_8017E2D4(void)
{
    DbwWork*  work;
    DbwMsg7DA msg;

    work        = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    msg.field_0 = Game_Session->field_7;
    msg.field_1 = Game_Session->field_6;
    msg.field_2 = 2;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_breezeway_80181E28, 0);
}

void func_dryfield_breezeway_8017E350(void)
{
    Gp_ArmStateF0(1);
}

void func_dryfield_breezeway_8017E370(s16 arg0)
{
    DbwWork* work;

    work          = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    work->field_C = arg0;
    work->field_E = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E390);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E464);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E65C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E81C);
