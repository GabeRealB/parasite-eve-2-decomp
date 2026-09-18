#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"

/// Sets the skip-link byte on the second sprite command of view 9 for the
/// current room. `arg0` zero skips OT-linking (`field_4` = 1); non-zero draws
/// it. No-op unless `GameSession.loc.stage` is 2.
void Room_Util16(s32 arg0)
{
    GameSession*      g;
    GameSessionFrom4* sess;
    GpSprtCmd*        cmd;

    g    = gGameSession;
    sess = &g->loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->field_74 - 1].field_0[sess->area - 1][8].field_4;
        if (!(arg0 & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}
