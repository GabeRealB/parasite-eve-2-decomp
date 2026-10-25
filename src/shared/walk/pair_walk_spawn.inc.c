/* Part of the pair walk library; see pair_walk.h. */

// actor_450800 and actor_535700 bind PAIR_WALK_TASK_TABLE to their public
// TaskDesc[PAIR_WALK_TASK_COUNT] before including this fragment and undefine it
// afterwards. The indices are declared in actors/pair_walk.h: body at
// PAIR_WALK_TASK_BODY, carried TMD model at PAIR_WALK_TASK_CARRIED_MODEL.
// The binding is one identifier, with no arguments or captured locals.

/// Initializes the pair walker's matrices and cached three-light sample.
///
/// Use only as standalone statements in a compound scope. Arguments must be
/// side-effect-free object/pointer lvalues: model, work and root are read
/// repeatedly, and sample is a caller-owned VECTOR lvalue. XYZ
/// retains the root cache's frame, with 800 units subtracted from Y; no compose.
/// The model borrows work's light/color matrices until teardown, so work must
/// outlive its model. The sample is only borrowed during the lighting call.
/// Captures only the lighting constants declared in the spawn function.
#define PAIR_WALK_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, sample)              \
    (model)->lightMtx = &(work)->light;                                                  \
    (model)->colorMtx = &(work)->color;                                                  \
    (sample).vx       = (rootCoord)->workm.t[0];                                         \
    (sample).vy       = (rootCoord)->workm.t[1] - PAIR_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET; \
    (sample).vz       = (rootCoord)->workm.t[2];                                         \
    worldCoordSetModelLighting((model), &(sample), 0, PAIR_WALK_LIGHT_COUNT)

/// Initializes a scripted pair walker and creates its carried model child.
///
/// Allocates zeroed task-owned work, destroying the enemy on failure. Requires
/// descriptor slot 1 to spawn a live model enemy: texture offsets come from
/// the owner's placement, and reparenting transfers that task beneath the walker.
/// The untargetable root is parented to the view; work-owned lighting matrices
/// use cached XYZ with Y minus 800 before composition. Initializes the 19-slot
/// rig with borrowed clips, requests idle clip 1, installs messages/teardown,
/// updates once and advances task state 0 to 1. Enemy, placement, model and clip
/// bank must remain live as required by their borrowed animation/texture data.
static void _pairWalkSpawn(Enemy* enemy, Task* task)
{
    enum { PAIR_WALK_LIGHT_COUNT                 = 3,
           PAIR_WALK_SPAWN_LIGHT_SAMPLE_Y_OFFSET = 800 };
    VECTOR        lightingSample;
    PairWalkWork* work;
    GfxCoord*     rootCoord;
    TmdObject*    model;
    Enemy*        pairedEnemy;

    model      = task->extra.tmd;
    rootCoord  = model->coords;
    task->work = (work = memCalloc(sizeof(PairWalkWork), false));
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    task->exitCallback               = _pairWalkExit;
    rootCoord->parent                = &gGfxViewCoord;
    enemy->field_4                   = &rootCoord->coord;
    enemy->field_48                  = 0;
    enemy->node.state.parts.targeted = 0;
    enemy->node.state.parts.flags    = WORLD_TARGET_NOT_LOCKABLE;
    model->otOffset                  = 1;
    work->enemy                      = enemy;
    pairedEnemy                      = enemySpawnFromTable(PAIR_WALK_TASK_TABLE, PAIR_WALK_TASK_CARRIED_MODEL, 0, enemy);
    _actorRenderApplyPlacementTextureOffsets(pairedEnemy->task->extra.tmd, enemy);
    taskReparent(task, pairedEnemy->task);
    work->pairTask = pairedEnemy->task;
    PAIR_WALK_INITIALIZE_MODEL_LIGHTING(model, work, rootCoord, lightingSample);
    animationInitContext(&work->rig.anim, _gPairWalkAnimationSets, model,
                         work->rig.poses, work->rig.slots);
    work->st.animId = PAIR_WALK_ANIM_IDLE;
    work->st.state  = ACTOR_ENEMY_ANIM_RESET;
    task->msgTable  = _gPairWalkMessages;
    _pairWalkUpdate(task);
    task->state++;
}

#undef PAIR_WALK_INITIALIZE_MODEL_LIGHTING

#undef PAIR_WALK_TASK_TABLE

/// Consumes one attempted walk step and records the idle choice when travel ends.
///
/// Borrows live, writable `work` with a nonzero signed-halfword `st.travel`
/// count of remaining attempts. The caller counts a step even when actor
/// freezing suppresses translation. Decrements without clamping; only a result
/// of zero selects `PAIR_WALK_ANIM_IDLE` and records `PAIR_WALK_IDLE_BLEND_FRAMES`
/// whole frames for a later blend. Leaves `st.state` and the playing tracks
/// unchanged, so this records a choice without requesting an animation reseed.
static inline void _pairWalkCompleteTravelTick(PairWalkWork* work)
{
    work->st.travel--;
    if (work->st.travel == 0) {
        work->blendFrames = PAIR_WALK_IDLE_BLEND_FRAMES;
        work->st.animId   = PAIR_WALK_ANIM_IDLE;
    }
}

/// Applies animation requests or advances the walker by a 12-unit travel tick.
///
/// Requires a live TMD walker with `PairWalkWork` and animation's loaded data,
/// initialized scratch stack and GTE. BLEND captures poses and RESET restarts
/// tracks, then each enters TICK and returns without another slot tick. TICK
/// moves along local Z only while the requested clip is WALK and travel is
/// nonzero, then advances slots 1 to 18. Other states do nothing.
/// Travel counts down even when actor freezing suppresses translation. Arrival
/// records IDLE and a ten-frame blend duration but leaves TICK and the playing
/// tracks unchanged; another request is needed to apply a clip change.
static void _pairWalkUpdate(Task* task)
{
    PairWalkWork* work;
    s16           requestedAnimId;

    work = task->work;
    if (work->st.state == ACTOR_ENEMY_ANIM_BLEND) {
        _pairWalkReseedAnim(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_RESET) {
        _pairWalkResetAnim(task);
        work->st.state = ACTOR_ENEMY_ANIM_TICK;
        return;
    }
    if (work->st.state == ACTOR_ENEMY_ANIM_TICK) {
        // Keep the actor-freeze test's constant separate from the state test's.
        do {
        } while (0);
        requestedAnimId = work->st.animId;
        if (requestedAnimId == PAIR_WALK_ANIM_WALK && work->st.travel != 0) {
            _actorMovementStepModelForward(task, PAIR_WALK_MODEL_STEP_UNITS);
            // Travel counts attempted steps, including those suppressed by freezing.
            _pairWalkCompleteTravelTick(work);
        }
        _pairWalkTickAnim(task);
        return;
    }
}
