#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"

extern s32 D_dryfield_r08_80180C24;

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08_2", func_dryfield_r08_8017EB68);

void func_dryfield_r08_8017F334(s32 arg0)
{
    D_dryfield_r08_80180C24 = arg0;
}

/// Sets the skip-OT-link byte (`GpSprtCmd.field_4`) of command record
/// `arg0` + 1 in this room's sprite-table command list: non-zero leaves that
/// record's prims out of the ordering table. `arg0` is a view index below
/// 0xB; the record the table yields is larger than its `GpSprtRec` prefix,
/// so `[3].field_4` reaches the command list its tail holds there.
void func_dryfield_r08_8017F340(u8 arg0, u8 arg1)
{
    GameSessionFrom4* sess;
    GpSprtCmd*        cmd;

    sess = &gGameSession->loc;
    if ((u32)(arg0 & 0xFF) < 0xBU) {
        cmd = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1][3].field_4;
        if (arg1 & 0xFF) {
            cmd[arg0 + 1].field_4 = 1;
            return;
        }
        cmd[arg0 + 1].field_4 = 0;
    }
}

void func_dryfield_r08_8017F3B8(u8 arg0, u8 arg1)
{
    GameSessionFrom4* sess;
    GpSprtRec*        rec;
    GpSprtCmd*        cmd;

    sess = &gGameSession->loc;
    if ((u32)(arg0 & 0xFF) < 3U) {
        rec = Gp_SprtTables[sess->stage - 1]->field_0[sess->area - 1];
        if ((u32)(arg0 & 0xFF) == 0U) {
            cmd = rec[1].field_4;
        } else {
            cmd = rec[2].field_4;
        }
        if (arg1 & 0xFF) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}
