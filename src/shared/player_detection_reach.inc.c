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

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    d.vx   = (u16)player->extra.tmd->coords->coord.t[0] - (u16)coord->coord.t[0];
    d.vy   = (u16)player->extra.tmd->coords->coord.t[1] - (u16)coord->coord.t[1];
    d.vz   = (u16)player->extra.tmd->coords->coord.t[2] - (u16)coord->coord.t[2];
    angle  = overlayWrapAngle(ratan2(d.vx, d.vz) - ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    x      = angle << 16;
    if (offset >= 0) {
        if (abs(x >> 16) > 0x400) {
            return 1;
        }
    } else {
        if (abs(x >> 16) < 0x400) {
            return 1;
        }
    }
    gfxReadMatrixZAxis(&coord->coord, &v);
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
