#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/stdio.h>

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_forest_zone.h"

extern u16 D_801153F6;

/* The value the latch below last saw in its message's third byte. */
extern s16 D_neo_ark_forest_zone_80182D68;

/// Payload of a 0x7DB message as its handler reads it: the sender id, 0xB05
/// for the spawn messages (the bytes 0x05, 0x0B the room itself sends), then
/// the command.
typedef struct NeoArkForestZoneMsg {
    u16 field_0;
    u16 field_2;
} NeoArkForestZoneMsg;

/// The word before the room's spawn parameters, reached through this label:
/// element 2 is `D_neo_ark_forest_zone_80182D20[0]`, the spawn-slot ceiling.
extern u16 D_neo_ark_forest_zone_80182D1C[];

/// The room's spawn parameters; the first halfword is the ceiling a spawn
/// slot is clamped to.
extern u16 D_neo_ark_forest_zone_80182D20[];

/// How many spawns each session slot arms for the first arming state, indexed
/// by `gGameSession->at4.loc.place`; zero disables that state in the slot.
extern u8 D_neo_ark_forest_zone_80182D54[];

/// Set when a spawn slot was filled while the `Gp_StateF0` reference could
/// not yet be released; the first arming state's per-frame tick releases it
/// later.
extern s16 D_neo_ark_forest_zone_80182D6A;

/// Message tables the two arming states install in `Task::msgTable`.
extern GpMsgEntry D_neo_ark_forest_zone_80182D6C[];
extern GpMsgEntry D_neo_ark_forest_zone_80182DC8[];

/// Spawn placements of the first arming state, indexed by the placement
/// request minus one.
extern NeoArkForestZoneSpawnPos D_neo_ark_forest_zone_80182D8C[];

/// 0x13F4 handler of the first arming state's message table: a positive
/// `arg2` fills the first free spawn slot with 110% of it, clamped to the
/// room's ceiling, and releases the `Gp_StateF0` reference (or marks it for
/// release once that is allowed). Every message bumps the countdown by 0x5A.
void func_neo_ark_forest_zone_801803B4(Task* task, s32 arg1, s32 arg2)
{
    s16 i;
    s16 v;

    if (arg2 > 0) {
        for (i = 0; i < 5; i++) {
            if (((s16*)D_neo_ark_forest_zone_80182E54)[i] == 0) {
                v                                         = arg2 * 0x6E / 100;
                ((s16*)D_neo_ark_forest_zone_80182E54)[i] = v;
                if (D_neo_ark_forest_zone_80182D20[0] < v) {
                    ((s16*)D_neo_ark_forest_zone_80182E54)[i] = D_neo_ark_forest_zone_80182D20[0];
                }
                if (D_801153F6 >= 2) {
                    Gp_ReleaseStateF0((GpObj20E*)task, 0xD);
                } else {
                    D_neo_ark_forest_zone_80182D6A = 1;
                }
                D_neo_ark_forest_zone_80182D62 += 0x5A;
                return;
            }
        }
        return;
    }
    D_neo_ark_forest_zone_80182D62 += 0x5A;
}

/// First arming state: with no spawns to arm for the session's slot it only
/// advances; otherwise it installs its message table, folds the slot's spawn
/// count into game flag 0x10C (remembering the slot in 0x10D), caps it at five
/// and fills that many spawn slots with the room's ceiling, zeroing the rest.
void func_neo_ark_forest_zone_801804B0(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_forest_zone_80182D54[gGameSession->at4.loc.place] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable                 = D_neo_ark_forest_zone_80182D6C;
    D_neo_ark_forest_zone_80182D64 = GameFlag_GetNibble(0x10C);
    nib                            = GameFlag_GetNibble(0x10D);
    if (gGameSession->at4.loc.place != nib) {
        D_neo_ark_forest_zone_80182D64 = D_neo_ark_forest_zone_80182D64 + D_neo_ark_forest_zone_80182D54[gGameSession->at4.loc.place];
        GameFlag_SetNibble(0x10C, D_neo_ark_forest_zone_80182D64);
        GameFlag_SetNibble(0x10D, gGameSession->at4.loc.place);
    }
    if (D_neo_ark_forest_zone_80182D64 >= 6) {
        D_neo_ark_forest_zone_80182D64 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_forest_zone_80182D64) {
            D_neo_ark_forest_zone_80182E54[i] = D_neo_ark_forest_zone_80182D1C[2];
        } else {
            D_neo_ark_forest_zone_80182E54[i] = 0;
        }
    }
    D_neo_ark_forest_zone_80182D62 = 0x5A;
    task->state                    = task->state + 1;
}

