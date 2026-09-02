#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_night_back_street_80180324[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_back_street/dryfield_night_back_street", func_dryfield_night_back_street_8017D5D8);

s32 func_dryfield_night_back_street_8017D724(void)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D72C(void)
{
    return 0;
}

s32 func_dryfield_night_back_street_8017D734(void)
{
    return 0;
}

void func_dryfield_night_back_street_8017D73C(Task* arg0)
{
    arg0->field_24 = D_dryfield_night_back_street_80180324;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_night_back_street_8017D780(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_back_street/dryfield_night_back_street", D_dryfield_night_back_street_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_back_street/dryfield_night_back_street", func_dryfield_night_back_street_8017D788);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_back_street/dryfield_night_back_street", func_dryfield_night_back_street_8017D7E0);
