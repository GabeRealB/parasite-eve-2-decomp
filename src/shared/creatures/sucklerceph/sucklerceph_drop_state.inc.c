/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Advances an armed Sucklerceph drop until it lands and enters crawling.
///
/// Requires live enemy/work/model and coordinate 1's lighting cache. Paused
/// updates sample that cache; hidden updates suppress drawing/targeting.
/// Running updates return until armed, then move, resolve contacts and animate.
/// Lighting samples coordinate 1 before root composition, preserving the cache's
/// existing frame/age. Positive parent-coordinate Y falls; landing clamps Y
/// to zero, stops both speeds and selects crawl animation/update task state 1.
static void _sucklercephDropState(Enemy* enemy, Task* task)
{
    enum {
        SUCKLERCEPH_DROP_COLOR_COORD_INDEX     = 1,
        SUCKLERCEPH_DROP_FORWARD_DECAY         = 2,
        SUCKLERCEPH_DROP_FALL_ACCELERATION     = 10,
        SUCKLERCEPH_DROP_COLLIDED_ACCELERATION = 20,
        SUCKLERCEPH_DROP_LANDING_Y             = 0,
        SUCKLERCEPH_DROP_UPDATE_TASK_STATE     = 1,
        SUCKLERCEPH_DROP_LANDING_SOUND         = 0x402C0008,
        SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT  = 8
    };
    SucklercephWork* work;
    GfxCoord*        rootCoord;
    s32              soundId;

    work = task->work;
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_PAUSED:
            _actorRenderUpdateCoordColor(enemy, &task->extra.tmd->coords[SUCKLERCEPH_DROP_COLOR_COORD_INDEX]);
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            break;
        case SCENE_COMBAT_ACTORS_RUNNING:
        default:
            if (work->dropArmed == 0) {
                return;
            }
            _sucklercephFallStep(task);
            _sucklercephDropCollide(task);
            _sucklercephTickAnim(task);
            // Keep the cached color sample before composing the moved root.
            _actorRenderUpdateCoordColor(enemy, &task->extra.tmd->coords[SUCKLERCEPH_DROP_COLOR_COORD_INDEX]);
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
            work->forwardSpeed -= SUCKLERCEPH_DROP_FORWARD_DECAY;
            if (work->forwardSpeed < 0) {
                work->forwardSpeed = 0;
            }
            rootCoord = task->extra.tmd->coords;
            if (rootCoord->coord.t[1] >= SUCKLERCEPH_DROP_LANDING_Y) {
                soundId = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT) | SUCKLERCEPH_DROP_LANDING_SOUND;
                sndEvtRequestScriptStart(soundId, (s8)worldCoordGetOriginAudioPan(rootCoord), (s8)worldCoordGetOriginAudioDepth(rootCoord));
                work->state                         = SUCKLERCEPH_STATE_AWAKE;
                work->awakeStage                    = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
                work->forwardSpeed                  = 0;
                work->fallSpeed                     = 0;
                work->animId                        = SUCKLERCEPH_ANIM_CRAWL;
                work->appliedAnim                   = 0;
                task->extra.tmd->coords->coord.t[1] = SUCKLERCEPH_DROP_LANDING_Y;
                task->state                         = SUCKLERCEPH_DROP_UPDATE_TASK_STATE;
            } else if (work->dropCollided == 0) {
                work->fallSpeed += SUCKLERCEPH_DROP_FALL_ACCELERATION;
            } else {
                work->fallSpeed += SUCKLERCEPH_DROP_COLLIDED_ACCELERATION;
            }
            break;
    }
}

