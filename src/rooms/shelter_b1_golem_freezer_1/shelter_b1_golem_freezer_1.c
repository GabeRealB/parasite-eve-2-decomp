#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern GpMsgEntry D_shelter_b1_golem_freezer_1_8017E6A8[];

extern void func_80131E70(void);
extern void func_80131E24(void);
void        func_shelter_b1_golem_freezer_1_8017D744(s32 arg0);

s32 func_shelter_b1_golem_freezer_1_8017D624(s32 arg0, s32 arg1, RoomEventMsg* msg)
{
    if (msg->field_2 == 1 && Game_Session->field_9 == 0x15) {
        func_80131E70();
    }
    return 0;
}

void func_shelter_b1_golem_freezer_1_8017D66C(Task* arg0)
{
    arg0->field_24 = D_shelter_b1_golem_freezer_1_8017E6A8;
    Game_SetPtrSlot(arg0, 7);
    if (Game_Session->field_9 == 0x15) {
        func_80131E24();
    }
    func_shelter_b1_golem_freezer_1_8017D744(0);
    arg0->state = arg0->state + 1;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1", D_shelter_b1_golem_freezer_1_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_golem_freezer_1/shelter_b1_golem_freezer_1", RoomsShared8017d878Table);
