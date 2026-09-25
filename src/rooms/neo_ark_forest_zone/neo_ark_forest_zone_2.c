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
#include "rooms/room.h"

/// A placement for a spawned task: the x and z written into its coordinate
/// translation (y is always zero) and the Y rotation passed to
/// `Gfx_RotMatrixY`. The halfword after `x` is not read.
typedef struct NeoArkForestZoneSpawnPos {
    s16 x;
    s16 pad_2;
    s16 z;
    s16 rotY;
} NeoArkForestZoneSpawnPos;

/// The word before the room's spawn parameters, reached through this label:
/// element 2 is `D_neo_ark_forest_zone_80182D20[0]`, the spawn-slot ceiling.
extern u16 D_neo_ark_forest_zone_80182D1C[];

/// The room's spawn parameters; the first halfword is the ceiling a spawn
/// slot is clamped to.
extern u16 D_neo_ark_forest_zone_80182D20[];

/// How many spawns each session slot arms, indexed by
/// `gGameSession->at4.loc.place`, for the second and the first arming task
/// respectively; zero disables that task's work in the slot.
extern u8 D_neo_ark_forest_zone_80182D44[];
extern u8 D_neo_ark_forest_zone_80182D54[];

/// Frame countdown: counted down each frame by the arming ticks, bumped by
/// 0x5A when a spawn is handed out, and tested for zero before a new
/// placement request is accepted.
extern s16 D_neo_ark_forest_zone_80182D62;

/// How many of the pending spawn slots are armed and scanned.
extern s16 D_neo_ark_forest_zone_80182D64;

/// Placement request, one-based (zero means none), taken from a 0x13EF
/// message. Cleared every frame by the arming ticks after it has been acted
/// on.
extern s16 D_neo_ark_forest_zone_80182D66;

/// The value the 0x13EF handlers last saw in their message's third byte.
extern s16 D_neo_ark_forest_zone_80182D68;

/// Set when a spawn slot was filled while the `Gp_StateF0` reference could
/// not yet be released; the first arming task's tick releases it later.
extern s16 D_neo_ark_forest_zone_80182D6A;

/// Message tables the two arming tasks install in `Task::msgTable`.
extern GpMsgEntry D_neo_ark_forest_zone_80182D6C[];
extern GpMsgEntry D_neo_ark_forest_zone_80182DC8[];

/// Spawn placements of the first and the second arming task, indexed by the
/// placement request minus one.
extern NeoArkForestZoneSpawnPos D_neo_ark_forest_zone_80182D8C[];
extern NeoArkForestZoneSpawnPos D_neo_ark_forest_zone_80182DE8[5];

/// `Gp_StateF0.field_6` as seen on the previous frame.
extern s16 D_neo_ark_forest_zone_80182DC4;

/// The payload the room sends with message 0x7DB.
extern GpCmdArg D_neo_ark_forest_zone_80182E44;

/// The room's five pending spawn values; a positive entry is handed to the
/// first waiting object and then cleared.
extern u16 D_neo_ark_forest_zone_80182E54[5];

void func_neo_ark_forest_zone_801804B0(Task* task);
void func_neo_ark_forest_zone_80180620(Task* task);
void func_neo_ark_forest_zone_8018141C(Task* arg0);

/// State table of the first arming task, indexed by `Task::state`.
const TaskFuncTable4 D_neo_ark_forest_zone_8017D5E8 = { {
    func_neo_ark_forest_zone_801804B0,
    func_neo_ark_forest_zone_80180620,
    func_neo_ark_forest_zone_8018141C,
    taskKill,
} };

