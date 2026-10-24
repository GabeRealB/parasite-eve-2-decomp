/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Binds the ordinary spawn's model, contact, hit-effect and animation storage.
///
/// Requires model == task->extra.tmd, live nine-part coordinates, zeroed work
/// and a live enemy. Model matrices, contacts, poses and slots borrow task-owned
/// work until teardown; parameters, message callbacks and clips borrow the
/// loaded carrier. Sets current/maximum HP and binds the animation context;
/// it leaves slots and pose buffers zeroed until the caller applies a clip.
/// Does not link collision bodies or acquire a battle reference.
static inline void _madChaserBindSpawnResources(Task* task, TmdObject* model, MadChaserWork* work, Enemy* enemy)
{
    enum {
        MAD_CHASER_SPAWN_HIT_EFFECT_LOW_ARG = 320,
        MAD_CHASER_SPAWN_HIT_EFFECT_COUNT   = 2
    };

    task->msgTable             = _gMadChaserMsgTable;
    model->lightMtx            = &work->lightMtx;
    model->colorMtx            = &work->colorMtx;
    enemy->param               = &MAD_CHASER_ENEMY_PARAMS;
    enemy->recs                = work->contacts;
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = MAD_CHASER_SPAWN_HIT_EFFECT_LOW_ARG;
    work->effectArg.spawnArgHi = MAD_CHASER_SPAWN_HIT_EFFECT_COUNT;
    enemy->hp = enemy->hpMax = MAD_CHASER_ENEMY_PARAMS.hpMax;
    animationInitContext(&work->anim, _gMadChaserAnimBank, model, work->poses, work->slots);
}

/// Initializes an ordinary Mad Chaser in lurk or anchored dangle state.
///
/// Requires a live Enemy in spawnArg2.pointer and its nine-part model. Allocates
/// zeroed task-owned work; allocation failure destroys the enemy and starts
/// teardown. Binds its matrices, contacts, animation slots and pose buffers,
/// applies clip 7 at normal rate, links collision and targeting, and acquires
/// one battle reference. The loaded carrier supplies parameters and callbacks.
/// Spawn kind 1 (spawnArg1's low nibble) selects dangle; all others select lurk,
/// both at behavior/sub-state zero. Raises the root by 60 parent-coordinate
/// units and saves XYZ narrowed to s16 as the moving anchor. Storage remains
/// live until task teardown.
static void _madChaserSpawn(Task* task)
{
    enum {
        MAD_CHASER_SPAWN_KIND_MASK    = 0xF,
        MAD_CHASER_SPAWN_INITIAL_CLIP = 7,
        MAD_CHASER_SPAWN_ROOT_LIFT    = 60
    };
    Enemy*         enemy;
    GfxCoord*      root;
    MadChaserWork* work;
    TmdObject*     liveModel;
    MadChaserWork* setupWork;
    Enemy*         setupEnemy;
    GfxCoord*      liveRoot;
    MadChaserWork* animationWork;
    s32            lurkTaskState;

    enemy      = task->spawnArg2.pointer;
    root       = task->extra.tmd->coords;
    task->work = memCalloc(sizeof(MadChaserWork), 0);
    work       = task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    _madChaserQueueSoundBank();
    // Reload live resources after the sound request, then apply the initial pose.
    liveModel  = task->extra.tmd;
    setupWork  = task->work;
    setupEnemy = task->spawnArg2.pointer;
    liveRoot   = liveModel->coords;
    _madChaserBindSpawnResources(task, liveModel, setupWork, setupEnemy);
    animationWork              = task->work;
    animationWork->animRate    = ANIMATION_RATE_ONE;
    animationWork->animId      = MAD_CHASER_SPAWN_INITIAL_CLIP;
    animationWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    _madChaserTickAnim(task);
    liveRoot->parent = &gGfxViewCoord;
    _madChaserLinkBodies(task);
    setupWork->rotation.vy = ratan2(-liveRoot->coord.m[2][0], liveRoot->coord.m[2][2]) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    enemy                  = task->spawnArg2.pointer;
    worldTargetLinkNode(&enemy->node);
    enemy->field_4                = &task->extra.tmd->coords->coord;
    enemy->field_48               = 0;
    enemy->bodyPos.vx             = 0;
    enemy->bodyPos.vy             = 0;
    enemy->bodyPos.vz             = 0;
    enemy->coord                  = &task->extra.tmd->coords[1];
    enemy->node.state.parts.flags = WORLD_TARGET_KEEP_SCANNED;
    lurkTaskState                 = MAD_CHASER_TASK_LURK;
    (sceneAcquireBattleRef)(0);
    if ((task->spawnArg1.value & MAD_CHASER_SPAWN_KIND_MASK) == lurkTaskState) {
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_DANGLE);
    } else {
        _madChaserEnterTaskState(task, lurkTaskState);
    }
    // Keep the anchor in the root's parent frame for the dangle and contact steps.
    work->anchorPos.vx = root->coord.t[0];
    root->coord.t[1]  -= MAD_CHASER_SPAWN_ROOT_LIFT;
    work->anchorPos.vy = root->coord.t[1];
    work->anchorPos.vz = root->coord.t[2];
}

