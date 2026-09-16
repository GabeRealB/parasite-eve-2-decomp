#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

extern s8 D_80071090;
extern u8 D_8007216C;

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_801827F8);

void func_dryfield_trailer_coach_80182850(void)
{
    s32 cond;

    cond  = GameFlag_GetNibble(0x28) >= 2;
    cond += 1;
    Gp_StartCapSlot(3, 0, cond);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_80182888);

void func_dryfield_trailer_coach_8018291C(void)
{
    char pad[0x10];

    if (D_8007216C == 8) {
        D_80071090 = 0;
    } else {
        D_80071090 = 3;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_80182950);