/// Per-frame state after `func_neo_ark_forest_zone_801804B0`: counts the
/// countdown down, releases a pending `Gp_StateF0` reference, and once that
/// reference has dropped folds the still-pending spawn slots back into game
/// flags 0x168 and 0x10C. On a placement request it hands the first pending
/// slot to a waiting slot-4 task (one whose parameter is -999), sends it
/// message 0x7DB and places it at the requested point.
void func_neo_ark_forest_zone_80180620(Task* task)
{
    s16                  i;
    s16                  count;
    s32                  a;
    s32                  b;
    NeoArkForestZoneObj* obj;
    s16                  j;
    s16                  k;

    gameGetPtrSlot(3);
    if (D_neo_ark_forest_zone_80182D54[gGameSession->at4.loc.place] == 0) {
        return;
    }
    if (D_neo_ark_forest_zone_80182D62 > 0) {
        D_neo_ark_forest_zone_80182D62--;
    }
    if (D_neo_ark_forest_zone_80182D6A == 1 && D_801153F6 >= 2) {
        D_neo_ark_forest_zone_80182D6A = 0;
        Gp_ReleaseStateF0((GpObj20E*)task, 0xD);
    }
    if (D_801153F6 == 0 && D_neo_ark_forest_zone_80182DC4 > 0) {
        D_neo_ark_forest_zone_80182D62 = 0x96;
        a                              = GameFlag_GetNibble(0x168);
        b                              = GameFlag_GetNibble(0x10C);
        count                          = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_forest_zone_80182E54)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x168, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_forest_zone_80182E54)[k] > 0) {
                count++;
            }
        }
        GameFlag_SetNibble(0x10C, count);
        Gp_SyncAreaKeyIndex(&gGameSession->at4);
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
        D_neo_ark_forest_zone_80182E44.field_1 = 0x1D;
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
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[0] = D_neo_ark_forest_zone_80182D8C[D_neo_ark_forest_zone_80182D66 - 1].x;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[1] = 0;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord.t[2] = D_neo_ark_forest_zone_80182D8C[D_neo_ark_forest_zone_80182D66 - 1].z;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->flg        = 0;
                    Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(i))->extra)->coords->coord,
                                   D_neo_ark_forest_zone_80182D8C[D_neo_ark_forest_zone_80182D66 - 1].rotY, 1);
                }
                break;
            }
        }
    }
    D_neo_ark_forest_zone_80182D66 = 0;
}

/// 0x7DB handler of the second arming state's message table, for messages
/// from sender 0xB05: command 0 stops the countdown at -1, command 2 hands
/// spawn slot 0 to the first slot-4 task, sends it message 0x7DB and places it
/// at (5, 0, -0x320) facing 0x400, restarting the countdown. Answers 1 only
/// for command 2.
s32 func_neo_ark_forest_zone_80180A60(Task* task, s32 arg1, NeoArkForestZoneMsg* msg)
{
    s32                  result;
    u16                  cmd;
    NeoArkForestZoneObj* obj;

    result = 0;
    if (msg->field_0 == 0xB05) {
        cmd = msg->field_2;
        switch (cmd) {
            case 0:
                D_neo_ark_forest_zone_80182D62 = -1;
                result                         = 0;
                return result;
            case 2:
                D_neo_ark_forest_zone_80182E44.field_0 = 5;
                D_neo_ark_forest_zone_80182E44.field_1 = 0xB;
                D_neo_ark_forest_zone_80182E44.field_2 = 0xC;
                result                                 = 1;
                if (Gp_LookupSlot4(0) != 0) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB,
                                   (s32)&D_neo_ark_forest_zone_80182E44, 0);
                    obj                                                                 = ((Task*)Gp_LookupSlot4(0))->spawnArg2;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord.t[0] = 5;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord.t[1] = 0;
                    ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord.t[2] = -0x320;
                    if (obj != 0) {
                        obj->field_40                     = D_neo_ark_forest_zone_80182E54[0];
                        D_neo_ark_forest_zone_80182E54[0] = 0;
                        obj->field_4C                     = 0;
                    }
                    Gfx_RotMatrixY(&((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords->coord,
                                   0x400, 1);
                    D_neo_ark_forest_zone_80182D62 = 0x5A;
                }
                return result;
            default:
                return 0;
        }
    } else {
        return result;
    }
}

/// Second arming state: the same as `func_neo_ark_forest_zone_801804B0` with
/// its own gate, message table and game flags 0x10A / 0x10B.
void func_neo_ark_forest_zone_80180BB4(Task* task)
{
    s16 i;
    s16 nib;

    if (D_neo_ark_forest_zone_80182D44[gGameSession->at4.loc.place] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable                 = D_neo_ark_forest_zone_80182DC8;
    D_neo_ark_forest_zone_80182D64 = GameFlag_GetNibble(0x10A);
    nib                            = GameFlag_GetNibble(0x10B);
    if (gGameSession->at4.loc.place != nib) {
        D_neo_ark_forest_zone_80182D64 = D_neo_ark_forest_zone_80182D64 + D_neo_ark_forest_zone_80182D44[gGameSession->at4.loc.place];
        GameFlag_SetNibble(0x10A, D_neo_ark_forest_zone_80182D64);
        GameFlag_SetNibble(0x10B, gGameSession->at4.loc.place);
    }
    if (D_neo_ark_forest_zone_80182D64 >= 6) {
        D_neo_ark_forest_zone_80182D64 = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < D_neo_ark_forest_zone_80182D64) {
            D_neo_ark_forest_zone_80182E54[i] = D_neo_ark_forest_zone_80182D1C[2];
        } else {
            D_neo_ark_forest_zone_80182E54[i] = 0;
        }
    }
    D_neo_ark_forest_zone_80182D62 = 0x5A;
    task->state                    = task->state + 1;
}

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
