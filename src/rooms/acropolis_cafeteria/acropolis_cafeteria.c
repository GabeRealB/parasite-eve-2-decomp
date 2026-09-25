#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/acropolis_cafeteria.h"
#include "rooms/room_common.h"

extern void Stage_RequestFromAreaTable(s32 arg0);
extern void func_807245E4(void*);
extern void func_80724608(void*, s32, s32, void*);

extern u8        D_80071075;
extern s16       D_80071076;
extern s16       D_8007107A;
extern GpAreaKey D_8007216C;
extern s8        D_8007216D;
extern s16       D_80073BA0;
extern s8        D_80114C12;
extern u8        D_801156A4;

extern GpMsgEntry     D_acropolis_cafeteria_80182AA8[];
extern TaskDesc       D_acropolis_cafeteria_80182AD8[];
extern s32            D_acropolis_cafeteria_80182D28;
extern s32            D_acropolis_cafeteria_80182DB8;
extern s32            D_acropolis_cafeteria_80182DDC;
extern s32            D_acropolis_cafeteria_80182E74;
extern s32            D_acropolis_cafeteria_801831BC;
extern s32            D_acropolis_cafeteria_8018330C;
extern s32            D_acropolis_cafeteria_801834D4;
extern s32            D_acropolis_cafeteria_8018363C;
extern s32            D_acropolis_cafeteria_80183DBC;
extern s32            D_acropolis_cafeteria_80183F3C;
extern s32            D_acropolis_cafeteria_80184164;
extern RECT           D_acropolis_cafeteria_80184168;
extern RECT           D_acropolis_cafeteria_80184170;
extern GpObj4A        D_acropolis_cafeteria_80189490[];
extern GpAreaApplyRec D_acropolis_cafeteria_8018C9D4[];
extern s32            D_acropolis_cafeteria_8018D6A0;
extern s32            D_acropolis_cafeteria_8018D6A4;
extern s32            D_acropolis_cafeteria_8018D6A8;

void func_acropolis_cafeteria_8017D6AC(Task* task);
void func_acropolis_cafeteria_8017E348(Task* task);

/// State handlers of the room task: set-up, the per-frame tick and `taskKill`.
const TaskFuncTable3 D_acropolis_cafeteria_8017D5C4 = {
    { func_acropolis_cafeteria_8017E348, func_acropolis_cafeteria_8017D6AC, taskKill },
};

static const char CafeteriaPlayerLabel[12] = "Player";

void func_acropolis_cafeteria_8017D6AC(Task* task)
{
    if (gDisplayState.field_112 != 0) {
        func_80724608(gameGetPtrSlot(3), -0x8C, -0x32, (void*)CafeteriaPlayerLabel);
        func_807245E4(gameGetPtrSlot(3));
    }
}

/// Copies the room message, selects its response, and starts capture slots 5
/// or 6 when the room's progress permits. field_5 suppresses side effects.
s32 func_acropolis_cafeteria_8017D700(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId;
    s32 responseId;

    *out = *in;
    if (in->msgId == 7 && in->field_2 == 4) {
        if (GameFlag_GetNibble(0) >= 3) {
            return 1;
        }
        if (in->field_5 == 0) {
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_StartCapSlot(5, 1, 0);
        }
        return 0;
    }
    msgId = in->msgId;
    if (msgId == 3) {
        if (GameFlag_GetNibble(0) < 2) {
            if (D_acropolis_cafeteria_80184164 == 0) {
                if (in->msgId == msgId && in->field_5 == 0) {
                    if (GameFlag_GetNibble(0) < 2) {
                        // Keep these stores separate from the later response branches.
                        if (GameFlag_GetNibble(0x21) < 2) {
                            s8 response = 1;
                            SOFT_BARRIER();
                            out->field_3 = response;
                        } else {
                            s8 response = 2;
                            SOFT_BARRIER();
                            out->field_3 = response;
                        }
                    } else {
                        out->field_3 = msgId;
                    }
                }
                return 1;
            }
            if (D_acropolis_cafeteria_80184164 == 2) {
                if (in->field_5 == 0) {
                    Gp_StartCapSlot(6, 1, 0);
                }
            }
            return 0;
        }
        if (GameFlag_GetNibble(0xE) == msgId && in->field_5 == 0) {
            GameFlag_SetNibble(0xE, 2);
        }
        responseId = in->msgId;
        if (responseId == 3 && in->field_5 == 0) {
            if (GameFlag_GetNibble(0) < 2) {
                if (GameFlag_GetNibble(0x21) < 2) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = 2;
                }
            } else {
                out->field_3 = responseId;
            }
        }
    }
    return 1;
}

