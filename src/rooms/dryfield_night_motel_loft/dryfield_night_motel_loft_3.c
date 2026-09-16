#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/dryfield_night_motel_loft.h"

extern s8 D_8007272D;

extern u8      D_dryfield_night_motel_loft_8017EB78[];
extern GpObj4A D_dryfield_night_motel_loft_80180440;

/// The room's `GpMsgEntry` table: its 0x13F0 entry is
/// `func_dryfield_night_motel_loft_8017D67C` and its 0x13F2 entry
/// `func_dryfield_night_motel_loft_8017D6C4`.
extern GpMsgEntry D_dryfield_night_motel_loft_8017EB1C[];

/// The room's 0x7DB payload buffer.
extern DryfieldNightMotelLoftMsg7DB D_dryfield_night_motel_loft_8018092C;

void func_dryfield_night_motel_loft_8017D9BC(s32 arg0);

void func_dryfield_night_motel_loft_8017D808(Task* arg0)
{
    arg0->field_24 = D_dryfield_night_motel_loft_8017EB1C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(0) != 0 && GameFlag_GetNibble(0x96) != 0) {
        D_dryfield_night_motel_loft_8018092C.field_2 = 1;
        Gp_DispatchMsg(Gp_LookupSlot4(0), 0x7DB, (s32)&D_dryfield_night_motel_loft_8018092C, 0);
    }
    func_dryfield_night_motel_loft_8017D9BC(Gp_GetCurBit2Flag(0xA) == 2);
    arg0->state = (s32)(arg0->state + 1);
}

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

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_3", jtbl_dryfield_night_motel_loft_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_motel_loft/dryfield_night_motel_loft_3", D_dryfield_night_motel_loft_8017D5F8);
