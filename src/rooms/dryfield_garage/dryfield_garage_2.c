#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_garage/dryfield_garage_2", func_dryfield_garage_8017D91C);

s32 func_dryfield_garage_8017DA18(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0x10) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0xFD) != 0 ? 0x16 : 0x10);
    }
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_garage/dryfield_garage_2", func_dryfield_garage_8017DA54);

INCLUDE_ASM("rooms/nonmatchings/dryfield_garage/dryfield_garage_2", func_dryfield_garage_8017DAA0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_garage/dryfield_garage_2", func_dryfield_garage_8017DB18);

void func_dryfield_garage_8017DC08(void)
{
}