/// Scripted-event task for this room, one step per `task->state`. Most states
/// advance by one; state 8 jumps to 14, states 9-13 are never reached that way
/// and idle. States 2-8 and 14 raise `blackout`, which covers the whole frame
/// with a black `TILE` linked into ordering-table slot 10. State 27 ends the
/// sequence by killing the task once the slot-3 object accepts message 0x3ED.
void func_acropolis_cafeteria_8017D8F8(Task* task)
{
    u8    param1[4];
    u8    param2[4];
    TILE* tile;
    u8    blackout;

    blackout = 0;
    switch (task->state) {
        case 0:
            gGameSession->flowFlags = 3;
            func_800E8634((s32)&D_acropolis_cafeteria_80182E74, 1, (s32)&D_acropolis_cafeteria_801831BC);
            task->state += 1;
            break;
        case 1:
            if (gGameSession->eventState != 1) {
                task->state += 1;
            }
            break;
        case 2:
            blackout                   = 1;
            gGameSession->at4.loc.room = D_8007216D = 2;
            gGameSession->roomObjsDirty             = 1;
            task->state                            += 1;
            break;
        case 3:
            blackout = 1;
            Gp_DispatchMsg(gameGetPtrSlot(4), 0x7D9, 0, 0);
            task->state += 1;
            break;
        case 4:
            blackout     = 1;
            task->state += 1;
            break;
        case 5:
            blackout = 1;
            func_800A99B4();
            task->state += 1;
            break;
        case 6:
            blackout  = 1;
            param1[2] = 0x15;
            param1[3] = 0;
            param1[0] = 0;
            param2[0] = 6;
            param2[1] = 0;
            param2[2] = 4;
            param2[3] = 6;
            CdCmd_Enqueue(0x21, param1, param2);
            task->state += 1;
            break;
        case 7:
            blackout = 1;
            if (CdCmd_IsIdle()) {
                Gp_SetAreaObjId(&D_8007216C, 2, 1);
                Gp_SyncAreaKeyIndex(&D_8007216C);
                Gp_SpawnArea(&D_8007216C);
                D_801156A4  &= ~0x40;
                task->state += 1;
            }
            break;
        case 8:
            blackout = 1;
            if (D_8007107A != -1) {
                Task_SpawnFromTable(&D_acropolis_cafeteria_80184178, 0, 0, 0);
            }
            task->state = 14;
            break;
        case 14:
            blackout = 1;
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case 15:
            func_800E8614((s32)&D_acropolis_cafeteria_80183F3C, 0);
            task->state += 1;
            break;
        case 16:
        case 20:
            if (gGameSession->eventState == 0) {
                task->state += 1;
            }
            break;
        case 18:
            if (Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7D6, 0, 0) == 0 && D_80073BA0 > 0 && D_80114C12 != 1 &&
                D_80071075 == 0) {
                Gp_MsgPlayerWeapon(0);
                task->state += 1;
            }
            break;
        case 19:
            Display_ClampField126(0);
            func_800E8634((s32)&D_acropolis_cafeteria_8018330C, 0, (s32)&D_acropolis_cafeteria_801834D4);
            task->state += 1;
            break;
        case 21:
            Gp_ReleaseStateF0Add(Gp_LookupSlot4(0), 0xA);
            gGameSession->flowFlags       |= 0x80;
            Gp_StateF0.field_1             = 3;
            D_acropolis_cafeteria_80184164 = 2;
            task->state                   += 1;
            break;
        case 17:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
            task->state += 1;
            break;
        case 27:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                func_800E3FAC(0xA2, 3);
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;
    }
    if (blackout != 0) {
        tile           = (TILE*)gGpuPrimCursor;
        gGpuPrimCursor = (void*)(tile + 1);
        SetTile(tile);
        tile->x0 = -0xA0;
        tile->y0 = -0x80;
        tile->w  = 0x140;
        tile->h  = 0x100;
        tile->r0 = 0;
        tile->g0 = 0;
        tile->b0 = 0;
        addPrim(&gGpuCurrentOt[10], tile);
    }
}

