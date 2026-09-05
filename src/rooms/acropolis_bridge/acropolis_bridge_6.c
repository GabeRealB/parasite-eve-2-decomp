#include "common.h"

#include "gameplay/D4.h"

#include "main/display.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/task.h"

#include "rooms/room_common.h"

/// Slides one of three mutually exclusive bridge sprites in view 9 by
/// `(dx, dy)` and makes it the visible one. Each state owns three consecutive
/// `GpSprtElem` entries, which move together, and one of the three
/// `GpSprtCmd` slots; `Gp_LinkViewSprts` treats a nonzero `field_4` as "skip
/// OT-linking", so the selected command gets 0 and the other two get 1. A
/// state outside 0..2 moves nothing and hides all three.
void func_acropolis_bridge_8017EB4C(s32 state, s8 dx, s8 dy)
{
    GameSession*      g    = Game_Session;
    GameSessionFrom4* sess = (GameSessionFrom4*)&g->field_4;
    GpSprtRec*        rec;
    GpSprtElem*       el;
    GpSprtCmd*        cmd;
    s32               mode;

    rec  = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1];
    cmd  = rec[9].field_4;
    el   = rec[9].field_0;
    mode = state & 0xFF;

    if (mode == 0) {
        el[0].x0      += dx;
        el[0].y0      += dy;
        el[1].x0      += dx;
        el[1].y0      += dy;
        el[2].x0      += dx;
        el[2].y0      += dy;
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    } else if (mode == 1) {
        el[3].x0      += dx;
        el[3].y0      += dy;
        el[4].x0      += dx;
        el[4].y0      += dy;
        el[5].x0      += dx;
        el[5].y0      += dy;
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
        cmd[3].field_4 = 1;
    } else if (mode == 2) {
        el[6].x0      += dx;
        el[6].y0      += dy;
        el[7].x0      += dx;
        el[7].y0      += dy;
        el[8].x0      += dx;
        el[8].y0      += dy;
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 1;
        cmd[3].field_4 = 1;
    }
}
