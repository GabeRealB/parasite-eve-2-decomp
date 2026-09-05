#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", D_shelter_b1_north_maintenance_walkway_8017D5C0);

extern GpMsgEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc   D_shelter_b1_north_maintenance_walkway_80184AAC[];

void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", func_shelter_b1_north_maintenance_walkway_8017D60C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", func_shelter_b1_north_maintenance_walkway_8017D7A4);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", func_shelter_b1_north_maintenance_walkway_8017D918);

s32 func_shelter_b1_north_maintenance_walkway_8017DA34(void)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA3C(void)
{
    return 0;
}

s32 func_shelter_b1_north_maintenance_walkway_8017DA44(void)
{
    return 0;
}

void func_shelter_b1_north_maintenance_walkway_8017DA4C(Task* arg0)
{
    arg0->field_24 = D_shelter_b1_north_maintenance_walkway_80184A84;
    Game_SetPtrSlot(arg0, 7);
    if (Game_Session->field_9 == 2) {
        Task_SpawnFromTable(D_shelter_b1_north_maintenance_walkway_80184AAC, 0, 0, 0);
        if (GameFlag_GetNibble(0x157) == 0) {
            GameFlag_SetNibble(0x157, 1);
            Gp_SpawnIfCapIdle(4, 0);
        }
    }
    func_shelter_b1_north_maintenance_walkway_8017DB54(GameFlag_GetNibble(0x84));
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b1_north_maintenance_walkway_8017DAF4(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", jtbl_shelter_b1_north_maintenance_walkway_8017D5E4);
