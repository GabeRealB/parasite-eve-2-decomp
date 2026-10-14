/* Part of the Skull Stalker library; see skull_stalker.h. */

/// Dispatches the Skull Stalker's spawn, active or death task state.
///
/// State must be 0..2, with the enemy in spawnArg2 and a live TMD body. Copies
/// the carrier's three callback pointers by value before dispatch; the handler
/// can destroy the task, so no task access follows the call.
static void _skullStalkerTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gSkullStalkerTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Updates active behavior, visibility, contacts, animation and model color.
///
/// Paused actor control updates only color; hidden control disables drawing and
/// lock-on and returns. Running control clears the target flags before the
/// update; other values also update. Each active tick raises the root by 128
/// parent-coordinate units, then processes behavior and the fade before pending
/// reactions and hits. Coordinate 1 is recomposed before sampling its lighting.
static void _skullStalkerUpdateState(Enemy* enemy, Task* task)
{
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _skullStalkerUpdateColor(enemy, task);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    task->extra.tmd->coords[0].coord.t[1] += 128;
    // Keep fade decisions ahead of hit/reaction changes and animation advancement.
    _skullStalkerUpdateBehavior(task);
    _skullStalkerUpdateVisibility(task);
    _skullStalkerConsumeReactions(task);
    _skullStalkerProcessContacts(task);
    _skullStalkerAnimate(task);
    // Refresh the lighting sample after playback updates the model parts.
    task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    _skullStalkerUpdateColor(enemy, task);
}

/// Consumes pending enemy reactions and enters the buildup hold when requested.
///
/// Stagger and damage-over-time requests are discarded; buildup selects the
/// status hold and clears its phase counter. Reads the byte again between
/// stages so each clear preserves the other pending bits.
static void _skullStalkerConsumeReactions(Task* task)
{
    Enemy*            enemy;
    SkullStalkerWork* work;
    u8                pendingReactions;

    enemy            = task->spawnArg2.pointer;
    work             = task->work;
    pendingReactions = enemy->reactionFlags;
    if (pendingReactions != 0) {
        if (pendingReactions & ENEMY_REACTION_STAGGER) {
            enemy->reactionFlags = pendingReactions & ENEMY_REACTION_STAGGER_CLEAR;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags = enemy->reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
            work->state          = SKULL_STALKER_STATE_STATUS_HOLD;
            work->phaseFrames    = 0;
        }
        pendingReactions = enemy->reactionFlags;
        if (pendingReactions & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            enemy->reactionFlags = pendingReactions & ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
        }
    }
}

/// Runs the idle behavior or holds the enemy visible during status buildup.
///
/// The status hold resets animation bookkeeping every four ticks and returns
/// to idle when buildup ends. The inert behavior runs nothing; unknown state
/// values also leave the work unchanged. Requires initialized work and enemy.
static void _skullStalkerUpdateBehavior(Task* task)
{
    enum { SKULL_STALKER_STATUS_RESET_TICKS = 4 };
    SkullStalkerWork* work;

    work = task->work;
    switch (work->state) {
        case SKULL_STALKER_STATE_IDLE:
            _skullStalkerIdleTick(task);
            break;
        case SKULL_STALKER_STATE_INERT:
            break;
        case SKULL_STALKER_STATE_STATUS_HOLD:
            work->field_292   = 0;
            work->hiding      = 0;
            work->phaseFrames = work->phaseFrames + 1;
            if (work->phaseFrames >= SKULL_STALKER_STATUS_RESET_TICKS) {
                work->appliedAnim = SKULL_STALKER_ANIM_IDLE;
                work->animFrames  = 0;
                work->phaseFrames = 0;
            }
            if (damageTickEnemyBuildup(task->spawnArg2.pointer) != 0) {
                work->state = SKULL_STALKER_STATE_IDLE;
            }
            break;
    }
}

/// Applies the requested animation or advances the two animated model parts.
///
/// This out-of-line entry shares its playback operation with the death state.
/// Requires initialized work and the live three-part TMD model.
static void _skullStalkerAnimate(Task* task)
{
    _skullStalkerTickAnimation(task);
}

/// Updates the model's lighting and color blend at its second part's composed origin.
///
/// The live enemy must own the task; coordinate 1 must already be composed.
/// Reserves one VECTOR on the scratch stack for the XYZ sample in game units;
/// the lighting query takes additional scratch and retains no sample pointer.
static void _skullStalkerUpdateColor(Enemy* enemy, Task* task)
{
    GfxCoord* sampleCoord;
    void**    cursorSlot;
    VECTOR*   scratchHead;
    VECTOR*   worldPosition;

    sampleCoord                                 = &task->extra.tmd->coords[1];
    cursorSlot                                  = SCRATCH_STACK_CURSOR_SLOT;
    scratchHead                                 = SCRATCH_STACK_CURSOR_AT(cursorSlot, VECTOR);
    worldPosition                               = scratchHead - 1;
    worldPosition->vx                           = sampleCoord->workm.t[0];
    worldPosition->vy                           = sampleCoord->workm.t[1];
    worldPosition->vz                           = sampleCoord->workm.t[2];
    SCRATCH_STACK_CURSOR_AT(cursorSlot, VECTOR) = worldPosition;
    worldCoordUpdateActorColor(enemy, worldPosition, 0, 0);
    SCRATCH_STACK_RELEASE_BYTES_AT(cursorSlot, sizeof(*worldPosition));
}