void func_acropolis_cafeteria_8017DD1C(Task* task)
{
    char pad[8];

    switch (task->state) {
        case 0:
            if (Gp_GetCurBit2Flag(3) == 1) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
                task->state = task->state + 1;
            } else {
                taskKill(task);
            }
            break;

        case 1:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_RunCapCmd1(3);
                task->state = task->state + 1;
            }
            break;

        case 2:
            if (Gp_CapBusy() == 0) {
                if (Gp_GetCurBit2Flag(3) == 1) {
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
                    task->state = task->state + 1;
                } else {
                    task->state = 6;
                }
            }
            break;

        case 3:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            }
            break;

        case 6:
            func_800E8634((s32)&D_acropolis_cafeteria_8018363C, 0, (s32)&D_acropolis_cafeteria_80183DBC);
            task->state = task->state + 1;
            break;

        case 7:
            if (gGameSession->eventState != 1) {
                task->state = task->state + 1;
            }
            break;

        case 8:
            GameFlag_SetNibble(0, 2);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 4);
            GameFlag_SetNibble(0xE, 1);
            Gp_ApplyAreaRecs(D_acropolis_cafeteria_8018C9D4);
            Mc_SaveData.sceneEvent = 4;
            func_800E3FAC(0xA2, 4);
            func_800ABFF8();
            func_800AC000();
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.at4.loc.stage = 1;
            Mc_SaveData.at4.loc.area  = 3;
            Mc_SaveData.at4.loc.warp  = 3;
            Mc_SaveData.at4.loc.room  = 3;
            D_80071076                = 1;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}

void func_acropolis_cafeteria_8017DF68(Task* task)
{
    GsCOORDINATE2* coord;

    coord = ((TmdObject*)((Task*)Gp_LookupSlot4(0))->extra)->coords;
    switch (task->state) {
        case 0:
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7D4, (s32)&D_acropolis_cafeteria_80182D28, 0);
            Gp_DispatchMsg((Task*)Gp_LookupSlot4(0), 0x7DB, (s32)&D_acropolis_cafeteria_80182DB8, 0);
            D_acropolis_cafeteria_8018D6A0 = 0;
            D_acropolis_cafeteria_8018D6A4 = -0x14;
            D_acropolis_cafeteria_8018D6A8 = -0x14;
            task->state                    = task->state + 1;
            break;

        case 1:
            coord->coord.t[0] += D_acropolis_cafeteria_8018D6A0;
            coord->coord.t[1] += D_acropolis_cafeteria_8018D6A4;
            if (coord->coord.t[1] > -0x12C) {
                coord->coord.t[1] = -0x12C;
            }
            coord->coord.t[2]              += D_acropolis_cafeteria_8018D6A8;
            coord->flg                      = 0;
            D_acropolis_cafeteria_8018D6A0  = D_acropolis_cafeteria_8018D6A0 / 2;
            D_acropolis_cafeteria_8018D6A4 += 5;
            if (D_acropolis_cafeteria_8018D6A4 > 0x14) {
                D_acropolis_cafeteria_8018D6A4 = 0x14;
            }
            D_acropolis_cafeteria_8018D6A8 = D_acropolis_cafeteria_8018D6A8 / 2;
            if (D_acropolis_cafeteria_8018D6A0 == 0 && D_acropolis_cafeteria_8018D6A8 == 0 &&
                coord->coord.t[1] == -0x12C) {
                taskKill(task);
            }
            break;
    }
}

