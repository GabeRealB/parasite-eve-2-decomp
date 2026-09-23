#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_forest_zone.h"

#include <psyq/libgs.h>
#include <psyq/stdio.h>

extern u16 D_801153F6;

/* The value the latch below last saw in its message's third byte. */
extern s16 D_neo_ark_forest_zone_80182D68;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_801803B4);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_801804B0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_80180620);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_80180A60);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_forest_zone/neo_ark_forest_zone_4", func_neo_ark_forest_zone_80180BB4);

void func_neo_ark_forest_zone_80180D24(Task* arg0)
{
    s16                  i;
    s16                  count;
    s32                  a;
    s32                  b;
    NeoArkForestZoneObj* obj;
    s16                  j;
    s16                  k;

    gameGetPtrSlot(3);
    if (D_neo_ark_forest_zone_80182D44[gGameSession->at4.loc.place] == 0) {
        return;
    }
    if (D_neo_ark_forest_zone_80182D62 > 0) {
        D_neo_ark_forest_zone_80182D62--;
    }
    if (D_801153F6 == 0 && D_neo_ark_forest_zone_80182DC4 > 0) {
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_forest_zone_80182E54)[k] > 0) {
                count++;
            }
        }
        printf(D_neo_ark_forest_zone_8017D5F8, b - count);
        a     = GameFlag_GetNibble(0x167);
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_forest_zone_80182E54)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x167, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_forest_zone_80182E54)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x10A, count);
        Gp_SyncAreaKeyIndex(&gGameSession->at4);
        D_neo_ark_forest_zone_80182D62 = 0x96;
    }
    D_neo_ark_forest_zone_80182DC4 = Gp_StateF0.field_6;
    if (gGameSession->field_126 == 1 && D_neo_ark_forest_zone_80182D62 == 0) {
        Gp_StateF0.field_0      = 0;
        Gp_StateF0.field_5      = 0;
        Gp_StateF0.field_6      = 0;
        Gp_StateF0.field_8      = 0;
        Gp_StateF0.field_C      = 0;
        Gp_StateF0.field_10     = 0;
        gGameSession->field_126 = 0;
    }
    if (Gp_StateF0.field_0 != 2 && D_neo_ark_forest_zone_80182D66 != 0) {
        D_neo_ark_forest_zone_80182E44.field_0 = 5;
        D_neo_ark_forest_zone_80182E44.field_1 = 0xB;
        D_neo_ark_forest_zone_80182E44.field_2 = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = ((Task*)Gp_LookupSlot4(i))->spawnArg2;
            if (obj == NULL) {
                break;
            }
            if ((s16)obj->field_40 == -999) {
                for (j = 0; j < D_neo_ark_forest_zone_80182D64; j++) {
                    if (((s16*)D_neo_ark_forest_zone_80182E54)[j] > 0) {
                        obj->field_40                     = D_neo_ark_forest_zone_80182E54[j];
                        obj->field_4C                     = 0;
                        D_neo_ark_forest_zone_80182E54[j] = 0;
                        break;
                    }
                }
                if ((s16)obj->field_40 > 0) {
                    Gp_IncStateF0Ref(0);
                    D_neo_ark_forest_zone_80182D62 += 0x5A;
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(i), 0x7DB, (s32)&D_neo_ark_forest_zone_80182E44, 0);
                    switch ((s16)(D_neo_ark_forest_zone_80182D66 - 1)) {
                        case 0:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[0].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[0].z;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg        = 0;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[0].rotY, 1);
                            break;
                        case 1:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[1].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[1].z;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg        = 0;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[1].rotY, 1);
                            break;
                        case 2:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[2].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[2].z;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[2].rotY, 1);
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg = 0;
                            break;
                        case 3:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[3].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[3].z;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[3].rotY, 1);
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg = 0;
                            break;
                        case 4:
                        default:
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[4].x;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[4].z;
                            Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[4].rotY, 1);
                            ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg = 0;
                            break;
                    }
                }
                break;
            }
        }
    }
    D_neo_ark_forest_zone_80182D66 = 0;
}

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

/* The same latch as func_neo_ark_forest_zone_801813C4 directly above, emitted a
 * second time at 0x801814B0 - two objects in the overlay, so shared code
 * cannot cover it. */
s32 func_neo_ark_forest_zone_801814B0(void* arg0, void* arg1, u8* arg2)
{
    s16 counter;

    if (arg2[2] != D_neo_ark_forest_zone_80182D68) {
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
    taskKill,
} };

void func_neo_ark_forest_zone_8018151C(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D634;
    sp.funcs[task->state](task);
}