/// Steps the 18-tick visibility fade and its draw and lock-on gates.
///
/// Fade progress is clamped to 0..SKULL_STALKER_FADE_FRAMES. Hiding starts a
/// black color blend with semitransparent drawing, then disables drawing and
/// lock-on at the hidden endpoint. Revealing restores lock-on and the default
/// color blend before clearing model flags at the visible endpoint.
static void _skullStalkerUpdateVisibility(Task* task)
{
    SkullStalkerWork* work;
    Enemy*            enemy;
    TmdObject*        model;

    work  = task->work;
    enemy = task->spawnArg2.pointer;
    model = task->extra.tmd;

    if (work->hiding != 0) {
        if (work->fadeFrames == 0) {
            work->fadeFrames++;
            model->flags = TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_BLACK);
        } else {
            work->fadeFrames++;
            if (work->fadeFrames >= SKULL_STALKER_FADE_FRAMES) {
                work->fadeFrames              = SKULL_STALKER_FADE_FRAMES;
                enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
                model->flags                  = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            }
        }
    } else {
        if (work->fadeFrames == SKULL_STALKER_FADE_FRAMES) {
            work->fadeFrames--;
            enemy->node.state.parts.flags = 0;
            model->flags                  = TMD_OBJECT_SEMI_TRANS;
            worldCoordSetActorColorMode(task->spawnArg2.pointer, ENEMY_COLOR_DEFAULT);
        } else {
            work->fadeFrames--;
            if (work->fadeFrames <= 0) {
                work->fadeFrames = 0;
                model->flags     = 0;
            }
        }
    }
}

/// Applies the death's decaying Y scale to the saved root matrix.
///
/// Requires live work, a model root and a saved matrix from the current death
/// tick. Scale is Q12 (ONE is 1.0). The decrement occurs only above the cutoff;
/// it can step below that cutoff. Reserves one ActorScaleScratch and invalidates
/// the root's composition cache; the model is already hidden by the kill.
static void _skullStalkerFlatten(Task* task)
{
    enum { SKULL_STALKER_FLATTEN_CUTOFF_Q12 = 0x200,
           SKULL_STALKER_FLATTEN_STEP_Q12   = 0x50 };
    GfxCoord*          rootCoord;
    ActorScaleScratch* scratchHead;
    ActorScaleScratch* scratch;
    SkullStalkerWork*  work;

    scratchHead                             = SCRATCH_STACK_CURSOR(ActorScaleScratch);
    work                                    = task->work;
    scratch                                 = scratchHead - 1;
    SCRATCH_STACK_CURSOR(ActorScaleScratch) = scratch;
    rootCoord                               = task->extra.tmd->coords;
    if (work->flattenScaleY > SKULL_STALKER_FLATTEN_CUTOFF_Q12) {
        work->flattenScaleY -= SKULL_STALKER_FLATTEN_STEP_Q12;
    }
    /// Restores and rescales the root, then invalidates its composition cache.
    ///
    /// Captures rootCoord, scratch and work as stable live pointers. Borrows
    /// the caller's scratch reservation and leaves its release to the caller.
    /// Expands to a statement list; use only within a braced compound statement.
#define SKULL_STALKER_APPLY_ROOT_SCALE()            \
    scratch->scale.vx = ONE;                        \
    scratch->scale.vy = work->flattenScaleY;        \
    scratch->scale.vz = ONE;                        \
    rootCoord->coord  = work->savedRootMtx;         \
    gfxSetRotIdentity(&scratch->matrix);            \
    ScaleMatrix(&scratch->matrix, &scratch->scale); \
    MulMatrix(&rootCoord->coord, &scratch->matrix); \
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY

    SKULL_STALKER_APPLY_ROOT_SCALE();
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
#undef SKULL_STALKER_APPLY_ROOT_SCALE
}

/// Detaches targeting and collision records before releasing the enemy task.
///
/// The task must retain its enemy, allocated work and model. Unlinking also
/// tolerates bodies already detached by the death state. Common enemy teardown
/// frees the enemy and work and starts body/task teardown after these embedded
/// records leave their lists.
static void _skullStalkerExit(Task* task)
{
    SkullStalkerWork* work;
    Enemy*            enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    enemy->recs = 0;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->frontSenseBody);
    worldCollisionUnlinkBody(&work->body);
    enemyTaskExit(task);
}