/// Applies room-grid contact corrections to a falling Sucklerceph.
///
/// Requires live root/work storage and the four initialized body contacts.
/// The first grid hit applies signed 16.16 whole-unit XYZ corrections, rebounds
/// Y at -100 units per frame and reduces forward speed by one quarter with
/// signed truncation. Later hits correct only X/Z. Opposed grid normals restore
/// the translation saved before the fall step. Consumes the contacts and
/// releases the full `ActorContactDeltaWideScratch` reservation before return;
/// composition is refreshed by the caller.
static void _sucklercephDropCollide(Task* task)
{
    enum { SUCKLERCEPH_DROP_REBOUND_SPEED = -100 };

    ActorContactDeltaWideScratch* scratch;
    SucklercephWork*              work;
    GfxCoord*                     rootCoord;
    s32                           pushbackResult;

    work           = task->work;
    scratch        = SCRATCH_STACK_RESERVE_BLOCK(ActorContactDeltaWideScratch);
    rootCoord      = task->extra.tmd->coords;
    pushbackResult = worldCollisionResolvePushback(work->contacts, &scratch->delta, ARRAY_SIZE(work->contacts), NULL);
    switch (pushbackResult) {
        case WORLD_COLLISION_PUSHBACK_NO_GRID_HIT:
            break;
        case WORLD_COLLISION_PUSHBACK_GRID_HIT:
            if (work->dropCollided == 0) {
                rootCoord->coord.t[1] += scratch->delta.fixed.vy.halves.integer;
                work->fallSpeed        = SUCKLERCEPH_DROP_REBOUND_SPEED;
                work->forwardSpeed    -= work->forwardSpeed / 4;
                work->dropCollided     = pushbackResult;
            }
            rootCoord->coord.t[0] += scratch->delta.fixed.vx.halves.integer;
            rootCoord->coord.t[2] += scratch->delta.fixed.vz.halves.integer;
            break;
        case WORLD_COLLISION_PUSHBACK_OPPOSED:
            rootCoord->coord.t[0] = work->prevRootPos.vx;
            rootCoord->coord.t[1] = work->prevRootPos.vy;
            rootCoord->coord.t[2] = work->prevRootPos.vz;
            break;
    }
    worldCollisionClearContacts(work->contacts);
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactDeltaWideScratch);
}

/// Emits a command's initial puff and advances the requested idle animation.
///
/// task must own work and rootCoord; both command paths invoke this once per
/// accepted command. The puff copies the model-local puff offset and uses
/// extent 1024. Animation may continue an already selected idle clip rather
/// than reset it. Requires the live animation rig and room-effect task bank.
static inline void _sucklercephStartCommandPuff(Task* task, SucklercephWork* work, GfxCoord* rootCoord)
{
    enum { SUCKLERCEPH_COMMAND_PUFF_SIZE = 1024 };

    effectSpawn(EFFECT_ADDITIVE_PUFF, rootCoord, SUCKLERCEPH_COMMAND_PUFF_SIZE, &_gSucklercephPuffOffset);
    work->animId = SUCKLERCEPH_ANIM_IDLE;
    _sucklercephTickAnim(task);
}

