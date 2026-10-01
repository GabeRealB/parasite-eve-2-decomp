/* Part of the Odd Stranger library; see odd_stranger.h. */

/// The actor's per-frame tick, the 401000 twin of `func_actor_401300_801405DC`:
/// copy the state table to the frame, advance the root coordinate and hand it
/// to `Gp_UpdateActorColor`, then run the `gSceneCombatState.actorControl` arm. Arms 1 and 2 only
/// drop the two obstacle records (2 also opening the `field_C` draw to 0x80)
/// and return; arm 0 falls through into the common tail, which counts
/// `field_BE8` down into `oddStrangerTakeHit`, carries a new
/// `field_0` into `field_2`/`field_4` (snapping the root to `field_BF8` on a
/// 0xB/0xD transition), dispatches through the table, re-flags `field_8D0`,
/// then appends the view-space position to the `field_C2C` ring and publishes
/// it as the enemy's `field_1C` while the `field_89E` clip is 0x14/0x15.
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
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            state = work->field_0;
            if ((state != 0) && (state != 0x15) && (state != 0x1D) && (state != 0x21)) {
                actor->extra.tmd->flags = 0;
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
                state = work->field_0;
            }
            if ((state == 0x21) && (work->field_89E == 2)) {
                Gp_DrawEffGroundQuad(MATRIX_TRANS(&actor->extra.tmd->coords->workm), 0x180, gRoomEffectState->groundShadowShade);
            }
            Gp_ClearRec18Occupied(work->field_A30);
            Gp_ClearRec18Occupied(work->field_8F0);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            Gp_ClearRec18Occupied(work->field_A30);
            Gp_ClearRec18Occupied(work->field_8F0);
            return;
    }

    head                                   = SCRATCH_STACK_CURSOR(ActorViewScratch);
    SCRATCH_STACK_CURSOR(ActorViewScratch) = head - 1;
    scratch                                = head - 1;

    if (work->field_BE8 > 0) {
        work->field_BE8 = (s16)((u16)work->field_BE8 - 1);
    } else {
        oddStrangerTakeHit(actor);
    }
    if (work->field_2 != work->field_0) {
        if ((work->field_2 == 0xB) || (work->field_2 == 0xD)) {
            actor->extra.tmd->coords->coord.t[0]   = work->field_BF8.vx;
            actor->extra.tmd->coords->coord.t[1]   = work->field_BF8.vy;
            actor->extra.tmd->coords->coord.t[2]   = work->field_BF8.vz;
            actor->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(actor->extra.tmd->coords);
        }
        work->field_4 = 1;
    } else {
        work->field_4 = 0;
    }
    work->field_2 = (u16)work->field_0;
#if ODD_STRANGER_VARIANT == 1
    states.fn[work->field_0](actor);

    state = work->field_0;
    if ((state == 0x15) || (state == 0) || (state == 0x1D) || (state == 0x21)) {
        work->field_8D0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->field_8D0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
#else
    index = work->field_0;
    SCHED_BARRIER();
    stop = 0x15;
    states.fn[index](actor);
    state = work->field_0;
    if ((state == 0x1C) || (state == stop) || (state == 0) || (state == 0x1D) || (state == 0x21)) {
        work->field_8D0.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->field_A10.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    } else {
        work->field_8D0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
        work->field_A10.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    }
#endif
    Gp_ClearRec18Occupied(work->field_A30);
    Gp_ClearRec18Occupied(work->field_8F0);

    if ((gSceneCombatState.signals.bytes.enemyAlert == 1) && (work->field_0 == 0x18)) {
        work->field_0 = 6;
    }

    scratch->pos.vx = 0;
    scratch->pos.vy = 0;
    scratch->pos.vz = 0;
    actorTransformToView(actor->extra.tmd->coords + 2, &scratch->pos);

    work->field_C2C[work->field_C7C].vx = scratch->pos.vx;
    work->field_C2C[work->field_C7C].vy = scratch->pos.vy;
    work->field_C2C[work->field_C7C].vz = scratch->pos.vz;

    SCRATCH_STACK_RELEASE_BYTES(0x18);
    work->field_C7C = (u16)work->field_C7C + 1;
    if (work->field_C7C == 7) {
        work->field_C7C = 0;
    }
    if ((u32)((u16)work->field_89E - 0x14) < 2U) {
        enemy->bodyPos.vx = work->field_C2C[work->field_C7C].vx;
        enemy->bodyPos.vy = work->field_C2C[work->field_C7C].vy;
        enemy->bodyPos.vz = work->field_C2C[work->field_C7C].vz;
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
