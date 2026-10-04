/* Part of the Odd Stranger library; see odd_stranger.h. */

/// The actor's per-frame tick, as in the Horned Stranger's `func_actor_401300_801405DC`:
/// copy the state table to the frame, advance the root coordinate and hand it
/// to `Gp_UpdateActorColor`, then run the `gSceneCombatState.actorControl` arm. Arms 1 and 2 only
/// drop the two obstacle records (2 also opening the `patrolPoints` draw to 0x80)
/// and return; arm 0 falls through into the common tail, which counts
/// `hitCooldown` down into `oddStrangerTakeHit`, carries a new
/// `state` into `prevState`/`stateEntered` (snapping the root to `grabStartPos` on a
/// 0xB/0xD transition), dispatches through the table, re-flags `hitBody`,
/// then appends the view-space position to the `bodyPosHistory` ring and publishes
/// it as the enemy's `field_1C` while the `animId` clip is 0x14/0x15.
void oddStrangerTick(Enemy* enemy, Task* actor)
{
    VECTOR                pos;
    OddStrangerStateTable states;
    OddStrangerWork*      work;
    ActorViewScratch*     scratch;
    ActorViewScratch*     head;
    s32                   state;
#if ODD_STRANGER_VARIANT == 2
    s32 stop;
    s32 index;
#endif

    work   = actor->work;
    states = gOddStrangerStates;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(actor->extra.tmd->coords);
    pos.vx = actor->extra.tmd->coords->workm.t[0];
    pos.vy = actor->extra.tmd->coords->workm.t[1];
    pos.vz = actor->extra.tmd->coords->workm.t[2];
    Gp_UpdateActorColor(enemy, &pos, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            state = work->state;
            if ((state != ODD_STRANGER_STATE_HIDDEN) && (state != ODD_STRANGER_STATE_DEATH_BURN) && (state != ODD_STRANGER_STATE_DEATH_BURST) && (state != ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ODD_STRANGER_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->state;
            if ((state != ODD_STRANGER_STATE_HIDDEN) && (state != ODD_STRANGER_STATE_DEATH_BURN) && (state != ODD_STRANGER_STATE_DEATH_BURST) && (state != ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->state;
            }
            if ((state == ODD_STRANGER_STATE_DEATH_BURST_WALK) && (work->animId == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->gridContacts);
            Gp_ClearRec18Occupied(work->hitContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->gridContacts);
            Gp_ClearRec18Occupied(work->hitContacts);
            return;
    }

    head                                   = SCRATCH_STACK_CURSOR(ActorViewScratch);
    SCRATCH_STACK_CURSOR(ActorViewScratch) = head - 1;
    scratch                                = head - 1;

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
            Gp_UpdateCoord(actor->extra.tmd->coords);
        }
        work->stateEntered = 1;
    } else {
        work->stateEntered = 0;
    }
    work->prevState = (u16)work->state;
#if ODD_STRANGER_VARIANT == 1
    states.fn[work->state](actor);

    state = work->state;
    if ((state == ODD_STRANGER_STATE_DEATH_BURN) || (state == ODD_STRANGER_STATE_HIDDEN) || (state == ODD_STRANGER_STATE_DEATH_BURST) || (state == ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
        work->hitBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
#else
    index = work->state;
    SCHED_BARRIER();
    stop = ODD_STRANGER_STATE_DEATH_BURN;
    states.fn[index](actor);
    state = work->state;
    if ((state == ODD_STRANGER_STATE_AMBUSH) || (state == stop) || (state == ODD_STRANGER_STATE_HIDDEN) || (state == ODD_STRANGER_STATE_DEATH_BURST) || (state == ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
        work->hitBody.flags  &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->hitBody.flags  |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->gridBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
#endif
    Gp_ClearRec18Occupied(work->gridContacts);
    Gp_ClearRec18Occupied(work->hitContacts);

    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->state == ODD_STRANGER_STATE_PATROL)) {
        work->state = ODD_STRANGER_STATE_ALERT;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->pos);

    work->bodyPosHistory[work->bodyPosCursor].vx = scratch->pos.vx;
    work->bodyPosHistory[work->bodyPosCursor].vy = scratch->pos.vy;
    work->bodyPosHistory[work->bodyPosCursor].vz = scratch->pos.vz;

    SCRATCH_STACK_RELEASE_BYTES(0x18);
    work->bodyPosCursor = (u16)work->bodyPosCursor + 1;
    if (work->bodyPosCursor == ARRAY_SIZE(work->bodyPosHistory)) {
        work->bodyPosCursor = 0;
    }
    if ((u32)((u16)work->animId - 0x14) < 2U) {
        enemy->bodyPos.vx = work->bodyPosHistory[work->bodyPosCursor].vx;
        enemy->bodyPos.vy = work->bodyPosHistory[work->bodyPosCursor].vy;
        enemy->bodyPos.vz = work->bodyPosHistory[work->bodyPosCursor].vz;
    } else {
        enemy->bodyPos.vx = scratch->pos.vx;
        enemy->bodyPos.vy = scratch->pos.vy;
        enemy->bodyPos.vz = scratch->pos.vz;
    }
    enemy->coord = &gGfxViewCoord;
#if ODD_STRANGER_VARIANT == 2
    TOUCH_REG(stop);
#endif
}