/// Applies Sucklerceph encounter commands to reveal, hide or start puffing.
///
/// Borrows a read-only command for this call; ignores its context and both ABI
/// words. Requires live enemy/work/model. Exact command words 4 and 5 start
/// puffing only in active task state 1; 5 also resets the swelling counter.
/// Neither selects the puffing-death state. Low byte 3 hides the model,
/// disables sensing/body passes, resets its root and returns to drop state 3.
/// Low byte 1 reveals only outside task states 1/2 and arms the drop; its full
/// high byte indexes the current room's spot table without masking or clamping.
/// Reveal requires dumping-hole area with spot 0..11 or incinerator with 0..15,
/// and entry-move bits 4..7 must be zero. Spot positions are root-parent game
/// coordinates and headings use 4096 units per turn. Always returns 0, including
/// ignored commands, and retains no request pointer.
static s32 _sucklercephMessage(Task* task, s32 unusedMessageId, const ActorCommand* request, s32 unusedSecondArg)
{
    enum {
        SUCKLERCEPH_MESSAGE_ACTIVE_TASK       = 1,
        SUCKLERCEPH_MESSAGE_DROP_TASK         = 3,
        SUCKLERCEPH_COMMAND_HIDE              = 3,
        SUCKLERCEPH_COMMAND_RESTART_PUFFING   = 5,
        SUCKLERCEPH_COMMAND_ACTION_MASK       = 0xFF,
        SUCKLERCEPH_COMMAND_SPOT_SHIFT        = 8,
        SUCKLERCEPH_DROP_FORWARD_SPEED        = 200,
        SUCKLERCEPH_DROP_FALL_SPEED           = 100,
        SUCKLERCEPH_DROP_APPEAR_SOUND         = 0x54270006,
        SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT = 8
    };
    SucklercephWork* work;
    Enemy*           enemy;
    Enemy*           soundEnemy;
    TmdObject*       model;
    GfxCoord*        rootCoord;
    SVECTOR          spawnRotation;
    u16              actionByte;
    s16              heading;
    s32              headingMagnitude;
    s32              commandWord;
    s32              taskState;
    s32              soundId;
    s32              soundPan;

    model     = task->extra.tmd;
    enemy     = task->spawnArg2.pointer;
    taskState = task->state;
    work      = task->work;
    rootCoord = model->coords;
    if (taskState == SUCKLERCEPH_MESSAGE_ACTIVE_TASK) {
        commandWord = request->command;
        if (commandWord == OVERLAY_ENCOUNTER_COMMAND_STOP) {
            _sucklercephStartCommandPuff(task, work, rootCoord);
            work->animFrames = 0;
            work->state      = SUCKLERCEPH_STATE_PUFFING;
            return 0;
        }
        if (commandWord == SUCKLERCEPH_COMMAND_RESTART_PUFFING) {
            _sucklercephStartCommandPuff(task, work, rootCoord);
            work->animFrames  = 0;
            work->swellFrames = 0;
            work->state       = SUCKLERCEPH_STATE_PUFFING;
            return 0;
        }
    }
    actionByte = request->command & SUCKLERCEPH_COMMAND_ACTION_MASK;
    if ((actionByte & SUCKLERCEPH_COMMAND_ACTION_MASK) == OVERLAY_ENCOUNTER_COMMAND_APPEAR) {
        if ((u32)(task->state - SUCKLERCEPH_MESSAGE_ACTIVE_TASK) >= 2U) {
            // Callers supply an in-bounds full high byte for the live room table.
            if (gGameSession->location.area == GAME_AREA_SHELTER_B3_DUMPING_HOLE) {
                spawnRotation.vx      = 0;
                spawnRotation.vy      = gShelterB3DumpingHoleEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].heading;
                spawnRotation.vz      = 0;
                rootCoord->coord.t[0] = gShelterB3DumpingHoleEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].x;
                rootCoord->coord.t[1] = gShelterB3DumpingHoleEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].y;
                rootCoord->coord.t[2] = gShelterB3DumpingHoleEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].z;
                soundEnemy            = task->spawnArg2.pointer;
                soundId               = (((soundEnemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << SUCKLERCEPH_DROP_SOUND_INSTANCE_SHIFT) | SUCKLERCEPH_DROP_APPEAR_SOUND);
                soundPan              = (s8)worldCoordGetOriginAudioPan(rootCoord);
                sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            } else if (gGameSession->location.area == GAME_AREA_SHELTER_B3_GARBAGE_INCINERATOR) {
                spawnRotation.vx      = 0;
                spawnRotation.vy      = gShelterB3GarbageIncineratorEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].heading;
                spawnRotation.vz      = 0;
                rootCoord->coord.t[0] = gShelterB3GarbageIncineratorEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].x;
                rootCoord->coord.t[1] = gShelterB3GarbageIncineratorEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].y;
                rootCoord->coord.t[2] = gShelterB3GarbageIncineratorEncounterSpots[request->command >> SUCKLERCEPH_COMMAND_SPOT_SHIFT].z;
            }
            heading          = spawnRotation.vy;
            work->heading    = heading;
            headingMagnitude = heading >= 0 ? heading : -heading;
            if (headingMagnitude >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                if (heading >= ACTOR_TRANSFORM_ANGLE_HALF_TURN + 1) {
                    work->heading = heading - ACTOR_TRANSFORM_ANGLE_TURN;
                } else if (heading < -ACTOR_TRANSFORM_ANGLE_HALF_TURN) {
                    work->heading = heading + ACTOR_TRANSFORM_ANGLE_TURN;
                }
            }
            // Reveal and re-enable only the sensing and ordinary body spheres.
            tmdAllocPrimitiveBuffer(task->extra.tmd);
            task->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            task->extra.tmd->flags       &= (u16)~TMD_OBJECT_SKIP_AUTO_BUFFER;
            enemy->node.state.parts.flags = 0;
            work->senseBody.flags        |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            work->body.flags             |= (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            RotMatrix(&spawnRotation, &rootCoord->coord);
            work->forwardSpeed                    = SUCKLERCEPH_DROP_FORWARD_SPEED;
            work->dropArmed                       = 1;
            work->fallSpeed                       = SUCKLERCEPH_DROP_FALL_SPEED;
            work->dropCollided                    = 0;
            work->state                           = SUCKLERCEPH_STATE_AWAKE;
            work->awakeStage                      = SUCKLERCEPH_AWAKE_STAGE_CRAWL;
            task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(task->extra.tmd->coords);
        }
        return 0;
    }
    if ((actionByte & SUCKLERCEPH_COMMAND_ACTION_MASK) == SUCKLERCEPH_COMMAND_HIDE) {
        task->extra.tmd->flags       |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
        task->extra.tmd->flags       |= TMD_OBJECT_SKIP_AUTO_BUFFER;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->senseBody.flags        &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->body.flags             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_FLOOR_QUERY | WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
        spawnRotation.vz              = 0;
        spawnRotation.vy              = 0;
        spawnRotation.vx              = 0;
        RotMatrix(&spawnRotation, &rootCoord->coord);
        rootCoord->coord.t[2]                 = 0;
        rootCoord->coord.t[1]                 = 0;
        rootCoord->coord.t[0]                 = 0;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
        task->state      = SUCKLERCEPH_MESSAGE_DROP_TASK;
        work->dropArmed  = 0;
        work->state      = SUCKLERCEPH_STATE_DORMANT;
        work->awakeStage = SUCKLERCEPH_AWAKE_STAGE_NONE;
    }
    return 0;
}

