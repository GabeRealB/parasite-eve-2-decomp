#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017d638.h"

extern s32 func_80179A04(RoomEventMsg* in, RoomEventMsg* out);

extern TaskDesc D_shelter_b1_armory_801824E8[];

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory_2", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory_2", func_shelter_b1_armory_80180214);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory_2", func_shelter_b1_armory_8018034C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_armory/shelter_b1_armory_2", func_shelter_b1_armory_80180468);

s32 func_shelter_b1_armory_801805A8(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventReq req;

    *out = *in;
    func_80179A04(in, out);
    if (in->msgId == 0xB) {
        req.field_0 = 4;
        req.field_4 = 1;
        req.field_8 = 0x540D0005;
        req.field_C = 0x540D0001;
        req.flagId  = 0xA6;
        req.itemId  = 0;
        return RoomsShared8017d638(&req, out);
    }
    if (in->msgId != 0xD) {
        return 1;
    }
    if (GameFlag_GetNibble(0xF0) != 0) {
        return 1;
    }
    if (in->field_5 == 0) {
        Gp_SetNibbleIf(in->field_6, 2);
        Gp_RunCapCmd1(0xD);
    }
    return 0;
}

s32 func_shelter_b1_armory_80180698(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 12:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xF0) == 0 ? 0xC : 0x17, 1);
            break;
        case 10:
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xF7) != 0 ? 0x10 : 0xA, 1);
            break;
    }
    return 0;
}

/// Handler for slot-7 msg `0x13EF`: the directed action (`field_2` 1) that
/// spawns the armory script.
s32 func_shelter_b1_armory_801806F8(Task* task, s32 msgId, GpMsg13EF* arg2, s32 arg3)
{
    if (arg2->field_2 == 1) {
        Gp_MsgPlayerWeapon(0);
        Task_SpawnFromTable(D_shelter_b1_armory_801824E8, 1, 0, 0);
    }
    return 0;
}
