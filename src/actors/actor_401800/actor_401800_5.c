#include "common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include "psyq/abs.h"

#include "actors/actor_401800.h"
#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

/// Returns nonzero while the actor at `coord` should keep closing on the
/// slot-3 player. With `arg2 >= 0` that is when the player is more than 0x400
/// off the actor's facing; with `arg2 < 0`, when the player is within 0x400
/// of it. Otherwise the point `arg2` units along the facing is taken and the
/// result is whether it is still at least `arg1 + 0x96` from the player.
s32 func_actor_401800_80133558(GsCOORDINATE2* coord, s16 arg1, s16 arg2)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = gameGetPtrSlot(3);
    d.vx   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_18 - ((GpCoordXZ*)coord)->field_18;
    d.vy   = (u16)((TmdObject*)player->extra)->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = ((GpCoordXZ*)((TmdObject*)player->extra)->coords)->field_20 - ((GpCoordXZ*)coord)->field_20;
    angle  = ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    if (angle < 0) {
    loop_neg:
        if (angle < -0x800) {
            angle += 0x1000;
            goto loop_neg;
        }
    } else {
    loop_pos:
        if (angle > 0x800) {
            angle -= 0x1000;
            goto loop_pos;
        }
    }
    x = angle << 16;
    if (arg2 >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    Gfx_MatrixCol2(&coord->coord, &v);
    pv = &v;
    VectorNormalSS(pv, pv);
    gte_lddp(arg2);
    gte_ldsv(pv);
    gte_gpf12();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = ((TmdObject*)player->extra)->coords->coord.t[0] - v.vx;
    e.vy  = ((TmdObject*)player->extra)->coords->coord.t[1] - v.vy;
    e.vz  = ((TmdObject*)player->extra)->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= arg1 + 0x96;
}
