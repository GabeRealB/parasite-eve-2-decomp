#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
extern GpMsgEntry D_shelter_b1_north_maintenance_walkway_80184A84[];
extern TaskDesc   D_shelter_b1_north_maintenance_walkway_80184AAC[];
extern u8         D_80071075;

void func_shelter_b1_north_maintenance_walkway_8017DB54(u8 arg0);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", func_shelter_b1_north_maintenance_walkway_8017D60C);

INCLUDE_ASM("rooms/nonmatchings/shelter_b1_north_maintenance_walkway/shelter_b1_north_maintenance_walkway", func_shelter_b1_north_maintenance_walkway_8017D7A4);

void func_shelter_b1_north_maintenance_walkway_8017D918(Task* arg0)
{
    SVECTOR unused;

    switch (arg0->state) {
        case 0:
            if (Gp_StateF0.field_0 == 1) {
                gGameSession->flowFlags |= 0x80;
                gGameSession->flowFlags |= 0x40;
                arg0->state++;
            }
            break;
        case 1:
            if (Gp_StateF0.field_6 == 0) {
                Gp_StateF0.field_1  = 0x3C;
                arg0->killCountdown = 0x3E;
                arg0->state++;
            }
            break;
        case 2:
            if (arg0->killCountdown == 0) {
                if (D_80071075 == 0) {
                    Gp_SpawnIfCapIdle(1, 0);
                    taskKill(arg0);
                }
            } else {
                arg0->killCountdown--;
            }
            break;
    }
}

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
    arg0->msgTable = D_shelter_b1_north_maintenance_walkway_80184A84;
    Game_SetPtrSlot(arg0, 7);
    if (gGameSession->at4.loc.place == 2) {
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
