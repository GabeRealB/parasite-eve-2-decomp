#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"

/// Sets the skip-link flag (`GpSprtCmd::field_4`) of the third sprite command
/// of the sixth record in the current room's view entry of the first
/// `Gp_SprtTables` stage. Only low-byte values 0 and 1 change the flag.
void func_neo_ark_power_plant_2_8017FD88(s32 arg0)
{
    GameSessionFrom4* sess = (GameSessionFrom4*)&gGameSession->field_4;
    GpSprtCmd*        cmd;
    s32               mode;

    cmd  = Gp_SprtTables[sess->field_3 - 1][0].field_0[sess->field_2 - 1][5].field_4;
    mode = arg0 & 0xFF;
    if (mode == 0) {
        cmd[2].field_4 = 0;
    } else if (mode == 1) {
        cmd[2].field_4 = 1;
    }
}
