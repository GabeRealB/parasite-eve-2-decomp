#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern u8         D_80115598;
extern GpMsgEntry D_dryfield_general_store_8017E188[];
extern s32        D_dryfield_general_store_8017E1B8;
extern TaskDesc   D_dryfield_general_store_8017E4C0;
extern s32        D_dryfield_general_store_8017E568;

/// Message handler on the slot-4 table that owns the store's story flag 0x5E:
/// message 1 spawns the cutscene task once the flag is still clear, message 2
/// arms the cutscene object and then both paths advance the flag.
s32 func_dryfield_general_store_8017DDFC(Task* task, s32 msgId, RoomEventMsg* arg2)
{
    switch (arg2->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x5E) == 0) {
                Task_SpawnFromTable(&D_dryfield_general_store_8017E4C0, 0, 0, 0);
                GameFlag_SetNibble(0x5E, 1);
            }
            break;
        case 2:
            if (Gp_StateF0.field_0 != 1 && GameFlag_GetNibble(0x5E) == 1) {
                func_800E8614((s32)&D_dryfield_general_store_8017E568, 1);
            }
            GameFlag_SetNibble(0x5E, 2);
            break;
    }
    return 0;
}

void func_dryfield_general_store_8017DEAC(Task* arg0)
{
    arg0->msgTable = D_dryfield_general_store_8017E188;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x5E) == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E1B8, 0x7DB);
    } else if (GameFlag_GetNibble(0x5E) == 1) {
        GameFlag_SetNibble(0x5E, 2);
    }
    arg0->state = arg0->state + 1;
    D_80115598  = 1;
}