/// Binds hidden-spawn resources to the task-owned work and loaded model.
///
/// Model matrices, enemy contacts and animation storage borrow work until
/// task teardown; the clip bank and message table remain overlay-owned.
/// Requires model == task->extra.tmd, live nine-part coordinates, zeroed work,
/// a live enemy and a loaded clip bank. Neither storage nor a battle reference
/// is allocated here; the caller owns initialization of slots and collision.
static inline void _madChaserBindHiddenResources(Task* task, TmdObject* model, MadChaserWork* work, Enemy* enemy)
{
    enum {
        MAD_CHASER_HIDDEN_HIT_EFFECT_SPAN  = 320,
        MAD_CHASER_HIDDEN_HIT_EFFECT_COUNT = 2,
    };
    task->msgTable             = _gMadChaserMsgTable;
    model->lightMtx            = &work->lightMtx;
    model->colorMtx            = &work->colorMtx;
    enemy->param               = &MAD_CHASER_ENEMY_PARAMS;
    enemy->recs                = work->contacts;
    work->effectArg.coord      = &task->extra.tmd->coords[1];
    work->effectArg.spawnArgLo = MAD_CHASER_HIDDEN_HIT_EFFECT_SPAN;
    work->effectArg.spawnArgHi = MAD_CHASER_HIDDEN_HIT_EFFECT_COUNT;
    enemy->hp = enemy->hpMax = MAD_CHASER_ENEMY_PARAMS.hpMax;
    animationInitContext(&work->anim, _gMadChaserAnimBank, model, work->poses, work->slots);
}

/// Initializes a Mad Chaser that waits for a scripted emerge command.
///
/// Requires a live enemy in spawnArg2.pointer and its nine-part model. Owns a
/// zeroed MadChaserWork allocation, whose matrices, contacts, slots and pose
/// buffers remain borrowed by the model, enemy and animation context until task
/// teardown. Acquires one battle reference and links an un-lockable target.
/// Disables pair/grid collision and limb shadows, raises the root by 60 parent
/// units and enters emerge behavior zero. Spawn kind 2 also hides active drawing.
/// Bit 16 of spawnArg1.value cancels the spawn after the sound-bank request;
/// allocation failure and cancellation destroy the enemy and begin task teardown.
static void _madChaserSpawnHidden(Task* task)
{
    enum {
        MAD_CHASER_HIDDEN_SPAWN_CANCEL_SHIFT    = 16,
        MAD_CHASER_HIDDEN_SPAWN_KIND_MASK       = 0xF,
        MAD_CHASER_HIDDEN_SPAWN_HIDE_MODEL_KIND = 2,
        MAD_CHASER_HIDDEN_INITIAL_CLIP          = 7,
        MAD_CHASER_HIDDEN_ROOT_LIFT             = 60,
    };
    TmdObject*     model;
    Enemy*         enemy;
    GfxCoord*      root;
    MadChaserWork* work;
    TmdObject*     liveModel;
    MadChaserWork* setupWork;
    Enemy*         setupEnemy;
    GfxCoord*      liveRoot;
    MadChaserWork* animationWork;
    MadChaserWork* stateWork;
    Enemy*         targetEnemy;
    s32            spawnFlags;

    model      = task->extra.tmd;
    enemy      = task->spawnArg2.pointer;
    root       = model->coords;
    task->work = memCalloc(sizeof(MadChaserWork), 0);
    work       = task->work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }
    _madChaserQueueSoundBank();
    spawnFlags = task->spawnArg1.value;
    if ((spawnFlags >> MAD_CHASER_HIDDEN_SPAWN_CANCEL_SHIFT) & 1) {
        enemyDestroy(enemy, task);
        return;
    }
    if ((spawnFlags & MAD_CHASER_HIDDEN_SPAWN_KIND_MASK) == MAD_CHASER_HIDDEN_SPAWN_HIDE_MODEL_KIND) {
        model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
    }
    // Bind task-owned storage before applying the initial pose.
    liveModel  = task->extra.tmd;
    setupWork  = task->work;
    setupEnemy = task->spawnArg2.pointer;
    liveRoot   = liveModel->coords;
    _madChaserBindHiddenResources(task, liveModel, setupWork, setupEnemy);
    animationWork              = task->work;
    animationWork->animRate    = ANIMATION_RATE_ONE;
    animationWork->animId      = MAD_CHASER_HIDDEN_INITIAL_CLIP;
    animationWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
    _madChaserTickAnim(task);
    liveRoot->parent = &gGfxViewCoord;
    _madChaserLinkBodies(task);
    setupWork->rotation.vy = ratan2(-liveRoot->coord.m[2][0], liveRoot->coord.m[2][2]) + ACTOR_TRANSFORM_ANGLE_HALF_TURN;
    (sceneAcquireBattleRef)(0);
    targetEnemy = task->spawnArg2.pointer;
    worldTargetLinkNode(&targetEnemy->node);
    targetEnemy->field_4                = &task->extra.tmd->coords->coord;
    targetEnemy->field_48               = 0;
    targetEnemy->bodyPos.vx             = 0;
    targetEnemy->bodyPos.vy             = 0;
    targetEnemy->bodyPos.vz             = 0;
    targetEnemy->coord                  = &task->extra.tmd->coords[1];
    targetEnemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
    // Retain the spawn anchor while disabling contact and shadow presentation.
    work->anchorPos.vx    = root->coord.t[0];
    root->coord.t[1]     -= MAD_CHASER_HIDDEN_ROOT_LIFT;
    work->anchorPos.vy    = root->coord.t[1];
    work->anchorPos.vz    = root->coord.t[2];
    work->shadowHidden    = 1;
    work->pairBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    work->gridBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
    stateWork             = task->work;
    task->state           = MAD_CHASER_TASK_EMERGE;
    stateWork->state      = 0;
    stateWork->subState   = 0;
}

