#include "main/random.h"

/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Parks the actor's target position off the player (`gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)`).
/// In state 3 it takes `field_6E6` from the player's heading and places the
/// target 0x5AA behind the player, raising bit 0x4000 of `field_5BA` and
/// `field_5DA`; in state 4 it rolls an angle from `gRandomLcgState` (anywhere, or
/// within a quarter turn either side while `field_6E8` is clear), derives
/// `field_5DC` / `field_5E0` from it, adds the player's heading and places the
/// target 0x4B out along the result, raising bit 0x4000 of `field_5BA`.
void stalkerPlaceTarget(Task* arg0)
{
    u8*                       head;
    Actor402200OffsetScratch* sc;
    Actor402200Work*          work;
    GfxCoord*                 coord;
    u32                       random;
    s32                       angle;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(Actor402200OffsetScratch);
    sc                       = (Actor402200OffsetScratch*)(head - sizeof(Actor402200OffsetScratch));
    work                     = arg0->work;
    if (work->field_6CE == 3) {
        coord           = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->field_6E6 = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
        sc->in.vz       = -0x5AA;
        sc->in.vx       = 0;
        sc->in.vy       = 0;
        gte_SetRotMatrix(&coord->coord);
        gte_ldv0(&sc->in);
        gte_rtv0();
        gte_stlvnl(&sc->out);
        work->field_6A4  = gPlayerStatus.coordMtx->t[0] + sc->out.vx;
        work->field_6A8  = gPlayerStatus.coordMtx->t[1];
        work->field_6AC  = gPlayerStatus.coordMtx->t[2] + sc->out.vz;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = -0x7D0;
        work->field_5DC  = 0;
        work->field_5BA |= 0x4000;
        work->field_5DA |= 0x4000;
    } else if (work->field_6CE == 4) {
        if (work->field_6E8 != 0) {
            gRandomLcgState = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            work->field_6E6 = (gRandomLcgState >> 16) & 0xFFF;
        } else {
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            angle           = (random >> 16) & 0x3FF;
            if (!((random >> 16) & 0x400)) {
                angle = -angle;
            }
            work->field_6E6 = angle;
        }
        work->field_5DC  = (u32)(rsin(work->field_6E6) * 0x7D) >> 8;
        work->field_5DE  = -0x3E8;
        work->field_5E0  = (u32)(rcos(work->field_6E6) * 0x7D) >> 8;
        coord            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER)->extra.tmd->coords;
        work->field_6E6  = (work->field_6E6 + (ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF)) & 0xFFF;
        sc->in.vx        = (u32)(rsin(work->field_6E6) * 0x4B) >> 8;
        sc->in.vz        = (u32)(rcos(work->field_6E6) * 0x4B) >> 8;
        work->field_6A4  = gPlayerStatus.coordMtx->t[0] + sc->in.vx;
        work->field_6A8  = gPlayerStatus.coordMtx->t[1];
        work->field_6AC  = gPlayerStatus.coordMtx->t[2] + sc->in.vz;
        work->field_5BA |= 0x4000;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(Actor402200OffsetScratch));
}
