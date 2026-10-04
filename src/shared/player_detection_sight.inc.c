/* Part of the player detection library; see player_detection.h. */

/// Rotates the slot-3 player's and this actor's raised root positions into
/// world space and returns `func_800E0308` on the pair.
s32 detectSightBlocked(Task* arg0)
{
    Task*                        player;
    PlayerDetectionSightScratch* head;
    PlayerDetectionSightScratch* block;
    SVECTOR*                     localEye;
    SVECTOR*                     v;
    SVECTOR*                     actorEye;

    player                                            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    head                                              = SCRATCH_STACK_CURSOR(PlayerDetectionSightScratch);
    localEye                                          = &head[-1].localEye;
    block                                             = head - 1;
    block->localEye.vx                                = player->extra.tmd->coords->coord.t[0];
    block->localEye.vy                                = player->extra.tmd->coords->coord.t[1] - 1000;
    SCRATCH_STACK_CURSOR(PlayerDetectionSightScratch) = block;
    block->localEye.vz                                = player->extra.tmd->coords->coord.t[2];
    actorRenderComposeCoord(&gGfxViewCoord);
    v = localEye;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(&block->playerEye);
    block->playerEye.vx += gGfxViewCoord.workm.t[0];
    block->playerEye.vy += gGfxViewCoord.workm.t[1];
    block->playerEye.vz += gGfxViewCoord.workm.t[2];

    block->localEye.vx = arg0->extra.tmd->coords->coord.t[0];
    block->localEye.vy = arg0->extra.tmd->coords->coord.t[1] - 1000;
    block->localEye.vz = arg0->extra.tmd->coords->coord.t[2];
    actorRenderComposeCoord(&gGfxViewCoord);
    actorEye = &head[-1].actorEye;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(v);
    gte_rtv0();
    gte_stsv(actorEye);
    block->actorEye.vx += gGfxViewCoord.workm.t[0];
    block->actorEye.vy += gGfxViewCoord.workm.t[1];
    block->actorEye.vz += gGfxViewCoord.workm.t[2];
    block->blocked      = func_800E0308(&block->playerEye, actorEye);
    SCRATCH_STACK_RELEASE_BLOCK(PlayerDetectionSightScratch);
    return block->blocked;
}