#include "mad_chaser_frame_shadows.inc.c"

/// Advances combat behavior, animation and contacts, then refreshes presentation.
///
/// Requires live enemy/model/work, initialized nine-part animation and behavior
/// state 0..10. Running frames track the nearer player and dispatch unless a
/// pull or vanish command takes over, relax spine yaw, pin part 6 when anchored,
/// rebuild rotation and apply contacts. Fatal blast hits enter death even while
/// busy; other death transitions wait for busy to clear. Scripted deaths require
/// the extended task table. Paused frames still update color and ground shadows;
/// hidden frames suppress model drawing and return. Borrows task-owned storage
/// and requires initialized rendering scratch and frame-arena space.
static void _madChaserCombatTick(Task* task)
{
    enum {
        MAD_CHASER_COMBAT_ANCHOR_PART            = 6,
        MAD_CHASER_COMBAT_SPINE_DECAY_MULTIPLIER = 16,
        MAD_CHASER_COMBAT_SPINE_DECAY_SHIFT      = 9
    };
    Enemy*          enemy     = task->spawnArg2.pointer;
    TmdObject*      model     = task->extra.tmd;
    MadChaserWork*  work      = task->work;
    GfxCoord*       rootCoord = model->coords;
    TaskFuncTable11 states    = _gMadChaserCombatStates;
    s32             spineYawBits;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            _madChaserTrackPlayer(task);
            if (_madChaserTakePullOrVanishCommand(task) == 0) {
                states.funcs[(s16)work->state](task);
            }
            _madChaserTickAnim(task);
            // Narrow the correction before shifting; angles use 4096 units/turn.
            spineYawBits   = (u16)work->spineYaw;
            work->spineYaw = spineYawBits + ((s16)(-(spineYawBits * MAD_CHASER_COMBAT_SPINE_DECAY_MULTIPLIER)) >> MAD_CHASER_COMBAT_SPINE_DECAY_SHIFT);
            _madChaserTwistSpine(task);
            if (work->anchored == 1) {
                _madChaserPinPart(task, MAD_CHASER_COMBAT_ANCHOR_PART, &work->leapAnchorPos);
            }
            _madChaserUpdateRotation(task);
            _madChaserApplyContacts(task, 0);
            if (work->leapCooldown != 0) {
                work->leapCooldown--;
            }
            // Contacts can request death after this frame's behavior has run.
            if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            }
            if (work->busy == 0 && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_DROP_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DROP_DEATH);
            } else if (work->command == MAD_CHASER_COMMAND_SHRINK_DEATH && work->busy == 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_SHRINK_DEATH);
            }
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            // Fall through so running and paused frames share presentation.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            _madChaserDrawFrameShadows(task);
            return;
    }
}

