#include "common.h"

#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_1f_guardroom_8017DA30[];

void func_shelter_1f_guardroom_8017D9CC(s32);

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_guardroom/shelter_1f_guardroom_3", func_shelter_1f_guardroom_8017D8D8);

void func_shelter_1f_guardroom_8017D9CC(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;

    cmd = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][2].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 1;
    } else {
        cmd[1].field_4 = 0;
    }
}

void func_shelter_1f_guardroom_8017DA28(void)
{
}
