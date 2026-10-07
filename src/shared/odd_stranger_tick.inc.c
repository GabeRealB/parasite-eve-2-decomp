/* Part of the Odd Stranger library; see odd_stranger.h. */

/// The actor's per-frame tick, as in the Horned Stranger's `func_actor_401300_801405DC`:
/// copy the state table to the frame, advance the root coordinate and hand it
/// to `worldCoordUpdateActorColor`, then run the `gSceneCombatState.actorControl` arm. Arms 1 and 2 only
/// drop the two obstacle records (2 also opening the `patrolPoints` draw to 0x80)
/// and return; arm 0 falls through into the common tail, which counts
/// `hitCooldown` down into `oddStrangerTakeHit`, carries a new
/// `state` into `prevState`/`stateEntered` (snapping the root to `grabStartPos` on a
/// 0xB/0xD transition), dispatches through the table, re-flags `hitBody`,
/// then appends the view-space position to the `bodyPosHistory` ring and publishes
/// it as the enemy's `field_1C` while the `animId` clip is 0x14/0x15.
void oddStrangerTick(Enemy* enemy, Task* actor)
{
    VECTOR                    pos;
    OddStrangerStateTable     states;
    OddStrangerWork*          work;
    ActorPartPositionScratch* scratch;
    ActorPartPositionScratch* head;
    s32                       state;
#if ODD_STRANGER_VARIANT == 2
    s32      stop;
    s32      index;
    TaskFunc handler;
#endif

    work   = actor->work;
    states = gOddStrangerStates;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->state;
            if ((state != ODD_STRANGER_STATE_HIDDEN) && (state != ODD_STRANGER_STATE_DEATH_BURN) && (state != ODD_STRANGER_STATE_DEATH_BURST) && (state != ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ODD_STRANGER_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->state;
            if ((state != ODD_STRANGER_STATE_HIDDEN) && (state != ODD_STRANGER_STATE_DEATH_BURN) && (state != ODD_STRANGER_STATE_DEATH_BURST) && (state != ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ODD_STRANGER_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            return;
    }

    head                                           = SCRATCH_STACK_CURSOR(ActorPartPositionScratch);
    SCRATCH_STACK_CURSOR(ActorPartPositionScratch) = head - 1;
    scratch                                        = head - 1;

    if (work->hitCooldown > 0) {
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else {
        oddStrangerTakeHit(actor);
    }
    if (work->prevState != work->state) {
        if ((work->prevState == ODD_STRANGER_STATE_GRAB) || (work->prevState == ODD_STRANGER_STATE_GRAB_STRIKE)) {
            actor->extra.tmd->coords->coord.t[0]   = work->grabStartPos.vx;
            actor->extra.tmd->coords->coord.t[1]   = work->grabStartPos.vy;
            actor->extra.tmd->coords->coord.t[2]   = work->grabStartPos.vz;
            actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(actor->extra.tmd->coords);
        }
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
#if ODD_STRANGER_VARIANT == 1
    work->prevState = (u16)work->state;
    states.handlers[work->state](actor);

    state = work->state;
    if ((state == ODD_STRANGER_STATE_DEATH_BURN) || (state == ODD_STRANGER_STATE_HIDDEN) || (state == ODD_STRANGER_STATE_DEATH_BURST) || (state == ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
#else
    work->prevState = (u16)work->state;
    index           = work->state;
    stop            = ODD_STRANGER_STATE_DEATH_BURN;
    /* Fitted. The image reads `state` again only after the `prevState` store and
       loads the DEATH_BURN constant beside that read, before the call, in a
       call-saved register. Both hold only if the table lookup was in a later
       basic block than the read when the scheduler ran, so a branch stood
       here and was deleted after scheduling. What it tested and what its arms
       held is not known; equal arms on the index's sign leave no code. */
    if (index >= 0) {
        handler = states.handlers[index];
    } else {
        handler = states.handlers[index];
    }
    handler(actor);
    state = work->state;
    if ((state == ODD_STRANGER_STATE_AMBUSH) || (state == stop) || (state == ODD_STRANGER_STATE_HIDDEN) || (state == ODD_STRANGER_STATE_DEATH_BURST) || (state == ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
        work->hitBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->gridBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
#endif
    worldCollisionClearContacts(work->gridContacts);
    worldCollisionClearContacts(work->hitContacts);

    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->state == ODD_STRANGER_STATE_PATROL)) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }

    scratch->position.vx = 0;
    scratch->position.vy = 0;
    scratch->position.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->position);

    work->bodyPosHistory[work->bodyPosCursor].vx = scratch->position.vx;
    work->bodyPosHistory[work->bodyPosCursor].vy = scratch->position.vy;
    work->bodyPosHistory[work->bodyPosCursor].vz = scratch->position.vz;

    SCRATCH_STACK_RELEASE_BLOCK(ActorPartPositionScratch);
    work->bodyPosCursor = (u16)work->bodyPosCursor + 1;
    if (work->bodyPosCursor == ARRAY_SIZE(work->bodyPosHistory)) {
        work->bodyPosCursor = 0;
    }
    if ((u32)((u16)work->animId - 0x14) < 2U) {
        enemy->bodyPos.vx = work->bodyPosHistory[work->bodyPosCursor].vx;
        enemy->bodyPos.vy = work->bodyPosHistory[work->bodyPosCursor].vy;
        enemy->bodyPos.vz = work->bodyPosHistory[work->bodyPosCursor].vz;
    } else {
        enemy->bodyPos.vx = scratch->position.vx;
        enemy->bodyPos.vy = scratch->position.vy;
        enemy->bodyPos.vz = scratch->position.vz;
    }
    enemy->coord = &gGfxViewCoord;
}