#include "main/random.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Starts the combat approach with a distance-scaled walk and random leap range.
///
/// Requires live enemy/model/work storage, a tracked player distance and the
/// walk entry sub-state. Blends clip 7 over eight frames, advances to approach,
/// plays the positional walk cue and consumes one resident LCG draw for a
/// 0..2047-unit range bonus. Distance bands of 1000 parent-coordinate units
/// select rates 16/20/24/28/32/64 (sixteenths of a frame) and heading steps
/// 16/18/20/22/24/32 (4096ths of a turn). Animation ticking belongs to the caller.
static void _madChaserWalkStart(Task* task)
{
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;
    s16            turnStep;

    work                  = task->work;
    work->animBlendFrames = MAD_CHASER_WALK_BLEND_FRAMES;
    work->animId          = MAD_CHASER_WALK_CLIP;
    work->animRate        = ANIMATION_RATE_ONE;
    work->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    work->subState++;
    soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 1);
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    RANDOM_LCG_ADVANCE();
    work->leapRangeBonus = RANDOM_LCG_DRAW() & MAD_CHASER_WALK_LEAP_RANGE_BONUS_MASK;
    if (work->playerDist < MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE;
        work->turnStep = MAD_CHASER_WALK_BASE_TURN_STEP;
        return;
    }
    if (work->playerDist < 2 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + MAD_CHASER_WALK_TURN_BAND_STEP;
    } else if (work->playerDist < 3 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + 2 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + 2 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else if (work->playerDist < 4 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + 3 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + 3 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else if (work->playerDist < 5 * MAD_CHASER_WALK_DISTANCE_BAND) {
        work->animRate = ANIMATION_RATE_ONE + 4 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep       = MAD_CHASER_WALK_BASE_TURN_STEP + 4 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else {
        work->animRate = MAD_CHASER_WALK_FAR_RATE;
        turnStep       = MAD_CHASER_WALK_FAR_TURN_STEP;
    }
    work->turnStep = turnStep;
}

/// Steps the root along local -Z at the walk animation's current rate.
///
/// work must be the task's live Mad Chaser work, with heading in 4096ths of a
/// turn and animRate in sixteenths of a frame. The normal-rate distance is -16
/// parent-coordinate units; rate scaling narrows it to s16 before multiplying
/// signed Q12 sine/cosine. Updates only X/Z translation and marks composition
/// dirty; animation playback and collision belong to the caller.
static inline void _madChaserWalkApproachAdvanceRoot(Task* task, MadChaserWork* work)
{
    enum {
        MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS = 16,
        MAD_CHASER_WALK_TRIG_FRACTION_BITS         = 12,
    };
    s16 heading;
    s16 stepDistance;

    stepDistance                          = _madChaserScaleByAnimRate(task, MAD_CHASER_WALK_DISTANCE_AT_NORMAL_RATE);
    heading                               = work->rotation.vy;
    task->extra.tmd->coords->coord.t[0]  += ((rsin(heading) << (MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS - MAD_CHASER_WALK_TRIG_FRACTION_BITS)) * stepDistance) >> MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(heading) << (MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS - MAD_CHASER_WALK_TRIG_FRACTION_BITS)) * stepDistance) >> MAD_CHASER_WALK_TRIG_PRODUCT_FRACTION_BITS;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Approaches the tracked player until distance, facing and cooldown permit a leap.
///
/// Requires live model/enemy/work storage and a tracked player in the root's
/// parent frame. Only raises the walk's rate and turn step to the current
/// distance band; moving closer does not slow it. Steps along local -Z by
/// animRate units per frame and plays the walk cue on slot-1 boundary/jump/held
/// status. Advances within 2000 + leapRangeBonus units when cooldown is zero
/// or distance is below 1500, and bearing is strictly within +/-512 of the
/// forward direction (2048) in 4096 angle units per turn. The caller ticks
/// animation; movement dirties root composition and retains s16 step narrowing.
static void _madChaserWalkApproach(Task* task)
{
    MadChaserWork* work = task->work;
    s16            playerDistance;
    s16            minimumRate;
    s16            turnStep;
    s32            soundId;
    s32            audioPan;

    playerDistance = work->playerDist;
    // Keep the fastest band reached during this approach.
    if (playerDistance < MAD_CHASER_WALK_DISTANCE_BAND) {
        minimumRate = ANIMATION_RATE_ONE;
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP;
    } else if (playerDistance < 2 * MAD_CHASER_WALK_DISTANCE_BAND) {
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + MAD_CHASER_WALK_TURN_BAND_STEP;
        minimumRate = ANIMATION_RATE_ONE + MAD_CHASER_WALK_RATE_BAND_STEP;
    } else if (playerDistance < 3 * MAD_CHASER_WALK_DISTANCE_BAND) {
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + 2 * MAD_CHASER_WALK_TURN_BAND_STEP;
        minimumRate = ANIMATION_RATE_ONE + 2 * MAD_CHASER_WALK_RATE_BAND_STEP;
    } else if (playerDistance < 4 * MAD_CHASER_WALK_DISTANCE_BAND) {
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + 3 * MAD_CHASER_WALK_TURN_BAND_STEP;
        minimumRate = ANIMATION_RATE_ONE + 3 * MAD_CHASER_WALK_RATE_BAND_STEP;
    } else if (playerDistance < 5 * MAD_CHASER_WALK_DISTANCE_BAND) {
        minimumRate = ANIMATION_RATE_ONE + 4 * MAD_CHASER_WALK_RATE_BAND_STEP;
        turnStep    = MAD_CHASER_WALK_BASE_TURN_STEP + 4 * MAD_CHASER_WALK_TURN_BAND_STEP;
    } else {
        minimumRate = MAD_CHASER_WALK_FAR_RATE;
        turnStep    = MAD_CHASER_WALK_FAR_TURN_STEP;
    }
    if (work->animRate < minimumRate) {
        work->animRate = minimumRate;
        work->turnStep = turnStep;
    }
    _madChaserTurnToPlayer(task, work->turnStep);
    _madChaserWalkApproachAdvanceRoot(task, work);
    if (_madChaserAnimHasBoundaryStatus(task)) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 1);
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // This unsigned wrap test excludes both facing-arc endpoints.
    if (work->playerDist < work->leapRangeBonus + MAD_CHASER_WALK_BASE_LEAP_DISTANCE && (work->playerDist < MAD_CHASER_WALK_CLOSE_LEAP_DISTANCE || work->leapCooldown == 0) &&
        (u16)(((work->playerBearing + ACTOR_TRANSFORM_ANGLE_TURN / 2) & ACTOR_TRANSFORM_ANGLE_MASK) - MAD_CHASER_WALK_LEAP_HALF_ARC) > ACTOR_TRANSFORM_ANGLE_TURN - 2 * MAD_CHASER_WALK_LEAP_HALF_ARC) {
        work->subState++;
    }
}

