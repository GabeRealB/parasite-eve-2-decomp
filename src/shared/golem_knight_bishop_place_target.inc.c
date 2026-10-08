#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Places and probes the next grab or strike relative to the player.
///
/// `task` owns a live GOLEM work block; idle steps 3 and 4 select grab and
/// strike, respectively. A grab is 1450 world units behind the player's heading.
/// A strike is 1200 units away, within a quarter turn of that heading unless a
/// broken feint allows a full turn. `targetYaw` uses 4096 units per turn.
/// The probe endpoint uses the relative angle before the player's yaw is added;
/// both probes are enabled for a grab, only the path probe for a strike.
/// Reserves and releases 24 scratch bytes and changes the GTE rotation state.
static void _golemKnightBishopPlaceTarget(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_TARGET_GRAB_STEP             = 3,
        GOLEM_KNIGHT_BISHOP_TARGET_STRIKE_STEP           = 4,
        GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK              = 0xFFF,
        GOLEM_KNIGHT_BISHOP_TARGET_QUARTER_TURN_MASK     = 0x3FF,
        GOLEM_KNIGHT_BISHOP_TARGET_YAW_SIGN_BIT          = 0x400,
        GOLEM_KNIGHT_BISHOP_PROBE_HEIGHT                 = -1000,
        GOLEM_KNIGHT_BISHOP_GRAB_PROBE_BACK              = -2000,
        GOLEM_KNIGHT_BISHOP_STRIKE_PROBE_DISTANCE_DIV16  = 125,
        GOLEM_KNIGHT_BISHOP_STRIKE_TARGET_DISTANCE_DIV16 = 75,
        GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS = 8,
    };
    GolemKnightBishopOffsetScratch* scratch;
    GolemKnightBishopWork*          work;
    GfxCoord*                       playerRoot;
    u32                             randomDraw;
    s32                             relativeYaw;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(GolemKnightBishopOffsetScratch);
    work    = task->work;
    if (work->step == GOLEM_KNIGHT_BISHOP_TARGET_GRAB_STEP) {
        playerRoot          = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->targetYaw     = ratan2(playerRoot->coord.m[0][2], playerRoot->coord.m[2][2]) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK;
        scratch->operand.vz = -GOLEM_KNIGHT_BISHOP_GRAB_TARGET_DISTANCE;
        scratch->operand.vx = 0;
        scratch->operand.vy = 0;
        gte_SetRotMatrix(&playerRoot->coord);
        gte_ldv0(&scratch->operand);
        gte_rtv0();
        gte_stlvnl(&scratch->offset);
        work->targetPos.vx                = gPlayerStatus.coordMtx->t[0] + scratch->offset.vx;
        work->targetPos.vy                = gPlayerStatus.coordMtx->t[1];
        work->targetPos.vz                = gPlayerStatus.coordMtx->t[2] + scratch->offset.vz;
        work->pathProbeCapsule.ends[0].vy = GOLEM_KNIGHT_BISHOP_PROBE_HEIGHT;
        work->pathProbeCapsule.ends[0].vz = GOLEM_KNIGHT_BISHOP_GRAB_PROBE_BACK;
        work->pathProbeCapsule.ends[0].vx = 0;
        work->pathProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->spotProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    } else if (work->step == GOLEM_KNIGHT_BISHOP_TARGET_STRIKE_STEP) {
        if (work->feintBroken != 0) {
            gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->targetYaw = (gRandomLcgState >> 16) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK;
        } else {
            randomDraw      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = randomDraw;
            relativeYaw     = (randomDraw >> 16) & GOLEM_KNIGHT_BISHOP_TARGET_QUARTER_TURN_MASK;
            if (!((randomDraw >> 16) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_SIGN_BIT)) {
                relativeYaw = -relativeYaw;
            }
            work->targetYaw = relativeYaw;
        }
        // Trig is Q12; distances divided by 16 and a logical shift retain the halfword result.
        work->pathProbeCapsule.ends[0].vx = (u32)(rsin(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_PROBE_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        work->pathProbeCapsule.ends[0].vy = GOLEM_KNIGHT_BISHOP_PROBE_HEIGHT;
        work->pathProbeCapsule.ends[0].vz = (u32)(rcos(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_PROBE_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        playerRoot                        = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->targetYaw                   = (work->targetYaw + (ratan2(playerRoot->coord.m[0][2], playerRoot->coord.m[2][2]) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK)) & GOLEM_KNIGHT_BISHOP_TARGET_YAW_MASK;
        scratch->operand.vx               = (u32)(rsin(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_TARGET_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        scratch->operand.vz               = (u32)(rcos(work->targetYaw) * GOLEM_KNIGHT_BISHOP_STRIKE_TARGET_DISTANCE_DIV16) >> GOLEM_KNIGHT_BISHOP_TARGET_PRODUCT_FRACTION_BITS;
        work->targetPos.vx                = gPlayerStatus.coordMtx->t[0] + scratch->operand.vx;
        work->targetPos.vy                = gPlayerStatus.coordMtx->t[1];
        work->targetPos.vz                = gPlayerStatus.coordMtx->t[2] + scratch->operand.vz;
        work->pathProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
}