/// Dispatches the standing Sucklerceph's spawn, active or death task state.
///
/// `task->state` must be in 0..2, and `spawnArg2.pointer` must hold its owning
/// enemy. Copies the carrier's three callbacks before dispatch; there is no
/// bounds check. Spawn or death may destroy either argument during the call.
static void _sucklercephTask(Task* task)
{
    EnemyTaskFuncTable3 states;

    states = _gSucklercephTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Updates the live specimen, with redraw-only pause and hidden-mode suppression.
///
/// Borrows the owning Enemy and live model task. RUNNING clears model/target
/// flags before behaviour, status reactions, contacts and animation; HIDDEN
/// sets draw suppression and makes the target unlockable; PAUSED only colours
/// and draws the shadow. Other mode values update without clearing those flags.
/// Running updates scale part 1 and compose its matrix before lighting; paused
/// redraw uses its existing cached matrix. No resource ownership changes.
static void _sucklercephUpdateState(Enemy* enemy, Task* task)
{
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            task->extra.tmd->flags        = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            task->extra.tmd->flags        = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
        case SCENE_COMBAT_ACTORS_PAUSED:
            _sucklercephColour(enemy, task);
            _sucklercephDrawShadow(task);
            return;
    }
    _sucklercephReactionDispatch(task);
    _sucklercephReactionFlags(task);
    _sucklercephContacts(task);
    _sucklercephAnimate(task);
    _sucklercephScalePart(task, &task->extra.tmd->coords[1]);
    task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    task->extra.tmd->coords[1].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[1]);
    _sucklercephColour(enemy, task);
    _sucklercephDrawShadow(task);
}