/// Captures part 6's world position for the leap's brief anchoring interval.
///
/// Requires live work, a nine-part model and initialized view coordinates.
/// Composes the joint, removes the view transform and narrows its translation
/// to s16. Leaves the joint dirty for the subsequent animation/pinning pass.
static inline void _madChaserLeapCaptureAnchor(Task* task, MadChaserWork* work)
{
    enum { MAD_CHASER_LEAP_ANCHOR_JOINT = 6 };
    MATRIX    anchorMatrix;
    GfxCoord* partCoords = task->extra.tmd->coords;
    SVECTOR*  anchorPos;

    gGfxViewCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&gGfxViewCoord);
    partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT]);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT].workm, &anchorMatrix);
    anchorPos                                             = &work->leapAnchorPos;
    anchorPos->vx                                         = anchorMatrix.t[0];
    anchorPos->vy                                         = anchorMatrix.t[1];
    anchorPos->vz                                         = anchorMatrix.t[2];
    partCoords[MAD_CHASER_LEAP_ANCHOR_JOINT].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Advances the leap's anchored windup, attack lunge and accelerating fall.
///
/// Requires initialized nine-part animation/work storage, a live enemy and root
/// parented to the view, and the launch height saved in moveStartPos.vy. Counts
/// callback frames with u16 wrapping, retaining the signed frame comparisons.
/// Hits can interrupt before frame 40; subsequent frames mark the move busy.
/// Captures part 6 in world space on frame 43 and pins it during frames 43..46.
/// Locks the lunge heading on frame 45, plays its positional cue on frame 46,
/// and steps -250 parent units along that heading on frames 45..53 with the
/// attack body enabled. From frame 47, the vertical acceleration grows by 30
/// per callback; moveSpeed and moveAccel narrow to s16 on each update.
///
/// On frames 45..48, playerDist below 369 starts clip 16 and jumps two sub-states
/// to rebound. Otherwise landing clamps the root to its launch Y, resets the
/// frame counter, starts clip 18 and advances one sub-state. Both transitions
/// blend over two normal-rate frames; the caller performs the animation tick.
static void _madChaserLeapAttack(Task* task)
{
    enum {
        MAD_CHASER_LEAP_COMMIT_FRAME            = 40,
        MAD_CHASER_LEAP_ANCHOR_START_FRAME      = 43,
        MAD_CHASER_LEAP_ANCHOR_END_FRAME        = 46,
        MAD_CHASER_LEAP_LOCK_HEADING_FRAME      = 45,
        MAD_CHASER_LEAP_SOUND_FRAME             = 46,
        MAD_CHASER_LEAP_LUNGE_START_FRAME       = 45,
        MAD_CHASER_LEAP_LUNGE_END_FRAME         = 53,
        MAD_CHASER_LEAP_REBOUND_START_FRAME     = 45,
        MAD_CHASER_LEAP_REBOUND_END_FRAME       = 48,
        MAD_CHASER_LEAP_FALL_START_FRAME        = 47,
        MAD_CHASER_LEAP_AIM_LIMIT               = 0x300,
        MAD_CHASER_LEAP_REBOUND_DISTANCE        = 369,
        MAD_CHASER_LEAP_LUNGE_DISTANCE          = -250,
        MAD_CHASER_LEAP_REBOUND_LAUNCH_SPEED    = -200,
        MAD_CHASER_LEAP_ACCEL_INCREMENT         = 30,
        MAD_CHASER_LEAP_REBOUND_CLIP            = 16,
        MAD_CHASER_LEAP_LAND_CLIP               = 18,
        MAD_CHASER_LEAP_TRANSITION_BLEND_FRAMES = 2,
        MAD_CHASER_LEAP_SOUND                   = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 5),
    };
    MadChaserWork* work = task->work;
    GfxCoord*      root = task->extra.tmd->coords;
    s16            leapHeading;
    s32            soundId;
    s32            audioPan;
    s16            relativeLeapBearing;
    s16            stepDistance;

    if ((s16)++work->stateFrames < MAD_CHASER_LEAP_COMMIT_FRAME) {
        if (_madChaserTakeHitRequest(task)) {
            return;
        }
    } else {
        work->busy = 1;
    }
    // Hold the anchor joint in world space while the leap winds up.
    if ((s16)work->stateFrames == MAD_CHASER_LEAP_ANCHOR_START_FRAME) {
        _madChaserLeapCaptureAnchor(task, work);
    }
    if (work->stateFrames >= MAD_CHASER_LEAP_ANCHOR_START_FRAME && work->stateFrames <= MAD_CHASER_LEAP_ANCHOR_END_FRAME) {
        work->anchored = 1;
    } else {
        work->anchored = 0;
    }
    if ((s16)work->stateFrames == MAD_CHASER_LEAP_SOUND_FRAME) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_LEAP_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    // Reverse the player bearing to aim the negative-distance lunge.
    if ((s16)work->stateFrames == MAD_CHASER_LEAP_LOCK_HEADING_FRAME) {
        relativeLeapBearing = (work->playerBearing + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
        if (relativeLeapBearing < MAD_CHASER_LEAP_AIM_LIMIT) {
            work->leapHeading = (relativeLeapBearing + work->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
        } else if (relativeLeapBearing >= ACTOR_TRANSFORM_ANGLE_TURN - MAD_CHASER_LEAP_AIM_LIMIT) {
            work->leapHeading = (relativeLeapBearing + work->rotation.vy) & ACTOR_TRANSFORM_ANGLE_MASK;
        } else {
            work->leapHeading = work->rotation.vy;
        }
    }
    if (work->stateFrames >= MAD_CHASER_LEAP_LUNGE_START_FRAME && work->stateFrames <= MAD_CHASER_LEAP_LUNGE_END_FRAME) {
        leapHeading                           = work->leapHeading;
        stepDistance                          = MAD_CHASER_LEAP_LUNGE_DISTANCE;
        task->extra.tmd->coords->coord.t[0]  += ((rsin(leapHeading) << 4) * stepDistance) >> 0x10;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(leapHeading) << 4) * stepDistance) >> 0x10;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        work->attackBody.flags               |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else {
        work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    // A nearby player selects rebound before the vertical fall update.
    if (work->stateFrames >= MAD_CHASER_LEAP_REBOUND_START_FRAME && work->stateFrames <= MAD_CHASER_LEAP_REBOUND_END_FRAME && work->playerDist < MAD_CHASER_LEAP_REBOUND_DISTANCE) {
        MadChaserWork* requestWork;

        work->moveAccel              = 0;
        work->moveSpeed              = MAD_CHASER_LEAP_REBOUND_LAUNCH_SPEED;
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_LEAP_TRANSITION_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_LEAP_REBOUND_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        work->anchored               = 0;
        work->attackBody.flags      &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->subState              += 2;
        return;
    }
    // Move the grid sphere with the root, then settle at the launch height.
    if ((s16)work->stateFrames >= MAD_CHASER_LEAP_FALL_START_FRAME) {
        root->coord.t[1]      += work->moveSpeed;
        work->gridBody.pos.vy += work->moveSpeed;
        work->moveAccel       += MAD_CHASER_LEAP_ACCEL_INCREMENT;
        work->moveSpeed       += work->moveAccel;
        if (root->coord.t[1] >= work->moveStartPos.vy) {
            MadChaserWork* requestWork = task->work;

            requestWork->animBlendFrames = MAD_CHASER_LEAP_TRANSITION_BLEND_FRAMES;
            requestWork->animRate        = ANIMATION_RATE_ONE;
            requestWork->animId          = MAD_CHASER_LEAP_LAND_CLIP;
            requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            root->coord.t[1]             = work->moveStartPos.vy;
            work->gridBody.pos.vy        = 0;
            work->stateFrames            = 0;
            work->subState++;
        }
    }
}

/// Finishes an ordinary leap landing, turns away and returns to idle lurk.
///
/// Requires live work and enemy/model storage with stateFrames reset on landing.
/// Plays the positional landing cue on frame 1. A slot-1 boundary, jump or held
/// pose clears busy, adds a half turn, stores a 90..217 combat-tick leap cooldown
/// from one random draw and resets clip 13 at normal rate. Enters the lurk task
/// at behavior/sub-state zero; the caller ticks the requested animation.
static void _madChaserLeapTurnAway(Task* task)
{
    enum {
        MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES  = 90,
        MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK = 127,
        MAD_CHASER_LEAP_LANDING_SOUND        = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 4),
        MAD_CHASER_LEAP_TURN_AWAY_CLIP       = 13,
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            soundId;
    s32            audioPan;
    u32            randomState;

    work = task->work;
    if ((s16)++work->stateFrames == 1) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_LEAP_LANDING_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if (_madChaserAnimHasBoundaryStatus(task) != 0) {
        work->busy               = 0;
        randomState              = RANDOM_LCG_NEXT(gRandomLcgState);
        work->leapCooldown       = ((randomState >> 16) & MAD_CHASER_LEAP_COOLDOWN_RANDOM_MASK) + MAD_CHASER_LEAP_COOLDOWN_MIN_FRAMES;
        work->rotation.vy       += ACTOR_TRANSFORM_ANGLE_HALF_TURN;
        requestWork              = task->work;
        requestWork->animRate    = ANIMATION_RATE_ONE;
        requestWork->animId      = MAD_CHASER_LEAP_TURN_AWAY_CLIP;
        requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        gRandomLcgState          = randomState;
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_LURK);
    }
}

