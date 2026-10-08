/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Draws the state-selected ground shadow and restores ordinary model draw flags.
///
/// Borrows the live model and its read-only work for this call. The root's
/// composed translation must be current. Ordinary states clear all model draw
/// flags; hidden and death states preserve them. Walking burst death draws only
/// during the walk clip. The square's half-side is 384 game-coordinate units;
/// the room's effect state controls shade and may suppress drawing.
static inline void _oddStrangerDrawFrameShadow(Task* actor, const OddStrangerWork* work)
{
    enum { ODD_STRANGER_GROUND_SHADOW_HALF_SIZE = 384 };
    s32 state = work->state;

    if ((state != ODD_STRANGER_STATE_HIDDEN) && (state != ODD_STRANGER_STATE_DEATH_BURN) && (state != ODD_STRANGER_STATE_DEATH_BURST) && (state != ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
        actor->extra.tmd->flags = 0;
        effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), ODD_STRANGER_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        state = work->state;
    }
    if ((state == ODD_STRANGER_STATE_DEATH_BURST_WALK) && (work->animId == ODD_STRANGER_ANIM_WALK)) {
        effectDrawGroundShadow(MATRIX_TRANS(&actor->extra.tmd->coords->workm), ODD_STRANGER_GROUND_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
    }
}

/// Updates Odd Stranger presentation, behavior and collision targeting each frame.
///
/// Requires the owning enemy and live model task with initialized work and rigs.
/// State must select a non-NULL handler within `ODD_STRANGER_STATE_COUNT`;
/// `bodyPosCursor` must index the seven-entry history. Paused or hidden actor
/// control refreshes presentation and clears contacts without advancing behavior.
/// Running actors process hits, restore the root after leaving GRAB or
/// GRAB_STRIKE, dispatch the copied state table and update collision enablement.
/// Appends part 2's world position to the history; sidestep clips target the
/// oldest entry, and other clips target the current position. Positions narrow
/// to signed halfword game units. Borrows one position scratch block plus the
/// dispatched handler's nested workspace.
static void _oddStrangerTick(Enemy* enemy, Task* actor)
{
    VECTOR                    rootPosition;
    OddStrangerStateTable     states;
    OddStrangerWork*          work;
    ActorPartPositionScratch* bodyPosition;
    ActorPartPositionScratch* savedCursor;
    s32                       state;
#if ODD_STRANGER_VARIANT == 2
    s32      deathBurnState;
    s32      dispatchState;
    TaskFunc stateHandler;
#endif

    work   = actor->work;
    states = gOddStrangerStates;

    actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(actor->extra.tmd->coords);
    rootPosition.vx = actor->extra.tmd->coords->workm.t[0];
    rootPosition.vy = actor->extra.tmd->coords->workm.t[1];
    rootPosition.vz = actor->extra.tmd->coords->workm.t[2];
    worldCoordUpdateActorColor(enemy, &rootPosition, 0, 0);

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            _oddStrangerDrawFrameShadow(actor, work);
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _oddStrangerDrawFrameShadow(actor, work);
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            worldCollisionClearContacts(work->gridContacts);
            worldCollisionClearContacts(work->hitContacts);
            return;
    }

    savedCursor                                    = SCRATCH_STACK_CURSOR(ActorPartPositionScratch);
    SCRATCH_STACK_CURSOR(ActorPartPositionScratch) = savedCursor - 1;
    bodyPosition                                   = savedCursor - 1;

    if (work->hitCooldown > 0) {
        work->hitCooldown = (s16)((u16)work->hitCooldown - 1);
    } else {
        _oddStrangerTakeHit(actor);
    }
    // Leaving a grab restores the root captured before its scripted movement.
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
    dispatchState   = work->state;
    deathBurnState  = ODD_STRANGER_STATE_DEATH_BURN;
    /* Fitted. The image reads `state` again only after the `prevState` store and
       loads the DEATH_BURN constant beside that read, before the call, in a
       call-saved register. Both hold only if the table lookup was in a later
       basic block than the read when the scheduler ran, so a branch stood
       here and was deleted after scheduling. What it tested and what its arms
       held is not known; equal arms on the dispatchState's sign leave no code. */
    if (dispatchState >= 0) {
        stateHandler = states.handlers[dispatchState];
    } else {
        stateHandler = states.handlers[dispatchState];
    }
    stateHandler(actor);
    state = work->state;
    if ((state == ODD_STRANGER_STATE_AMBUSH) || (state == deathBurnState) || (state == ODD_STRANGER_STATE_HIDDEN) || (state == ODD_STRANGER_STATE_DEATH_BURST) || (state == ODD_STRANGER_STATE_DEATH_BURST_WALK)) {
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

    bodyPosition->position.vx = 0;
    bodyPosition->position.vy = 0;
    bodyPosition->position.vz = 0;
    _actorRenderTransformToWorld(actor->extra.tmd->coords + 2, &bodyPosition->position);

    work->bodyPosHistory[work->bodyPosCursor].vx = bodyPosition->position.vx;
    work->bodyPosHistory[work->bodyPosCursor].vy = bodyPosition->position.vy;
    work->bodyPosHistory[work->bodyPosCursor].vz = bodyPosition->position.vz;

    // No further scratch allocation occurs before the sampled point is consumed.
    SCRATCH_STACK_RELEASE_BLOCK(ActorPartPositionScratch);
    work->bodyPosCursor = (u16)work->bodyPosCursor + 1;
    if (work->bodyPosCursor == ARRAY_SIZE(work->bodyPosHistory)) {
        work->bodyPosCursor = 0;
    }
    if ((u32)((u16)work->animId - ODD_STRANGER_ANIM_SIDESTEP_NEGATIVE) < 2U) {
        enemy->bodyPos.vx = work->bodyPosHistory[work->bodyPosCursor].vx;
        enemy->bodyPos.vy = work->bodyPosHistory[work->bodyPosCursor].vy;
        enemy->bodyPos.vz = work->bodyPosHistory[work->bodyPosCursor].vz;
    } else {
        enemy->bodyPos.vx = bodyPosition->position.vx;
        enemy->bodyPos.vy = bodyPosition->position.vy;
        enemy->bodyPos.vz = bodyPosition->position.vz;
    }
    enemy->coord = &gGfxViewCoord;
}