/// Advances swelling, buildup hold and damage-over-time requests on the Enemy.
///
/// Requires the live task/work/Enemy. STAGGER grows Q12 scale by 200 per call and
/// starts death on the fifth; its bit remains set. BUILDUP is consumed, stops
/// movement and freezes animation until the behaviour dispatch releases it.
/// Damage-over-time ticks feed their HP amount to `_sucklercephTakeDamage` and
/// clear both status bits on expiry. Later tests reload flags after earlier calls.
static void _sucklercephReactionFlags(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;
    s32              damageOverTime;
    u8               initialReactionFlags;

    enemy                = task->spawnArg2.pointer;
    initialReactionFlags = enemy->reactionFlags;
    work                 = task->work;
    if (initialReactionFlags != 0) {
        if (initialReactionFlags & ENEMY_REACTION_STAGGER) {
            work->swellFrames += 1;
            work->swellScale  += SUCKLERCEPH_SWELL_SCALE_STEP_Q12;
            if (work->swellFrames >= SUCKLERCEPH_SWELL_DURATION_FRAMES) {
                _sucklercephKill(task, 0);
                task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
                work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                task->state         = SUCKLERCEPH_TASK_DEATH;
                enemy->hp           = 0;
            }
        }
        if (enemy->reactionFlags & ENEMY_REACTION_BUILDUP) {
            enemy->reactionFlags &= ENEMY_REACTION_BUILDUP_CLEAR;
            work->state           = SUCKLERCEPH_STATE_STATUS_HOLD;
            work->deathFrames     = 0;
            work->forwardSpeed    = 0;
            work->animFrozen      = 1;
        }
        if (enemy->reactionFlags & ENEMY_REACTION_DAMAGE_OVER_TIME_BITS) {
            damageOverTime = damageTickEnemyDamageOverTime(enemy);
            if (damageOverTime != 0) {
                _sucklercephTakeDamage(task, damageOverTime);
            }
            if (damageIsEnemyDamageOverTimeExpired(enemy) != 0) {
                enemy->reactionFlags &= ENEMY_REACTION_DAMAGE_OVER_TIME_CLEAR;
            }
        }
    }
}

/// Advances the Sucklerceph's root for one ground-movement frame.
///
/// Requires a live root and `SucklercephWork`. Saves the old translation for
/// collision rollback, moves X/Z along the local matrix's Q12 forward column
/// by signed `forwardSpeed` game units, and adds 128 to Y for floor correction.
/// The caller resolves contacts and invalidates composition after the step.
static void _sucklercephStep(Task* task)
{
    enum { SUCKLERCEPH_GROUND_FLOOR_STEP  = 128,
           SUCKLERCEPH_AXIS_FRACTION_BITS = 12 };

    GfxCoord*        rootCoord;
    SucklercephWork* work;

    rootCoord              = &task->extra.tmd->coords[0];
    work                   = task->work;
    work->prevRootPos.vx   = rootCoord->coord.t[0];
    work->prevRootPos.vy   = rootCoord->coord.t[1];
    work->prevRootPos.vz   = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> SUCKLERCEPH_AXIS_FRACTION_BITS;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> SUCKLERCEPH_AXIS_FRACTION_BITS;
    rootCoord->coord.t[1] += SUCKLERCEPH_GROUND_FLOOR_STEP;
}

/// Updates the two animated Sucklerceph parts through the out-of-line frame hook.
///
/// Requires an initialized model and `SucklercephWork`. Uses the same request,
/// freeze and frame-count rules as `_sucklercephTickAnim`.
static void _sucklercephAnimate(Task* task)
{
    _sucklercephTickAnim(task);
}

/// Updates the Sucklerceph's lighting and hit colour at model part 1.
///
/// `enemy` must belong to `task`, whose model has a freshly composed part 1
/// and live work-owned light and colour matrices. The sampled translation is
/// in composition space. Reserves one `VECTOR` across the lighting call and
/// releases it before return; its fourth word is unused.
static void _sucklercephColour(Enemy* enemy, Task* task)
{
    GfxCoord* sampleCoord;
    VECTOR*   position;

    sampleCoord  = &task->extra.tmd->coords[1];
    position     = SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    position->vx = sampleCoord->workm.t[0];
    position->vy = sampleCoord->workm.t[1];
    position->vz = sampleCoord->workm.t[2];
    worldCoordUpdateActorColor(enemy, position, 0, 0);
    SCRATCH_STACK_RELEASE_BLOCK(VECTOR);
}

