#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern TaskDesc D_mine_cavern_80183CA4[];

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

s32 func_mine_cavern_8017D908(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179A04(in, out);

    if (in->msgId == 8) {
        if (GameFlag_GetNibble(0xBB) != 1) {
            if (in->field_5 != 0) {
                return 0;
            }
            Gp_SetNibbleIf(in->field_6, 2);
            if (Gp_StateF0.field_0 == 1 && gGameSession->at4.loc.place == Gp_StateF0.field_0) {
                Gp_RunCapCmd1(9);
                return 0;
            }
            Gp_RunCapCmd1(0xD);
            if (GameFlag_GetNibble(0x11A) != 0) {
                return 0;
            }
            GameFlag_SetNibble(0x11A, 1);
            return 0;
        }
        if (in->field_5 == 0 && GameFlag_GetNibble(0x11A) != 2) {
            GameFlag_SetNibble(0x11A, 2);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 0);
        }
    }

    if (in->msgId == 5) {
        if (gGameSession->at4.loc.place == 1 || gGameSession->at4.loc.place == 4) {
            if (Gp_StateF0.field_0 == 1) {
                if (in->field_5 == 0) {
                    Gp_RunCapCmd1(0xB);
                }
                return 0;
            }
        }
    }
    return 1;
}

s32 func_mine_cavern_8017DAA0(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8  temp;
    s32 flag;
    s32 cmd;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xC7) != 0) {
            return 0;
        }
        if (Gp_StateF0.field_0 == arg2) {
            temp = gGameSession->at4.loc.place;
            if (temp == arg2 || temp == 4) {
                cmd = 0xA;
                goto cap_only;
            }
        }
        flag = GameFlag_GetNibble(0xBB);
        if (flag == 1) {
            cmd = 0x11;
            goto cap_only;
        }
        flag = GameFlag_GetNibble(0xBB);
        if (flag == 3) {
            cmd = 0x12;
            goto spawn;
        }
        flag = GameFlag_GetNibble(0xBE);
        cmd  = 5;
        if (flag == 2) {
            goto cap_only;
        }
    spawn:
        Gp_RunCapCmd1(cmd);
        Task_SpawnFromTable(D_mine_cavern_80183CA4, 0, 0, 0);
        goto rest;
    cap_only:
        Gp_RunCapCmd1(cmd);
    }
rest:
    temp = gGameSession->at4.loc.place;
    if (temp == 1 || temp == 4) {
        switch (arg2) {
            case 8:
                Gp_StartCapSlot(8, 1, GameFlag_GetNibble(0xE2) & 1);
                break;
            case 14:
                Gp_StartCapSlot(0xE, 1, ((u32)GameFlag_GetNibble(0xE2) >> 1) & 1);
                break;
            case 15:
                Gp_StartCapSlot(0xF, 1, ((u32)GameFlag_GetNibble(0xE2) >> 2) & 1);
                break;
            case 16:
                Gp_StartCapSlot(0x10, 1, ((u32)GameFlag_GetNibble(0xE2) >> 3) & 1);
                break;
        }
    }
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern", D_mine_cavern_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/mine_cavern/mine_cavern", RoomsShared8017d878Table);