/// Message handler that accepts its message and does nothing else.
s32 func_acropolis_cafeteria_8017E0D4(void)
{
    return 0;
}

s32 func_acropolis_cafeteria_8017E0DC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    if (arg2 == 7) {
        if (GameFlag_GetNibble(0) >= 2 || D_acropolis_cafeteria_80184164 >= 2) {
            if (Gp_GetCurBit2Flag(4) == 1 || Gp_GetCurBit2Flag(4) == 0) {
                Gp_StartCapSlot(7, 1, 0);
            }
        }
    }
    return 0;
}
/// Handler for slot-7 msg `0x13EF`: the directed action selected by `field_2`.
s32 func_acropolis_cafeteria_8017E154(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 0) {
        if (D_acropolis_cafeteria_80184164 >= 2 || GameFlag_GetNibble(0) >= 2) {
            Task_SpawnFromTable(D_acropolis_cafeteria_80182AD8, 1, 0, 0);
            return 0;
        }
    }
    if (arg2->field_2 == 2) {
        Gp_RunCapCmd1(9);
    } else if (arg2->field_2 == 3) {
        if (D_acropolis_cafeteria_80184164 == 0 && GameFlag_GetNibble(0) == 1) {
            D_acropolis_cafeteria_80184164 = 1;
            Task_SpawnFromTable(D_acropolis_cafeteria_80182AD8, 0, 0, 0);
        }
    }
    return 0;
}
s32 func_acropolis_cafeteria_8017E22C(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    switch (arg2) {
        case 0xA:
            SndEvt_EnqueueType6(0x5104000A, 0, 0);
            break;
        case 0xB:
            SndEvt_EnqueueType6(0x5104000B, 0, 0);
            break;
    }
    return 0;
}
void func_acropolis_cafeteria_8017E27C(s32 arg0)
{
    ((GameActor*)(gameGetPtrSlot(3))->work)->field_930 = arg0;
}

void func_acropolis_cafeteria_8017E2B0(void)
{
    Gp_PulseState1C80();
}

void func_acropolis_cafeteria_8017E2D0(void)
{
    MoveImage(&D_acropolis_cafeteria_80184168, 0x180, 0x100);
    MoveImage(&D_acropolis_cafeteria_80184170, 0, 0xF7);
}

void func_acropolis_cafeteria_8017E310(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}

void func_acropolis_cafeteria_8017E348(Task* task)
{
    task->msgTable = D_acropolis_cafeteria_80182AA8;
    Game_SetPtrSlot(task, 7);
    if (GameFlag_GetNibble(0) == 1) {
        Gp_MsgSlot4Chain(0, 0);
        Gp_MsgSlot4Chain(1, 1);
    } else if (GameFlag_GetNibble(0) == 2) {
        Gp_MsgSlot4Chain(0, 1);
        Gp_MsgSlot4Chain(1, 2);
        Gp_MsgSlot4Chain(2, 1);
        D_acropolis_cafeteria_80189490[0].field_4A &= 0xBF;
        Gp_DispatchMsg((Task*)Gp_LookupSlot4(2), 0x7D4, (s32)&D_acropolis_cafeteria_80182DDC, 0);
    }
    task->state = task->state + 1;
}

/// Runs the task's current state through a stack copy of the room's
/// three-entry state table.
void func_acropolis_cafeteria_8017E424(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_cafeteria_8017D5C4;
    sp.funcs[task->state](task);
}
