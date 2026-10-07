/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Stages a point's XYZ offset from a coordinate's local translation for a bearing query.
///
/// Requires one free word-aligned scratch `VECTOR` below the initialized
/// cursor. Writes XYZ without touching `pad` and restores the cursor;
/// consume the returned block before anything reserves scratch again.
static __inline__ VECTOR* _actorAngleStageCoordPointOffset(const SVECTOR3* point, const GfxCoord* originCoord)
{
    VECTOR* scratchHead;
    VECTOR* delta;

    scratchHead                  = SCRATCH_STACK_CURSOR(VECTOR);
    delta                        = scratchHead - 1;
    delta->vx                    = point->vx - originCoord->coord.t[0];
    SCRATCH_STACK_CURSOR(VECTOR) = delta;
    delta->vy                    = point->vy - originCoord->coord.t[1];
    delta->vz                    = point->vz - originCoord->coord.t[2];
    // Consume the released block before another scratch reservation can reuse it.
    SCRATCH_STACK_CURSOR(VECTOR) = scratchHead;
    return delta;
}

/// Measures the XZ bearing from a coordinate's local translation to a target point.
///
/// `point` is a packed signed-halfword position in `originCoord`'s parent
/// frame and game-coordinate units. The local translation retains its full
/// signed 32-bit width; XYZ differences and their XZ magnitudes must fit
/// signed 32 bits. Rotation, scale, parent links and the composition cache
/// do not affect this query.
/// Inputs are only read and are not retained.
///
/// Returns the SDK's signed 32-bit angle in [-2048, 2048], with 4096 units
/// per turn: zero along +Z, positive toward +X, and zero when the XZ
/// positions coincide.
/// Requires an initialized, word-aligned scratch cursor with room for one
/// `VECTOR` (16 bytes). All three offsets are staged, although only X and Z
/// determine the angle; `pad` is untouched and the cursor is restored before
/// `ratan2` runs.
static __inline__ s32 _actorAngleBearingFromCoordXZ(const SVECTOR3* point, const GfxCoord* originCoord)
{
    VECTOR* delta;

    delta = _actorAngleStageCoordPointOffset(point, originCoord);
    return ratan2(delta->vx, delta->vz);
}

/// Turns the walker towards `pos` by at most `turnLimit` angle units a frame.
/// The wrapped relative bearing drives the consecutive-turn counter, then
/// becomes the absolute yaw the model's saved scale matrix is rebuilt around.
void bossStrangerTurnToward(BossStrangerWalker* work, SVECTOR3* pos)
{
    BossStrangerTurnTowardScratch* s;
    GfxCoord*                      coord;
    s16                            diff;
    s32                            angle;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.unknown_5C0 == 1)
        return;
    s     = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerTurnTowardScratch);
    coord = work->coord;
    diff  = _actorAngleBearingFromCoordXZ(pos, coord) -
           ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    angle    = _actorAngleNormalizeYaw(diff);
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
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerTurnTowardScratch);
}