/// Moves the leap rebound back along its locked heading until landing.
///
/// Requires live work/model storage and the launch height in moveStartPos.vy.
/// Moves 200 parent-coordinate units per callback along leapHeading; Y and the
/// grid sphere follow moveSpeed, with acceleration growing by 14 each callback.
/// Both motion fields narrow to s16. At the saved height, clamps root Y, clears
/// the grid's local Y offset and frame counter, requests clip 19 blended over
/// two normal-rate frames and advances to rebound landing. The caller ticks it.
static void _madChaserLeapRebound(Task* task)
{
    enum {
        MAD_CHASER_REBOUND_STEP_DISTANCE     = 200,
        MAD_CHASER_REBOUND_ACCEL_INCREMENT   = 14,
        MAD_CHASER_REBOUND_LAND_BLEND_FRAMES = 2,
        MAD_CHASER_REBOUND_LAND_CLIP         = 19,
    };
    MadChaserWork* work;
    s16            reboundHeading;
    GfxCoord*      root;
    MadChaserWork* requestWork;
    s32            stepDistance;
    s32            scaledSine;

    work                                  = task->work;
    reboundHeading                        = work->leapHeading;
    root                                  = task->extra.tmd->coords;
    scaledSine                            = rsin(reboundHeading) << 4;
    stepDistance                          = MAD_CHASER_REBOUND_STEP_DISTANCE;
    task->extra.tmd->coords->coord.t[0]  += (scaledSine * stepDistance) >> 16;
    task->extra.tmd->coords->coord.t[2]  += ((rcos(reboundHeading) << 4) * stepDistance) >> 16;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    // Move the collision sphere with the vertical arc before testing landing.
    root->coord.t[1]      += work->moveSpeed;
    work->gridBody.pos.vy += work->moveSpeed;
    work->moveAccel       += MAD_CHASER_REBOUND_ACCEL_INCREMENT;
    work->moveSpeed       += work->moveAccel;
    if (root->coord.t[1] >= work->moveStartPos.vy) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_REBOUND_LAND_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_REBOUND_LAND_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        root->coord.t[1]             = work->moveStartPos.vy;
        work->gridBody.pos.vy        = 0;
        work->stateFrames            = 0;
        work->subState++;
    }
}

