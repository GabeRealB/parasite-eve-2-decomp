#include "common.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "rooms/room_common.h"

extern TaskDesc       D_80135E78;
extern GpMsgEntry     D_shelter_b6_growth_room_8017F16C[];
extern GpAreaApplyRec D_shelter_b6_growth_room_801807C8;
extern u8             D_80136110[];
extern u8             D_80136308[];

extern void func_801327A8(void);
extern void func_80132834(void);

s32 func_shelter_b6_growth_room_8017D634(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xD8) == 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_80135E78, 3, 0, 0);
        } else {
            Gp_RunCapCmd1(1);
        }
    }
    if (arg2 == 0x10) {
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xD8) == 0 ? 0x10 : 0x11, 0);
    }
    return 0;
}

s32 func_shelter_b6_growth_room_8017D6C8(s32 arg0, s32 arg1, RoomEventMsg* arg2)
{
    if (arg2->field_2 == 1) {
        func_801327A8();
    }
    if (arg2->field_2 == 2) {
        func_80132834();
    }
    return 0;
}

void func_shelter_b6_growth_room_8017D71C(Task* arg0)
{
    arg0->msgTable = D_shelter_b6_growth_room_8017F16C;
    Game_SetPtrSlot(arg0, 7);
    Gp_FillAllyHp();
    Gp_ApplyAreaRecs(&D_shelter_b6_growth_room_801807C8);
    func_800E8634((s32)D_80136110, 0, (s32)D_80136308);
    Task_SpawnFromTable(&D_80135E78, 1, 0, 0);
    Task_SpawnFromTable(&D_80135E78, 2, 0, 0);
    func_800E3FAC(0xA2, 0x33);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_b6_growth_room_8017D7CC(void)
{
}
INCLUDE_RODATA("rooms/nonmatchings/shelter_b6_growth_room/shelter_b6_growth_room", D_shelter_b6_growth_room_8017D5C4);
