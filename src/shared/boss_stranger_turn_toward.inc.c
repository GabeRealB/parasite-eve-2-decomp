/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Stages a point's XYZ offset from a coordinate's local translation for a bearing query.
///
/// `point` is packed signed-halfword XYZ in `originCoord`'s parent frame.
/// The translation stays signed 32-bit; differences must fit signed 32-bit
/// game-coordinate units. Rotation and the composition cache are not read.
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

/// Turns the walker toward a packed target position within its per-frame yaw limit.
///
/// `goal` and the coordinate's local translation must share the parent frame;
/// angles use 4096 units per turn. Narrows the bearing-minus-heading to a signed
/// halfword and normalizes to [-2048, 2048], retaining both half-turn endpoints.
/// Updates `turnRun` modulo 65536; all three duration tiers set `turnBonus` to
/// zero. `turnLimit` zero suppresses turning. The saved scale basis is copied
/// back and yaw is multiplied onto it, retaining translation.
/// Returns unchanged when the live save's `unknown_5C0` equals 1.
/// Requires the bearing helper's coordinate/range contract and initialized
/// scratch storage for the turn block and nested bearing/rotation blocks.
/// Borrows the read-only goal for this call; the enclosing tick dirties the cache.
static void _bossStrangerTurnToward(BossStrangerWalker* walker, const SVECTOR3* goal)
{
    enum {
        BOSS_STRANGER_TURN_FIRST_DURATION_FRAMES  = 30,
        BOSS_STRANGER_TURN_SECOND_DURATION_FRAMES = 60
    };
    BossStrangerTurnTowardScratch* scratch;
    GfxCoord*                      coord;
    s32                            normalizedTurn;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.unknown_5C0 == 1)
        return;
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerTurnTowardScratch);
    coord          = walker->coord;
    normalizedTurn = _actorAngleNormalizeYaw(_actorAngleBearingFromCoordXZ(goal, coord) -
                                             ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]));
    scratch->angle = normalizedTurn;
    if (normalizedTurn != 0)
        walker->turnRun++;
    else
        walker->turnRun = 0;
    // Extra turn allowance by how long the walker has kept turning; every
    // tier grants nothing, so the limit is always `turnLimit` alone.
    if (walker->turnRun > BOSS_STRANGER_TURN_SECOND_DURATION_FRAMES)
        walker->turnBonus = 0;
    else if (walker->turnRun > BOSS_STRANGER_TURN_FIRST_DURATION_FRAMES)
        walker->turnBonus = 0;
    else
        walker->turnBonus = 0;
    if (walker->turnLimit + walker->turnBonus < scratch->angle)
        scratch->angle = walker->turnLimit + walker->turnBonus;
    if (scratch->angle < -(walker->turnLimit + walker->turnBonus))
        scratch->angle = -(walker->turnLimit + walker->turnBonus);
    if (walker->turnLimit == 0)
        scratch->angle = 0;
    // Rebuild yaw on the saved scale basis after clamping the relative turn.
    scratch->angle += ratan2(-walker->coord->coord.m[2][0], walker->coord->coord.m[2][2]);
    memcpy(walker->coord->coord.m, walker->scaleMtx.m, sizeof(walker->scaleMtx.m));
    gfxRotMatrixY(&walker->coord->coord, scratch->angle, GRAPHICS_ROTATION_COMPOSE);
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerTurnTowardScratch);
}