#include "mad_chaser_frame_shadows.inc.c"

/// Advances the hanging body's behavior and keeps its anchor fixed after animation.
///
/// Requires live enemy/model/work and initialized nine-part animation. Dangle
/// uses behavior zero and sub-state 0..3. Running frames apply contacts while
/// ignoring the player's body, enter death on a fatal blast, tick animation and
/// pin part 6 to anchorPos when anchored. Paused frames still refresh color and
/// ground shadows. Both running and paused frames enable model drawing; hidden
/// frames suppress it and return. Borrows task-owned storage and requires
/// initialized rendering scratch and frame-arena space.
static void _madChaserDangleFrame(Task* task)
{
    enum { MAD_CHASER_DANGLE_ANCHOR_PART = 6 };
    TmdObject*     model     = task->extra.tmd;
    MadChaserWork* work      = task->work;
    Enemy*         enemy     = task->spawnArg2.pointer;
    GfxCoord*      rootCoord = model->coords;
    // The behavior dispatch remains indexed even though dangle only uses zero.
    TaskFunc states[1] = { _madChaserDangleState };

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            states[(s16)work->state](task);
            _madChaserApplyContacts(task, 1);
            if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST && enemy->hp <= 0) {
                _madChaserEnterTaskState(task, MAD_CHASER_TASK_DEATH);
            }
            // Animate before translating the root to retain the hanging anchor.
            _madChaserTickAnim(task);
            if (work->anchored == 1) {
                _madChaserPinPart(task, MAD_CHASER_DANGLE_ANCHOR_PART, &work->anchorPos);
            }
            rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
            // Fall through so running and paused frames share presentation.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            _madChaserDrawFrameShadows(task);
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

