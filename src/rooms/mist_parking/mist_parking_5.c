#include "common.h"
#include "main/gameflag.h"
#include "main/session.h"

extern u8 D_8007216D;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_5", func_mist_parking_801826E8);

void func_mist_parking_80182750(s32 arg0)
{
    if (GameFlag_GetNibble(0x7A) != 0) {
        arg0 += 2;
    }
    D_8007216D             = arg0;
    Game_Session->field_5  = arg0;
    Game_Session->field_76 = 1;
}

void func_mist_parking_801827A0(s32 arg0)
{
    Gp_SpawnIfCapIdle(arg0, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_5", func_mist_parking_801827C0);
