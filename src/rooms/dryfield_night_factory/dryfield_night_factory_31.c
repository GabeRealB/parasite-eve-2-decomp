#include "common.h"

#include "gameplay/D4.h"
#include "main/session.h"
#include "rooms/dryfield_night_factory.h"

void func_dryfield_night_factory_80181620(s32 show)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    if (sess->stage == 2) {
        cmd = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1][8].field_4;
        if (!(show & 0xFF)) {
            cmd[1].field_4 = 1;
            return;
        }
        cmd[1].field_4 = 0;
    }
}