/// Replaces the root's nine Q12 rotation coefficients for the dangle fall.
///
/// Both matrices must be live through the call. Copies only the 3x3 basis;
/// translation and the matrix's alignment halfword are retained. The caller
/// is responsible for invalidating coordinate composition.
static inline void _madChaserDangleFallCopyRotation(MATRIX* rootMatrix, const MATRIX* rotationMatrix)
{
    rootMatrix->m[0][0] = rotationMatrix->m[0][0];
    rootMatrix->m[0][1] = rotationMatrix->m[0][1];
    rootMatrix->m[0][2] = rotationMatrix->m[0][2];
    rootMatrix->m[1][0] = rotationMatrix->m[1][0];
    rootMatrix->m[1][1] = rotationMatrix->m[1][1];
    rootMatrix->m[1][2] = rotationMatrix->m[1][2];
    rootMatrix->m[2][0] = rotationMatrix->m[2][0];
    rootMatrix->m[2][1] = rotationMatrix->m[2][1];
    rootMatrix->m[2][2] = rotationMatrix->m[2][2];
}

/// Drops the released root to parent Y zero while easing its saved pitch upright.
///
/// Requires live model/work storage and fall motion initialized by the sway.
/// Eases pitch by one quarter of its negation in 4096 angle units per turn,
/// retains heading, and replaces the root's rotation. Adds 2 to s16 acceleration
/// and then acceleration to s16 speed each frame; Y advances by that speed in
/// parent-coordinate units. Landing requires Y > 0: clamps it to zero, resets
/// the u16 frame counter, cuts to clip 12 at twice normal rate and advances.
/// The caller ticks animation and marks the root composition dirty.
static void _madChaserDangleFall(Task* task)
{
    enum {
        MAD_CHASER_DANGLE_PITCH_EASE_SHIFT = 2,
        MAD_CHASER_DANGLE_FALL_ACCEL       = 2,
        MAD_CHASER_DANGLE_LAND_CLIP        = 12,
    };
    MadChaserWork* work;
    GfxCoord*      root;
    MATRIX         rotation;
    MATRIX*        rotationMatrix;
    MATRIX*        rootMatrix;
    MadChaserWork* requestWork;

    work           = task->work;
    root           = task->extra.tmd->coords;
    rotationMatrix = &rotation;
    gfxSetRotIdentity(rotationMatrix);
    work->fallPitch += -work->fallPitch >> MAD_CHASER_DANGLE_PITCH_EASE_SHIFT;
    RotMatrixX(work->fallPitch, rotationMatrix);
    RotMatrixY(work->rotation.vy, rotationMatrix);
    rootMatrix = &root->coord;
    _madChaserDangleFallCopyRotation(rootMatrix, rotationMatrix);
    // Signed halfword motion accumulates before the strict floor crossing test.
    work->moveAccel  += MAD_CHASER_DANGLE_FALL_ACCEL;
    work->moveSpeed  += work->moveAccel;
    root->coord.t[1] += work->moveSpeed;
    if (root->coord.t[1] > 0) {
        work->stateFrames        = 0;
        root->coord.t[1]         = 0;
        requestWork              = task->work;
        requestWork->animRate    = 2 * ANIMATION_RATE_ONE;
        requestWork->animId      = MAD_CHASER_DANGLE_LAND_CLIP;
        requestWork->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
        work->subState++;
    }
}

/// Plays a landing cue on this enemy's positional character-bank channel.
///
/// Requires a live enemy and a model root whose local-to-view matrix is already
/// composed. cue is a packed character-bank script id with a zero instance byte;
/// the enemy's place index supplies bits 8..11. Pan is -16..15 and attenuation
/// depth -128..127; both are passed as signed bytes. Requires the origin-audio
/// queries' initialized scratch stack and projection state. Retains no storage.
static inline void _madChaserDangleLandPlayCue(Task* task, u32 cue)
{
    enum { MAD_CHASER_DANGLE_SOUND_INSTANCE_SHIFT = 8 };
    u32 soundId;
    s32 audioPan;

    soundId   = ((Enemy*)task->spawnArg2.pointer)->placeKey;
    soundId >>= ENEMY_PLACE_INDEX_SHIFT;
    soundId <<= MAD_CHASER_DANGLE_SOUND_INSTANCE_SHIFT;
    soundId  |= cue;
    // Keep the low-byte sign extension before querying attenuation depth.
    audioPan   = worldCoordGetOriginAudioPan(task->extra.tmd->coords) << 24;
    audioPan >>= 24;
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Completes the dangle landing animation and enters the combat walk.
///
/// Requires live enemy/model/work storage, initialized animation slots and a
/// stateFrames counter reset by the fall. Increments the wrapping u16 counter
/// and plays positional character-bank cues 4 and 3 on signed frames 1 and 2.
/// A slot-1 boundary, control jump or held pose enters combat at its walk
/// behavior and sub-state zero; the caller applies animation and collision.
static void _madChaserDangleLand(Task* task)
{
    MadChaserWork* work;

    work = task->work;
    if ((s16)++work->stateFrames == 1) {
        _madChaserDangleLandPlayCue(task, SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 4));
    }
    if ((s16)work->stateFrames == 2) {
        _madChaserDangleLandPlayCue(task, SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3));
    }
    if (_madChaserAnimHasBoundaryStatus(task)) {
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
    }
}
