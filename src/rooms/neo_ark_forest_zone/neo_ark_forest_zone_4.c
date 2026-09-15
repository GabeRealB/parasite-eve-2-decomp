#include "common.h"
#include "main/task.h"
extern u16 D_neo_ark_forest_zone_80182D62;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_801803B4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_801804B0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_80180620);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_80180A60);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_80180BB4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_80180D24);

s32 func_neo_ark_forest_zone_801813BC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_801813C4);

void func_neo_ark_forest_zone_8018141C(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/* Same stack-copied handler-table dispatch as func_neo_ark_forest_zone_8017DBBC,
 * over the second table in this overlay's leading rodata. Kept local: a shared
 * symbol can only link once per overlay, and every copy here names a different
 * table (0x8017DBBC, 0x80181430, 0x8018151C). */
extern const TaskFuncTable4 D_neo_ark_forest_zone_8017D5E8;

void func_neo_ark_forest_zone_80181430(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D5E8;
    sp.funcs[task->state](task);
}

s32 func_neo_ark_forest_zone_80181494(void)
{
    D_neo_ark_forest_zone_80182D62 += 0x5A;
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_801814B0);

void func_neo_ark_forest_zone_80181508(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

void func_neo_ark_forest_zone_80180BB4(Task* arg0);
void func_neo_ark_forest_zone_80180D24(Task* arg0);

/* The same dispatch once more, over the third table in this unit's rodata.
 * splat migrates it into `func_neo_ark_forest_zone_8018151C`'s own `.s`, so
 * there is no standalone rodata file to `INCLUDE_RODATA`; defining it here
 * emits it after the `jtbl_neo_ark_forest_zone_8017D620` carried by the
 * `func_neo_ark_forest_zone_80180D24` include above, which is where it sits. */
const TaskFuncTable4 D_neo_ark_forest_zone_8017D634 = { {
    func_neo_ark_forest_zone_80180BB4,
    func_neo_ark_forest_zone_80180D24,
    func_neo_ark_forest_zone_80181508,
    Task_Kill,
} };

void func_neo_ark_forest_zone_8018151C(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D634;
    sp.funcs[task->state](task);
}
