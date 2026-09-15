#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

#include "rooms/dryfield_breezeway.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017DEC0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E010);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E114);

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E2D4);

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