/// 0x13F4 handler of the first arming state's message table: a positive
/// `arg2` fills the first free spawn slot with 110% of it, clamped to the
/// room's ceiling, and releases the `Gp_StateF0` reference (or marks it for
/// release once that is allowed). The countdown is bumped by 0x5A unless every
/// slot was already full.
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
                if (Gp_StateF0.field_6 >= 2) {
                    Gp_ReleaseStateF0(task, 0xD);
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
/// slot to a waiting slot-4 task (one whose enemy `hp` still reads -999), sends it
/// message 0x7DB and places it at the requested point.
void func_neo_ark_forest_zone_80180620(Task* task)
{
    s16      i;
    s16      count;
    s32      a;
    s32      b;
    GpEnemy* obj;
    s16      j;
    s16      k;

    gameGetPtrSlot(3);
    if (D_neo_ark_forest_zone_80182D54[gGameSession->at4.loc.place] == 0) {
        return;
    }
    if (D_neo_ark_forest_zone_80182D62 > 0) {
        D_neo_ark_forest_zone_80182D62--;
    }
    if (D_neo_ark_forest_zone_80182D6A == 1 && Gp_StateF0.field_6 >= 2) {
        D_neo_ark_forest_zone_80182D6A = 0;
        Gp_ReleaseStateF0(task, 0xD);
    }
    if (Gp_StateF0.field_6 == 0 && D_neo_ark_forest_zone_80182DC4 > 0) {
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
        D_neo_ark_forest_zone_80182E44.from.loc.stage = 5;
        D_neo_ark_forest_zone_80182E44.from.loc.area  = 0x1D;
        D_neo_ark_forest_zone_80182E44.command        = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = ((Task*)Gp_LookupSlot4(i))->spawnArg2;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < D_neo_ark_forest_zone_80182D64; j++) {
                    if (((s16*)D_neo_ark_forest_zone_80182E54)[j] > 0) {
                        obj->hp                           = D_neo_ark_forest_zone_80182E54[j];
                        obj->reactionFlags                = 0;
                        D_neo_ark_forest_zone_80182E54[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    Gp_IncStateF0Ref(0);
                    D_neo_ark_forest_zone_80182D62 += 0x5A;
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(i), 0x7DB, (s32)&D_neo_ark_forest_zone_80182E44, 0);
                    ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182D8C[D_neo_ark_forest_zone_80182D66 - 1].x;
                    ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[1] = 0;
                    ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182D8C[D_neo_ark_forest_zone_80182D66 - 1].z;
                    ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->flg        = 0;
                    Gfx_RotMatrixY(&((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord,
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
s32 func_neo_ark_forest_zone_80180A60(Task* task, s32 arg1, GpCmdArg* msg)
{
    s32      result;
    u16      cmd;
    GpEnemy* obj;

    result = 0;
    if (msg->from.key == 0xB05) {
        cmd = msg->command;
        switch (cmd) {
            case 0:
                D_neo_ark_forest_zone_80182D62 = -1;
                result                         = 0;
                return result;
            case 2:
                D_neo_ark_forest_zone_80182E44.from.loc.stage = 5;
                D_neo_ark_forest_zone_80182E44.from.loc.area  = 0xB;
                D_neo_ark_forest_zone_80182E44.command        = 0xC;
                result                                        = 1;
                if (Gp_LookupSlot4(0) != 0) {
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB,
                                   (s32)&D_neo_ark_forest_zone_80182E44, 0);
                    obj                                                       = ((Task*)Gp_LookupSlot4(0))->spawnArg2;
                    ((Task*)Gp_LookupSlot4(0))->extra.tmd->coords->coord.t[0] = 5;
                    ((Task*)Gp_LookupSlot4(0))->extra.tmd->coords->coord.t[1] = 0;
                    ((Task*)Gp_LookupSlot4(0))->extra.tmd->coords->coord.t[2] = -0x320;
                    if (obj != 0) {
                        obj->hp                           = D_neo_ark_forest_zone_80182E54[0];
                        D_neo_ark_forest_zone_80182E54[0] = 0;
                        obj->reactionFlags                = 0;
                    }
                    Gfx_RotMatrixY(&((Task*)Gp_LookupSlot4(0))->extra.tmd->coords->coord,
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
    s16      i;
    s16      count;
    s32      a;
    s32      b;
    GpEnemy* obj;
    s16      j;
    s16      k;

    gameGetPtrSlot(3);
    if (D_neo_ark_forest_zone_80182D44[gGameSession->at4.loc.place] == 0) {
        return;
    }
    if (D_neo_ark_forest_zone_80182D62 > 0) {
        D_neo_ark_forest_zone_80182D62--;
    }
    if (Gp_StateF0.field_6 == 0 && D_neo_ark_forest_zone_80182DC4 > 0) {
        b     = GameFlag_GetNibble(0x10A);
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)D_neo_ark_forest_zone_80182E54)[k] > 0) {
                count++;
            }
        }
        printf("(get_flag(266)-get_total()) = %d\n", b - count);
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
        D_neo_ark_forest_zone_80182E44.from.loc.stage = 5;
        D_neo_ark_forest_zone_80182E44.from.loc.area  = 0xB;
        D_neo_ark_forest_zone_80182E44.command        = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = ((Task*)Gp_LookupSlot4(i))->spawnArg2;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < D_neo_ark_forest_zone_80182D64; j++) {
                    if (((s16*)D_neo_ark_forest_zone_80182E54)[j] > 0) {
                        obj->hp                           = D_neo_ark_forest_zone_80182E54[j];
                        obj->reactionFlags                = 0;
                        D_neo_ark_forest_zone_80182E54[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    Gp_IncStateF0Ref(0);
                    D_neo_ark_forest_zone_80182D62 += 0x5A;
                    Gp_DispatchMsg((Task*)Gp_LookupSlot4(i), 0x7DB, (s32)&D_neo_ark_forest_zone_80182E44, 0);
                    switch ((s16)(D_neo_ark_forest_zone_80182D66 - 1)) {
                        case 0:
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[0].x;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[1] = 0;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[0].z;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->flg        = 0;
                            Gfx_RotMatrixY(&((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[0].rotY, 1);
                            break;
                        case 1:
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[1].x;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[1] = 0;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[1].z;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->flg        = 0;
                            Gfx_RotMatrixY(&((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[1].rotY, 1);
                            break;
                        case 2:
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[2].x;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[1] = 0;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[2].z;
                            Gfx_RotMatrixY(&((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[2].rotY, 1);
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->flg = 0;
                            break;
                        case 3:
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[3].x;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[1] = 0;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[3].z;
                            Gfx_RotMatrixY(&((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[3].rotY, 1);
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->flg = 0;
                            break;
                        case 4:
                        default:
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[0] = D_neo_ark_forest_zone_80182DE8[4].x;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[1] = 0;
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord.t[2] = D_neo_ark_forest_zone_80182DE8[4].z;
                            Gfx_RotMatrixY(&((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->coord,
                                           D_neo_ark_forest_zone_80182DE8[4].rotY, 1);
                            ((Task*)Gp_LookupSlot4(i))->extra.tmd->coords->flg = 0;
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

/// The first arming task: runs the state handler its state selects, through a
/// copy of the state table on the stack.
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

/// State table of the second arming task, indexed by `Task::state`.
const TaskFuncTable4 D_neo_ark_forest_zone_8017D634 = { {
    func_neo_ark_forest_zone_80180BB4,
    func_neo_ark_forest_zone_80180D24,
    func_neo_ark_forest_zone_80181508,
    taskKill,
} };

/// The second arming task: runs the state handler its state selects, through
/// a copy of the state table on the stack.
void func_neo_ark_forest_zone_8018151C(Task* task)
{
    TaskFuncTable4 sp;

    sp = D_neo_ark_forest_zone_8017D634;
    sp.funcs[task->state](task);
}
