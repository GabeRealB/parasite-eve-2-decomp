#include "common.h"

#include "gameplay/268.h"
#include "main/display.h"

extern s32      D_map_akropolis_8017A9A8;
extern s32      D_map_akropolis_8017A9AC[4];
extern TaskDesc D_map_akropolis_8017AA00;

INCLUDE_ASM("mapui/nonmatchings/map_akropolis/map_akropolis", func_map_akropolis_80179C50);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179950);

INCLUDE_ASM("mapui/nonmatchings/map_akropolis/map_akropolis", func_map_akropolis_80179D78);

INCLUDE_ASM("mapui/nonmatchings/map_akropolis/map_akropolis", func_map_akropolis_80179E8C);

s32 func_map_akropolis_80179FC8(s32 arg0, s32 arg1)
{
    s32* p;
    s32  i;

    if (arg1 == 0) {
        i = 0;
        p = D_map_akropolis_8017A9AC;
        do {
            Gp_SetCollectedBit(*p++);
            i++;
        } while (i < 4);
        D_map_akropolis_8017A9A8 = 0;
    }
    Display_InitModeObj(&D_map_akropolis_8017AA00, 0, 0, 0);
    return 1;
}

s32 func_map_akropolis_8017A038(void)
{
    return D_map_akropolis_8017A9A8;
}

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_801799C0);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179A34);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179AC4);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179B28);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179B9C);

INCLUDE_RODATA("mapui/nonmatchings/map_akropolis/map_akropolis", D_map_akropolis_80179C08);
