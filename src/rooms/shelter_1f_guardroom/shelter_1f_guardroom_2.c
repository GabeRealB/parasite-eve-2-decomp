#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_1f_guardroom_8017DA30[];
extern TaskDesc   D_shelter_1f_guardroom_8017DA60;

void func_shelter_1f_guardroom_8017D9CC(s32);

s32 func_shelter_1f_guardroom_8017D788(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 2) {
        if (GameFlag_GetNibble(0xB2) == 0) {
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_1f_guardroom_8017DA60, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(3);
        }
    }
    return 0;
}

s32 func_shelter_1f_guardroom_8017D7E8(void)
{
    return 0;
}

s32 func_shelter_1f_guardroom_8017D7F0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 3) {
        SndEvt_EnqueueType6(0x55060003, 0, 0);
    }
    return 0;
}

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
