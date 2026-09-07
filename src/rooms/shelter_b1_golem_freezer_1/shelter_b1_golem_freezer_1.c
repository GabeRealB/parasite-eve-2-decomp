#include "common.h"
#include "main/session.h"
#include "rooms/room_common.h"

extern void func_80131E70(void);

s32 func_shelter_b1_golem_freezer_1_8017D624(s32 arg0, s32 arg1, RoomEventMsg* msg)
{
    if (msg->field_2 == 1 && Game_Session->field_9 == 0x15) {
        func_80131E70();
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1", func_shelter_b1_golem_freezer_1_8017D66C);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1", D_shelter_b1_golem_freezer_1_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1", RoomsShared8017d878Table);
