#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"

/// Sets the skip-link byte on the second sprite command of view 9 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.field_7` is 2.
void Room_Util16(s32 arg0)
{
    GameSession*      g;
    GameSessionFrom4* sess;
    GpSprtCmd*        cmd;

    g    = Game_Session;
    sess = (GameSessionFrom4*)&g->field_4;
    if (sess->field_3 == 2) {
        cmd = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1][8].field_4;
        if (!(arg0 & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}
