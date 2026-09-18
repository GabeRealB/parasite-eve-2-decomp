#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "rooms/room_common.h"

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", D_neo_ark_garden_8017D5C0);

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

extern GpAreaApplyRec D_neo_ark_garden_80182BF8[];

extern GpMsgEntry D_neo_ark_garden_801813B0[];
extern s32        D_801334EC;
extern s32        D_80133954;

INCLUDE_ASM("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", func_neo_ark_garden_8017D64C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", func_neo_ark_garden_8017E2A0);

s32 func_neo_ark_garden_8017E840(void)
{
    return 0;
}

s32 func_neo_ark_garden_8017E848(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if (in->msgId != 0x21) {
        return 1;
    }
    if (GameFlag_GetNibble(0xDC) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(1);
    return 0;
}

s32 func_neo_ark_garden_8017E8DC(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x141) != 0 ? 6 : 4);
        if ((GameFlag_GetNibble(0xFA) == 0) && (GameFlag_GetNibble(0xDC) == 0)) {
            GameFlag_SetNibble(0xFA, 1);
            Gp_ApplyAreaRecs(D_neo_ark_garden_80182BF8);
        }
    }
    if (arg2 == 7) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 9 : 7, 0);
    }
    if (arg2 == 5) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xC7) != 0 ? 0xA : 5, 0);
    }
    return 0;
}

s32 func_neo_ark_garden_8017E9AC(void)
{
    return 0;
}

void func_neo_ark_garden_8017E9B4(Task* arg0)
{
    arg0->msgTable = D_neo_ark_garden_801813B0;
    Game_SetPtrSlot(arg0, 7);
    if (*(u16*)&gGameSession->at4.loc.warp == 0x203) {
        func_800E8634((s32)&D_801334EC, 0, (s32)&D_80133954);
        func_800E3FAC(0xA2, 0x34);
    }
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_garden/neo_ark_garden", RoomsShared8017d878Table);
