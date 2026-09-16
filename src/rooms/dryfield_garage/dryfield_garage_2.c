#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "rooms/room_common.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_garage/dryfield_garage_2", func_dryfield_garage_8017D91C);

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_garage/dryfield_garage_2", func_dryfield_garage_8017DB18);

void func_dryfield_garage_8017DC08(void)
{
}
