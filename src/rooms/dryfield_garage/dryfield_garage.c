#include "common.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

INCLUDE_RODATA("rooms/nonmatchings/dryfield_garage/dryfield_garage", RoomsShared8017d878Table);

extern TaskDesc D_dryfield_garage_8017DCAC;

s32 func_dryfield_garage_8017D91C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    // Never touched, but its stack slot is load-bearing: `expand_decl` gives
    // every BLKmode local a frame slot whether or not anything reads it, and
    // MIPS_STACK_ALIGN(0x14) is what puts the saves at 0x28 and the frame at
    // 0x38. Dropping it shrinks the frame to 0x20 and the overlay stops
    // matching. See DECOMPILATION_LEARNINGS.md, "A frame 24 bytes too small is
    // a dead aggregate local".
    RoomEventReq req;
    s32          nib;

    *out = *in;
    if (in->msgId == 0x1A) {
        if (GameFlag_GetNibble(0x33) == 0) {
            if (in->field_5 == 0) {
                Task_SpawnFromTable(&D_dryfield_garage_8017DCAC, 0, 0, 0);
            }
            return 2;
        }
        if (GameFlag_GetNibble(0x2F) == 0) {
            GameFlag_SetNibble(0x2F, 1);
            GameFlag_SetNibble(0x4B, 3);
        }
    }
    if (in->msgId == 0x17) {
        if (in->field_5 == 0) {
            nib = GameFlag_GetNibble(0x47);
            if (nib == 0) {
                nib = 1;
            } else {
                nib = 2;
            }
            out->field_3 = nib;
        }
    }
    return 1;
}

s32 func_dryfield_garage_8017DA18(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x10) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0xFD) != 0 ? 0x16 : 0x10);
    }
    return 0;
}

s32 func_dryfield_garage_8017DA54(s32 arg0, s32 arg1, RoomEventMsg* msg)
{
    if ((msg->field_2 == 2) && (Game_Session->field_9 != 1)) {
        Gp_SpawnIfCapIdle(0x13, 0);
    }
}

extern TaskDesc D_80141B6C;
extern Task*    D_dryfield_garage_8018021C;

void func_dryfield_garage_8017DAA0(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_dryfield_garage_8018021C = Task_SpawnFromTable(&D_80141B6C, 1, 0, 0);
            arg0->state               += 1;
            break;
        case 1:
            Task_Kill(arg0);
            break;
    }
}

extern GpObj4A    D_dryfield_garage_8017FD1C;
extern GpMsgEntry D_dryfield_garage_8017DC7C[];
extern s32        D_dryfield_garage_8017DCC4;

void func_dryfield_garage_8017DB18(Task* arg0)
{
    arg0->field_24 = D_dryfield_garage_8017DC7C;
    Game_SetPtrSlot(arg0, 7);
    if ((Game_GetPtrSlot(0xA) != NULL) && (Game_Session->field_8 == 2)) {
        Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_garage_8017DCC4, 0);
    }
    if (GameFlag_GetNibble(0x155) == 1) {
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 2);
    }
    if (Game_Session->field_9 != 1) {
        D_dryfield_garage_8017FD1C.field_4A &= 0xBF;
    }
    arg0->state = arg0->state + 1;
}

void func_dryfield_garage_8017DC08(void)
{
}
