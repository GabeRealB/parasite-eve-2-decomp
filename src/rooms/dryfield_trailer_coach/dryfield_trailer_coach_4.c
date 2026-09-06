#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_801827F8);

void func_dryfield_trailer_coach_80182850(void)
{
    s32 cond;

    cond  = GameFlag_GetNibble(0x28) >= 2;
    cond += 1;
    Gp_StartCapSlot(3, 0, cond);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_80182888);

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_8018291C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_trailer_coach/dryfield_trailer_coach_4", func_dryfield_trailer_coach_80182950);
