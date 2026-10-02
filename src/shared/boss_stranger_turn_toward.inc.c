/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Turns the walker towards `pos` by at most `turnLimit` angle units a frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes the absolute yaw the model's saved scale matrix is rebuilt around.
void bossStrangerTurnToward(BossStrangerWalker* work, SVECTOR3* pos)
{
    OverlayWalkerTurnScratch* s;
    GfxCoord*                 coord;
    u8*                       head;
    s16                       diff, t;
    s32                       angle;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.unknown_5C0 == 1)
        return;
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x1C;
    s                        = (OverlayWalkerTurnScratch*)(head - 0x1C);
    coord                    = work->coord;
    diff                     = overlayCoordBearingXZ(pos, coord) -
           ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    t = diff;
    if (diff < 0) {
    wrapUp:
        if (t < -0x800) {
            t += 0x1000;
            goto wrapUp;
        }
    } else {
    wrapDown:
        if (t > 0x800) {
            t -= 0x1000;
            goto wrapDown;
        }
    }
    angle    = t;
    s->angle = angle;
    if (angle != 0)
        work->turnRun++;
    else
        work->turnRun = 0;
    // Extra turn allowance by how long the walker has kept turning; every
    // tier grants nothing, so the limit is always `turnLimit` alone.
    if (work->turnRun > 60)
        work->turnBonus = 0;
    else if (work->turnRun > 30)
        work->turnBonus = 0;
    else
        work->turnBonus = 0;
    if (work->turnLimit + work->turnBonus < s->angle)
        s->angle = work->turnLimit + work->turnBonus;
    if (s->angle < -(work->turnLimit + work->turnBonus))
        s->angle = -(work->turnLimit + work->turnBonus);
    if (work->turnLimit == 0)
        s->angle = 0;
    s->angle += ratan2(-work->coord->coord.m[2][0], work->coord->coord.m[2][2]);
    memcpy(work->coord->coord.m, work->scaleMtx.m, sizeof(work->scaleMtx.m));
    gfxRotMatrixY(&work->coord->coord, s->angle, 0);
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}
