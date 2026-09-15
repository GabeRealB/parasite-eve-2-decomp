#include "common.h"
#include "main/task.h"

/* The pending-change counter and the pair of latched values it gates. The
 * counter is unsigned everywhere else in this unit (func_...80180620 and
 * func_...80180D24 read it with `lhu` while testing it with `lh`), so the
 * signed reader below is what this body needs, not a signed declaration. */
extern u16 D_neo_ark_forest_zone_80182D62;
extern s16 D_neo_ark_forest_zone_80182D66;
extern s16 D_neo_ark_forest_zone_80182D68;

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

/* Publishes the byte at 0x2 of `arg2` as `D_neo_ark_forest_zone_80182D66` only
 * while the counter is idle, and remembers the byte in `D_...80182D68` either
 * way; a change arriving while the counter runs is suppressed to zero. */
s32 func_neo_ark_forest_zone_801813C4(void* arg0, void* arg1, u8* arg2)
{
    s16 counter;

    if (arg2[2] != D_neo_ark_forest_zone_80182D68) {
        /* Assigning to a signed temp is what makes this reader `lh`; testing
           the counter in place folds the conversion into the comparison and
           reads it with `lhu` instead. */
        counter = D_neo_ark_forest_zone_80182D62;
        if (counter == 0) {
            D_neo_ark_forest_zone_80182D66 = arg2[2];
        } else {
            goto L_clear;
        }
    } else {
    L_clear:
        D_neo_ark_forest_zone_80182D66 = 0;
    }
    D_neo_ark_forest_zone_80182D68 = arg2[2];
    return 1;
}

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
