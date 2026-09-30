/* Part of the player detection library; see player_detection.h. */

/// Tests the player (task slot 3) against `coord`: when the player's bearing
/// relative to the facing of `coord` is outside +/-0x400 for a non-negative
/// `offset`, or inside it for a negative one, returns 1 outright. Otherwise
/// returns whether the player stands at least `range + 0x96` from the point
/// `offset` units ahead of `coord` along its facing.
s32 detectPlayerOutOfReach(GfxCoord* coord, s16 range, s16 offset)
{
    SVECTOR  v;
    SVECTOR  d;
    VECTOR   e;
    Task*    player;
    s16      angle;
    SVECTOR* pv;
    s32      x;

    player = gameGetPtrSlot(3);
    d.vx   = (u16)player->extra.tmd->coords->coord.t[0] - (u16)coord->coord.t[0];
    d.vy   = (u16)player->extra.tmd->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = (u16)player->extra.tmd->coords->coord.t[2] - (u16)coord->coord.t[2];
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
    if (offset >= 0) {
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
    gte_lddp(offset);
    gte_ldsv(pv);
    gte_gpf12();
    gte_stsv(pv);
    v.vx += (u16)coord->coord.t[0];
    v.vy += (u16)coord->coord.t[1];
    v.vz += (u16)coord->coord.t[2];
    e.vx  = player->extra.tmd->coords->coord.t[0] - v.vx;
    e.vy  = player->extra.tmd->coords->coord.t[1] - v.vy;
    e.vz  = player->extra.tmd->coords->coord.t[2] - v.vz;
    return SquareRoot0(e.vx * e.vx + e.vy * e.vy + e.vz * e.vz) >= range + 0x96;
}
