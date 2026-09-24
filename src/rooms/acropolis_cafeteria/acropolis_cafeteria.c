#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "rooms/acropolis_cafeteria.h"

extern u8        D_80071075;
extern s16       D_8007107A;
extern GpAreaKey D_8007216C;
extern s8        D_8007216D;
extern s16       D_80073BA0;
extern s8        D_80114C12;
extern s8        D_801153F1;
extern u8        D_801156A4;
extern s32       D_acropolis_cafeteria_80182E74;
extern s32       D_acropolis_cafeteria_801831BC;
extern s32       D_acropolis_cafeteria_8018330C;
extern s32       D_acropolis_cafeteria_801834D4;
extern s32       D_acropolis_cafeteria_80183F3C;
extern TaskDesc  D_acropolis_cafeteria_80184178;
INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", D_acropolis_cafeteria_8017D5C4);

static const char CafeteriaPlayerLabel[12] = "Player";

void func_acropolis_cafeteria_8017D6AC(void)
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
            D_801153F1                     = 3;
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
