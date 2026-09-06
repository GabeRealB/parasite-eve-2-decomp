#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_shelter_r49_8017D9D8[];

extern s8  D_8007218B;
extern s32 D_80133560;
extern s32 D_80133860;

s32 func_shelter_r49_8017D638(void)
{
    return 0;
}

s32 func_shelter_r49_8017D640(void)
{
    return 0;
}

void func_shelter_r49_8017D648(Task* arg0)
{
    arg0->field_24 = D_shelter_r49_8017D9D8;
    Game_SetPtrSlot(arg0, 7);
    if (D_8007218B != 9) {
        func_800E8634((s32)&D_80133560, 0, (s32)&D_80133860);
    }
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_r49/shelter_r49", D_shelter_r49_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r49/shelter_r49", RoomsShared8017d878Table);