#include "gameplay/actor_render_shadow_types.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Draws the Sucklerceph's textured ground shadow at its composed root.
///
/// Requires a live model with an up-to-date root `workm`. The square has a
/// half-side of 448 game-coordinate units and uses the texture without colour
/// modulation. Reserves the full 24-byte centre block for the draw call;
/// only its first three words are accessed, and the trailing role is unproven.
static void _sucklercephDrawShadow(Task* task)
{
    enum { SUCKLERCEPH_SHADOW_HALF_SIDE   = 448,
           SUCKLERCEPH_SHADOW_RAW_TEXTURE = 0 };

    GfxCoord*                             rootCoord;
    ActorRenderGroundShadowCentreScratch* shadowScratch;

    rootCoord                = task->extra.tmd->coords;
    shadowScratch            = SCRATCH_STACK_RESERVE_BLOCK(ActorRenderGroundShadowCentreScratch);
    shadowScratch->centre.vx = rootCoord->workm.t[0];
    shadowScratch->centre.vy = rootCoord->workm.t[1];
    shadowScratch->centre.vz = rootCoord->workm.t[2];
    effectDrawGroundShadow(&shadowScratch->centre, SUCKLERCEPH_SHADOW_HALF_SIDE, SUCKLERCEPH_SHADOW_RAW_TEXTURE);
    SCRATCH_STACK_RELEASE_BLOCK(ActorRenderGroundShadowCentreScratch);
}

/// Applies the Sucklerceph's swelling scale to a model part's local rotation.
///
/// Requires initialized work and a writable coordinate. Clamps the unsigned
/// Q12 `swellScale` to `ONE`..`ONE + 1000`, then multiplies all three columns
/// on the GTE with signed-halfword saturation. Translation is retained;
/// repeated calls compound scale unless animation restores the rotation first.
/// The caller invalidates composition. Reserves and releases one `SVECTOR`
/// with its fourth halfword untouched and changes GTE state.
static void _sucklercephScalePart(Task* task, GfxCoord* coord)
{
    enum { SUCKLERCEPH_SWELL_SCALE_MAX = ONE + 1000 };

    SVECTOR*         column;
    MATRIX*          matrix;
    SucklercephWork* work;

    column = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    work   = task->work;
    if (work->swellScale >= SUCKLERCEPH_SWELL_SCALE_MAX) {
        work->swellScale = SUCKLERCEPH_SWELL_SCALE_MAX;
    }
    if (work->swellScale < ONE + 1) {
        work->swellScale = ONE;
    }
    matrix = &coord->coord;

    /// Scales one local basis column by the Sucklerceph's unsigned Q12 swell factor.
    ///
    /// columnIndex must be an integer constant 0..2 for the GTE wrappers'
    /// immediate halfword offsets. Requires a writable MATRIX, disjoint aligned
    /// SVECTOR workspace and live work. matrix and column repeat, work occurs
    /// once; all must be stable and side-effect-free. The caller clamps
    /// swellScale to 4096..5096.
    /// Products shift by 12 and saturate to s16; matrix translation and the
    /// workspace's fourth halfword stay intact. Changes GTE state, captures no
    /// locals and retains no pointers; undefined after the three column calls.
#define SUCKLERCEPH_SCALE_COLUMN(matrix, columnIndex, column, work) \
    do {                                                            \
        gte_ReadMatrixColumn((matrix), (columnIndex), (column));    \
        gte_lddp((work)->swellScale);                               \
        gte_ldsv((column));                                         \
        gte_gpf12();                                                \
        gte_stsv((column));                                         \
        gte_WriteMatrixColumn((column), (matrix), (columnIndex));   \
    } while (0)
    SUCKLERCEPH_SCALE_COLUMN(matrix, 0, column, work);
    SUCKLERCEPH_SCALE_COLUMN(matrix, 1, column, work);
    SUCKLERCEPH_SCALE_COLUMN(matrix, 2, column, work);

#undef SUCKLERCEPH_SCALE_COLUMN

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Rebuilds the root from its saved pose and applies an additional Q12 Y scale.
///
/// The caller supplies live root, work and scratch storage. Translation is
/// restored from `savedRootMtx`; the caller then invalidates composition.
static inline void _sucklercephRescaleRoot(GfxCoord* rootCoord, const SucklercephWork* work, ActorScaleScratch* scratch)
{
    scratch->scale.vx = ONE;
    scratch->scale.vy = work->flattenScaleY;
    scratch->scale.vz = ONE;
    rootCoord->coord  = work->savedRootMtx;
    gfxSetRotIdentity(&scratch->matrix);
    ScaleMatrix(&scratch->matrix, &scratch->scale);
    MulMatrix(&rootCoord->coord, &scratch->matrix);
}

/// Flattens the dying Sucklerceph's root along its local Y axis.
///
/// Requires live root/work storage and `savedRootMtx` captured when the death
/// countdown ended. The Q12 Y scale falls by 80 while above 512, so it may
/// finish below that threshold; X/Z stay at `ONE`. Rebuilds from the saved
/// matrix each call to avoid compounding scale and invalidates composition.
/// Releases its `ActorScaleScratch` block before return.
static void _sucklercephFlatten(Task* task)
{
    enum { SUCKLERCEPH_FLATTEN_SCALE_THRESHOLD = 512,
           SUCKLERCEPH_FLATTEN_SCALE_STEP      = 80 };

    GfxCoord*          rootCoord;
    ActorScaleScratch* scratch;
    SucklercephWork*   work;

    scratch   = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleScratch);
    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->flattenScaleY >= SUCKLERCEPH_FLATTEN_SCALE_THRESHOLD + 1) {
        work->flattenScaleY -= SUCKLERCEPH_FLATTEN_SCALE_STEP;
    }
    _sucklercephRescaleRoot(rootCoord, work, scratch);
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleScratch);
}

