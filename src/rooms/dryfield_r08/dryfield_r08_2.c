#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"

extern s32 D_dryfield_r08_80180C24;

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08_2", func_dryfield_r08_8017EB68);

void func_dryfield_r08_8017F334(s32 arg0)
{
    D_dryfield_r08_80180C24 = arg0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_r08/dryfield_r08_2", func_dryfield_r08_8017F340);

void func_dryfield_r08_8017F3B8(u8 arg0, u8 arg1)
{
    GameSessionFrom4* sess;
    GpSprtRec*        rec;
    GpSprtCmd*        cmd;

    sess = (GameSessionFrom4*)&Game_Session->field_4;
    if ((u32)(arg0 & 0xFF) < 3U) {
        rec = Gp_SprtTables[sess->field_3 - 1]->field_0[sess->field_2 - 1];
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
