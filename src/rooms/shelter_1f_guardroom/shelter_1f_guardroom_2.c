#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_1f_guardroom_8017DA30[];

void func_shelter_1f_guardroom_8017D9CC(s32);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_guardroom/shelter_1f_guardroom_2", func_shelter_1f_guardroom_8017D788);

s32 func_shelter_1f_guardroom_8017D7E8(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_guardroom/shelter_1f_guardroom_2", func_shelter_1f_guardroom_8017D7F0);

void func_shelter_1f_guardroom_8017D824(Task* arg0)
{
    arg0->field_24 = D_shelter_1f_guardroom_8017DA30;
    Game_SetPtrSlot(arg0, 7);
    func_shelter_1f_guardroom_8017D9CC(GameFlag_GetNibble(0xB2) & 0xFF);
    arg0->state = (s32)(arg0->state + 1);
}

void func_shelter_1f_guardroom_8017D878(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_guardroom/shelter_1f_guardroom_2", func_shelter_1f_guardroom_8017D880);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_guardroom/shelter_1f_guardroom_2", func_shelter_1f_guardroom_8017D8D8);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_guardroom/shelter_1f_guardroom_2", func_shelter_1f_guardroom_8017D9CC);

void func_shelter_1f_guardroom_8017DA28(void)
{
}