/// Detaches the Sucklerceph's collision and targeting state before teardown.
///
/// Requires the initialized task's live `SucklercephWork` and the owning enemy
/// in `spawnArg2.pointer`. Unlinking tolerates the bodies and target node already
/// being detached by the death state. Common enemy teardown releases the enemy
/// and task work; neither pointer remains live after the call.
static void _sucklercephExit(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;

    enemy = task->spawnArg2.pointer;
    work  = task->work;

    // Detach borrowed storage and list links before either owner is freed.
    enemy->recs = NULL;
    worldTargetUnlinkNode(&enemy->node);
    worldCollisionUnlinkBody(&work->senseBody);
    worldCollisionUnlinkBody(&work->body);
    worldCollisionUnlinkBody(&work->attackBody);
    worldCollisionUnlinkBody(&work->blastBody);
    enemyTaskExit(task);
}

/// Dispatches the hidden/drop Sucklerceph task through its four enemy states.
///
/// Requires a live enemy in `spawnArg2.pointer` and state 0..3: hidden setup,
/// ordinary update, death/teardown, or dropping into place. Copies the carrier's
/// four callbacks to the stack and indexes without checking. A callback may
/// destroy the enemy and task; no owned data is accessed after dispatch.
static void _sucklercephDropTask(Task* task)
{
    EnemyTaskFuncTable4 states;

    states = _gSucklercephDropTaskStates;
    states.funcs[task->state](task->spawnArg2.pointer, task);
}

/// Advances the Sucklerceph's root for one falling frame.
///
/// Requires a live root and `SucklercephWork`. Saves the old translation for
/// collision rollback, moves X/Z along the local matrix's Q12 forward column
/// by signed `forwardSpeed`, and adds `fallSpeed` to Y. Both speeds use parent
/// coordinate units per frame; positive Y falls. The caller applies contacts,
/// accelerates the fall and invalidates composition.
static void _sucklercephFallStep(Task* task)
{
    enum { SUCKLERCEPH_FALL_AXIS_FRACTION_BITS = 12 };

    GfxCoord*        rootCoord;
    SucklercephWork* work;

    rootCoord              = task->extra.tmd->coords;
    work                   = task->work;
    work->prevRootPos.vx   = rootCoord->coord.t[0];
    work->prevRootPos.vy   = rootCoord->coord.t[1];
    work->prevRootPos.vz   = rootCoord->coord.t[2];
    rootCoord->coord.t[0] += (rootCoord->coord.m[0][2] * work->forwardSpeed) >> SUCKLERCEPH_FALL_AXIS_FRACTION_BITS;
    rootCoord->coord.t[1] += work->fallSpeed;
    rootCoord->coord.t[2] += (rootCoord->coord.m[2][2] * work->forwardSpeed) >> SUCKLERCEPH_FALL_AXIS_FRACTION_BITS;
}
