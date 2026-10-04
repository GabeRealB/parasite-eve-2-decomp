#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Places `targetPos` by the player (`gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`) for the attack the idle
/// sequence's `step` has picked. For a grab (step 3) it takes `targetYaw`
/// from the player's heading and places the target 0x5AA behind the player,
/// grid-enabling `pathProbeBody` and `spotProbeBody`. For a strike (step 4) it
/// rolls an angle from `gRandomLcgState` (anywhere while `feintBroken` is set,
/// otherwise within a quarter turn either side of the player's facing), points
/// `pathProbeCapsule.ends[0]` 2000 out along it, adds the player's heading
/// into `targetYaw` and places the target 1200 out along the result,
/// grid-enabling `pathProbeBody` alone.
void golemKnightBishopPlaceTarget(Task* arg0)
{
    u8*                             head;
    GolemKnightBishopOffsetScratch* sc;
    GolemKnightBishopWork*          work;
    GfxCoord*                       coord;
    u32                             random;
    s32                             angle;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(GolemKnightBishopOffsetScratch);
    sc                       = (GolemKnightBishopOffsetScratch*)(head - sizeof(GolemKnightBishopOffsetScratch));
    work                     = arg0->work;
    if (work->step == 3) {
        coord           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->targetYaw = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        sc->operand.vz  = -0x5AA;
        sc->operand.vx  = 0;
        sc->operand.vy  = 0;
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&sc->operand);
        gte_rtv0();
        gte_stlvnl(&sc->offset);
        work->targetPos.vx                = gPlayerStatus.coordMtx->t[0] + sc->offset.vx;
        work->targetPos.vy                = gPlayerStatus.coordMtx->t[1];
        work->targetPos.vz                = gPlayerStatus.coordMtx->t[2] + sc->offset.vz;
        work->pathProbeCapsule.ends[0].vy = -0x3E8;
        work->pathProbeCapsule.ends[0].vz = -0x7D0;
        work->pathProbeCapsule.ends[0].vx = 0;
        work->pathProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        work->spotProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    } else if (work->step == 4) {
        if (work->feintBroken != 0) {
            gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->targetYaw = (gRandomLcgState >> 16) & 0xFFF;
        } else {
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            angle           = (random >> 16) & 0x3FF;
            if (!((random >> 16) & 0x400)) {
                angle = -angle;
            }
            work->targetYaw = angle;
        }
        work->pathProbeCapsule.ends[0].vx = (u32)(rsin(work->targetYaw) * 0x7D) >> 8;
        work->pathProbeCapsule.ends[0].vy = -0x3E8;
        work->pathProbeCapsule.ends[0].vz = (u32)(rcos(work->targetYaw) * 0x7D) >> 8;
        coord                             = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->targetYaw                   = (work->targetYaw + (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF)) & 0xFFF;
        sc->operand.vx                    = (u32)(rsin(work->targetYaw) * 0x4B) >> 8;
        sc->operand.vz                    = (u32)(rcos(work->targetYaw) * 0x4B) >> 8;
        work->targetPos.vx                = gPlayerStatus.coordMtx->t[0] + sc->operand.vx;
        work->targetPos.vy                = gPlayerStatus.coordMtx->t[1];
        work->targetPos.vz                = gPlayerStatus.coordMtx->t[2] + sc->operand.vz;
        work->pathProbeBody.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
}
