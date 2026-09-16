#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern u8         D_80115598;
extern GpMsgEntry D_dryfield_general_store_8017E188[];
extern s32        D_dryfield_general_store_8017E1B8;

INCLUDE_ASM("rooms/nonmatchings/dryfield_general_store/dryfield_general_store_3", func_dryfield_general_store_8017DDFC);

void func_dryfield_general_store_8017DEAC(Task* arg0)
{
    arg0->field_24 = D_dryfield_general_store_8017E188;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x5E) == 0) {
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&D_dryfield_general_store_8017E1B8, 0x7DB);
    } else if (GameFlag_GetNibble(0x5E) == 1) {
        GameFlag_SetNibble(0x5E, 2);
    }
    arg0->state = arg0->state + 1;
    D_80115598  = 1;
}
