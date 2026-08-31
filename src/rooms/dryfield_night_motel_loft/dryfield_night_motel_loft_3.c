#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"

extern s8 D_8007272D;

extern u8      D_dryfield_night_motel_loft_8017EB78[];
extern GpObj4A D_dryfield_night_motel_loft_80180440;

void func_dryfield_night_motel_loft_8017D9BC(s32 arg0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_3", func_dryfield_night_motel_loft_8017D808);

void func_dryfield_night_motel_loft_8017D8B0(void)
{
    func_dryfield_night_motel_loft_8017D9BC(Gp_GetCurBit2Flag(0xA) == 2);
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        D_dryfield_night_motel_loft_80180440.field_4A &= 0xBF;
    }
    if (Gp_HasCollectedBit(0x117) && GameFlag_GetNibble(0x96) == 0 && Gp_LookupSlot4(0)) {
        GameFlag_SetNibble(0x96, 1);
        func_800E8614((s32)&D_dryfield_night_motel_loft_8017EB78, 0);
        func_800E3FAC(0xA2, 0x15);
        D_8007272D = 3;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_3", func_dryfield_night_motel_loft_8017D964);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_3", func_dryfield_night_motel_loft_8017D9BC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_3", func_dryfield_night_motel_loft_8017DB64);
