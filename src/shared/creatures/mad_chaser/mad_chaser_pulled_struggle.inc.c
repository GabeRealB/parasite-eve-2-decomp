/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Holds a dead resisting pull in its settle pose, except for a blast reaction.
///
/// Requires `work == task->work` and a current clip in 1..19. Clip 8 selects
/// clip 5 before the first leap or clip 6 afterwards; other clips use the
/// carrier's one-based settle table. A non-blast sets busy and requests a
/// four-frame blend at normal rate on the live work block. A blast only clears
/// busy, retaining the animation. Playback and task transitions belong to the
/// caller; this helper borrows the work and leaves state/subState intact.
static inline void _madChaserPulledStruggleRequestSettle(Task* task, MadChaserWork* work)
{
    if (work->hitReaction != MAD_CHASER_HIT_REACTION_BLAST) {
        work->busy = 1;
        if (work->animId == MAD_CHASER_PULL_LEAP_CLIP) {
            if (work->hasLeaped == 0) {
                MadChaserWork* requestWork = task->work;

                requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
                requestWork->animRate        = ANIMATION_RATE_ONE;
                requestWork->animId          = MAD_CHASER_PULL_LOW_SETTLE_CLIP;
                requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            } else {
                MadChaserWork* requestWork = task->work;

                requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
                requestWork->animRate        = ANIMATION_RATE_ONE;
                requestWork->animId          = MAD_CHASER_PULL_UPRIGHT_SETTLE_CLIP;
                requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            }
        } else {
            MadChaserWork* requestWork;
            s16            settleClip;

            settleClip                   = _gMadChaserSettleAnims[work->animId - 1];
            requestWork                  = task->work;
            requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
            requestWork->animRate        = ANIMATION_RATE_ONE;
            requestWork->animId          = settleClip;
            requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
    } else {
        work->busy = 0;
    }
}

/// Resists a scripted pull while accelerating toward the room's capture point.
///
/// Requires live work, enemy/model storage and pullPoint in the root's parent
/// frame. While alive, ramps the rate from 16 through 31 over four counts per unit,
/// then switches to 64 (rates are in sixteenths of a frame). It
/// turns by 24 of 4096 angle units outside a +/-256 dead band, and backs away by
/// animRate game units. Also accelerates toward the point by moveSpeed/64 each
/// callback, with s16 motion/offset narrowing and a wrapping u16 frame counter.
/// After signed frame 120, sets busy and eases Y by 1/16 within 3000 units or
/// 1/32 farther away; earlier death uses 1/32. Earlier live slot-1 boundary/jump/
/// held status plays the resistance cue. A 3D distance below 800 advances before
/// the HP check; otherwise dead non-blast enemies request a four-frame settle
/// blend and dead blasts clear busy. The caller ticks animation and collision.
static void _madChaserPulledStruggle(Task* task)
{
    enum {
        MAD_CHASER_STRUGGLE_PULL_MAX_RATE            = 64,
        MAD_CHASER_STRUGGLE_PULL_TURN_DEAD_BAND      = 256,
        MAD_CHASER_STRUGGLE_PULL_TURN_STEP           = 24,
        MAD_CHASER_STRUGGLE_PULL_RESIST_DISTANCE     = -16,
        MAD_CHASER_STRUGGLE_PULL_RATE_FRACTION_BITS  = 4,
        MAD_CHASER_STRUGGLE_PULL_SPEED_FRACTION_BITS = 6,
        MAD_CHASER_STRUGGLE_PULL_Y_EASE_FRAME        = 120,
        MAD_CHASER_STRUGGLE_PULL_FAST_Y_DISTANCE     = 3000,
        MAD_CHASER_STRUGGLE_PULL_FAST_Y_SHIFT        = 4,
        MAD_CHASER_STRUGGLE_PULL_SLOW_Y_SHIFT        = 5,
        MAD_CHASER_STRUGGLE_PULL_SOUND               = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 1),
    };
    TmdObject*     model;
    MadChaserWork* work;
    Enemy*         enemy;
    GfxCoord*      root;
    GfxCoord*      liveRoot;
    VECTOR         pullOffset;
    SVECTOR        pullDirection;
    VECTOR         squaredOffset;
    VECTOR*        squareResult;
    s16            moveHeading;
    s32            pullDistance;
    s32            soundId;
    s32            audioPan;

    model = task->extra.tmd;
    work  = task->work;
    root  = model->coords;
    enemy = task->spawnArg2.pointer;
    work->stateFrames++;
    if (enemy->hp > 0) {
        MadChaserWork* headingWork;
        s32            headingError;
        s32            distanceAtNormalRate;
        s32            stepDistance;

        if ((u32)(work->stateScratch >> 1) < MAD_CHASER_STRUGGLE_PULL_MAX_RATE) {
            work->animRate = work->stateScratch >> 2;
            work->stateScratch++;
        } else {
            work->animRate = MAD_CHASER_STRUGGLE_PULL_MAX_RATE;
        }
        headingWork      = task->work;
        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        headingError = (((u16)headingWork->rotation.vy - ratan2(pullDirection.vx, pullDirection.vz)) << 20) >> 20;
        if (headingError > MAD_CHASER_STRUGGLE_PULL_TURN_DEAD_BAND) {
            headingWork->rotation.vy -= MAD_CHASER_STRUGGLE_PULL_TURN_STEP;
        } else if (headingError < -MAD_CHASER_STRUGGLE_PULL_TURN_DEAD_BAND) {
            headingWork->rotation.vy += MAD_CHASER_STRUGGLE_PULL_TURN_STEP;
        }
        moveHeading                           = work->rotation.vy;
        distanceAtNormalRate                  = MAD_CHASER_STRUGGLE_PULL_RESIST_DISTANCE;
        stepDistance                          = ((((MadChaserWork*)task->work)->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_STRUGGLE_PULL_RATE_FRACTION_BITS)) >> 16;
        task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    // The pull accelerates even while the live enemy pushes away.
    work->moveAccel++;
    work->moveSpeed += work->moveAccel;
    {
        s32 stepDistance = work->moveSpeed >> MAD_CHASER_STRUGGLE_PULL_SPEED_FRACTION_BITS;

        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        moveHeading                           = ratan2(pullDirection.vx, pullDirection.vz);
        task->extra.tmd->coords->coord.t[0]  += ((rsin(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(moveHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    // Measure capture in 3D before allowing a dead enemy to settle.
    pullOffset.vx = root->coord.t[0] - work->pullPoint.vx;
    pullOffset.vy = root->coord.t[1] - work->pullPoint.vy;
    pullOffset.vz = root->coord.t[2] - work->pullPoint.vz;
    squareResult  = &squaredOffset;
    gte_ldlvl(&pullOffset);
    gte_sqr0();
    gte_stlvnl(squareResult);
    pullDistance = SquareRoot0(squaredOffset.vx + squaredOffset.vy + squaredOffset.vz);
    if ((s16)work->stateFrames > MAD_CHASER_STRUGGLE_PULL_Y_EASE_FRAME) {
        if (pullDistance <= MAD_CHASER_STRUGGLE_PULL_FAST_Y_DISTANCE) {
            work->busy        = 1;
            root->coord.t[1] += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_STRUGGLE_PULL_FAST_Y_SHIFT;
        } else {
            work->busy        = 1;
            root->coord.t[1] += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_STRUGGLE_PULL_SLOW_Y_SHIFT;
        }
    } else if (enemy->hp > 0) {
        if (_madChaserAnimHasBoundaryStatusInline(task)) {
            soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_STRUGGLE_PULL_SOUND;
            audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
        }
    } else {
        work->busy        = 1;
        root->coord.t[1] += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_STRUGGLE_PULL_SLOW_Y_SHIFT;
    }
    if (pullDistance < MAD_CHASER_PULL_CAPTURE_DISTANCE) {
        work->subState++;
        return;
    }
    if (enemy->hp <= 0) {
        _madChaserPulledStruggleRequestSettle(task, work);
    }
}

/// Detaches a captured Mad Chaser and hands its destruction to timed despawn.
///
/// Requires live work, enemy and model storage after the pull reaches its capture
/// radius. Marks busy and plays the positional death cue unless HP is negative.
/// Releases this enemy's alert ownership, target link and battle reference with
/// rewards, detaches borrowed contacts and all three collision bodies, then
/// enters despawn at behavior/sub-state zero. Sends event 0 to placed actor 0
/// and hides active drawing; work, enemy and model stay live for delayed teardown.
static void _madChaserPulledIn(Task* task)
{
    enum {
        MAD_CHASER_PULL_CAPTURE_SOUND        = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3),
        MAD_CHASER_PULL_CAPTURE_PLACED_ACTOR = 0,
        MAD_CHASER_PULL_CAPTURE_EVENT        = 0,
    };
    MadChaserWork* collisionWork;
    Enemy*         enemy;
    TmdObject*     model;
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;

    work       = task->work;
    enemy      = task->spawnArg2.pointer;
    model      = task->extra.tmd;
    work->busy = 1;
    if (enemy->hp >= 0) {
        soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_PULL_CAPTURE_SOUND;
        audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    }
    if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
        gSceneCombatState.madChaserAlertOwner = 0;
    }
    // Detach list users before handing live allocations to delayed despawn.
    worldTargetUnlinkNode(&enemy->node);
    sceneReleaseBattleRefWithRewards(task, 0);
    enemy->recs   = 0;
    collisionWork = task->work;
    worldCollisionUnlinkBody(&collisionWork->pairBody);
    worldCollisionUnlinkBody(&collisionWork->gridBody);
    worldCollisionUnlinkBody(&collisionWork->attackBody);
    _madChaserEnterTaskState(task, MAD_CHASER_TASK_DESPAWN);
    taskMessageDispatch(sceneFindPlacedActor(MAD_CHASER_PULL_CAPTURE_PLACED_ACTOR), ROOM_MESSAGE_ACTOR_EVENT, MAD_CHASER_PULL_CAPTURE_EVENT, 0);
    model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
}

/// Holds a dead limp pull in its settle pose, except for a blast reaction.
///
/// Requires `work == task->work` and a current clip in 1..19. Clip 8 selects
/// clip 5 before the first leap or clip 6 afterwards; other clips use the
/// carrier's one-based settle table. A non-blast sets busy and requests a
/// four-frame blend at normal rate on the live work block. A blast only clears
/// busy, retaining the animation. Playback and task transitions belong to the
/// caller; this helper borrows the work and leaves state/subState intact.
static inline void _madChaserPulledLimpRequestSettle(Task* task, MadChaserWork* work)
{
    s16 settleClip;

    if (work->hitReaction != MAD_CHASER_HIT_REACTION_BLAST) {
        work->busy = 1;
        if (work->animId == MAD_CHASER_PULL_LEAP_CLIP) {
            if (work->hasLeaped == 0) {
                MadChaserWork* requestWork = task->work;

                requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
                requestWork->animRate        = ANIMATION_RATE_ONE;
                requestWork->animId          = MAD_CHASER_PULL_LOW_SETTLE_CLIP;
                requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            } else {
                MadChaserWork* requestWork = task->work;

                requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
                requestWork->animRate        = ANIMATION_RATE_ONE;
                requestWork->animId          = MAD_CHASER_PULL_UPRIGHT_SETTLE_CLIP;
                requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
            }
        } else {
            MadChaserWork* requestWork;

            settleClip                   = _gMadChaserSettleAnims[work->animId - 1];
            requestWork                  = task->work;
            requestWork->animBlendFrames = MAD_CHASER_PULL_SETTLE_BLEND_FRAMES;
            requestWork->animRate        = ANIMATION_RATE_ONE;
            requestWork->animId          = settleClip;
            requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        }
    } else {
        work->busy = 0;
    }
}

/// Accelerates the limp root toward the room's pull point until capture.
///
/// Requires live work, enemy/model storage and pullPoint in the root's parent
/// frame. Increments the u16 frame count and s16 acceleration/speed each call.
/// Before signed frame 90, horizontal distance is moveSpeed/64 per callback;
/// later it is moveSpeed/32, busy is set and Y eases by 1/16 of its remaining
/// offset. Direction and motion fields narrow to s16; coordinates use game units.
/// A 3D distance below 800 advances the sub-state before checking HP. Otherwise,
/// dead non-blast enemies request their settle clip blended over four normal-rate
/// frames; a dead blast clears busy. The caller updates animation and collision.
static void _madChaserPulledLimp(Task* task)
{
    enum {
        MAD_CHASER_LIMP_PULL_FAST_FRAME          = 90,
        MAD_CHASER_LIMP_PULL_SPEED_FRACTION_BITS = 6,
        MAD_CHASER_LIMP_PULL_Y_EASE_SHIFT        = 4,
    };
    TmdObject*     model;
    MadChaserWork* work;
    Enemy*         enemy;
    GfxCoord*      root;
    GfxCoord*      liveRoot;
    VECTOR         pullOffset;
    SVECTOR        pullDirection;
    VECTOR         squaredOffset;
    VECTOR*        squareResult;
    s16            pullHeading;

    model = task->extra.tmd;
    work  = task->work;
    enemy = task->spawnArg2.pointer;
    root  = model->coords;
    work->stateFrames++;
    work->moveAccel++;
    work->moveSpeed += work->moveAccel;
    if ((s16)work->stateFrames < MAD_CHASER_LIMP_PULL_FAST_FRAME) {
        s32 stepDistance = work->moveSpeed >> MAD_CHASER_LIMP_PULL_SPEED_FRACTION_BITS;

        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        pullHeading                           = ratan2(pullDirection.vx, pullDirection.vz);
        task->extra.tmd->coords->coord.t[0]  += ((rsin(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        s32 stepDistance;

        work->busy       = 1;
        stepDistance     = work->moveSpeed >> (MAD_CHASER_LIMP_PULL_SPEED_FRACTION_BITS - 1);
        liveRoot         = task->extra.tmd->coords;
        pullDirection.vx = work->pullPoint.vx - liveRoot->coord.t[0];
        pullDirection.vy = 0;
        pullDirection.vz = work->pullPoint.vz - liveRoot->coord.t[2];
        VectorNormalSS(&pullDirection, &pullDirection);
        pullHeading                           = ratan2(pullDirection.vx, pullDirection.vz);
        task->extra.tmd->coords->coord.t[0]  += ((rsin(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->coord.t[2]  += ((rcos(pullHeading) << 4) * stepDistance) >> 16;
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        root->coord.t[1]                     += (work->pullPoint.vy - root->coord.t[1]) >> MAD_CHASER_LIMP_PULL_Y_EASE_SHIFT;
    }
    // Measure capture in 3D before allowing a dead enemy to settle.
    pullOffset.vx = root->coord.t[0] - work->pullPoint.vx;
    pullOffset.vy = root->coord.t[1] - work->pullPoint.vy;
    pullOffset.vz = root->coord.t[2] - work->pullPoint.vz;
    squareResult  = &squaredOffset;
    gte_ldlvl(&pullOffset);
    gte_sqr0();
    gte_stlvnl(squareResult);
    if (SquareRoot0(squaredOffset.vx + squaredOffset.vy + squaredOffset.vz) < MAD_CHASER_PULL_CAPTURE_DISTANCE) {
        work->subState++;
        return;
    }
    if (enemy->hp <= 0) {
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
        _madChaserPulledLimpRequestSettle(task, work);
    }
}

#include "gameplay/room_effects.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs one room-command drop-death frame, including periodic blast effects.
///
/// Requires live enemy/model/work storage with at least nine coordinates and
/// work->state in 0..4. Hidden actors skip drawing and all frame work. Running
/// actors increment the s16 frame counter, dispatch one copied handler, spawn
/// a blast at part 1 every 32 counts and dirty the root. Running and paused
/// actors both update colour and optional limb shadows, then enable drawing.
static void _madChaserDropDeathTick(Task* task)
{
    enum { MAD_CHASER_DROP_DEATH_BLAST_INTERVAL_FRAMES = 32 };
    TmdObject*     model  = task->extra.tmd;
    MadChaserWork* work   = task->work;
    GfxCoord*      root   = model->coords;
    TaskFuncTable5 states = _gMadChaserDropDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            states.funcs[(s16)work->state](task);
            if (!(work->frameCount & (MAD_CHASER_DROP_DEATH_BLAST_INTERVAL_FRAMES - 1))) {
                effectSpawnHit(EFFECT_HIT_KIND_BLAST, &task->extra.tmd->coords[1], NULL, &work->effectArg);
            }
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            // Colour and shadows stay live while state progression is paused.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawLimbShadow(task, 2, 6, 0xC8, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 1, 7, 0x80, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 7, 8, 0x80, 0, 0xFF);
            }
            model->flags &= ~TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
}

#include "gameplay/room_effects.h"

/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Runs one room-command shrink-death frame, including periodic blast effects.
///
/// Requires live enemy/model/work storage with at least nine coordinates and
/// work->state in 0..6. Hidden actors skip drawing and all frame work. Running
/// actors increment the s16 frame counter, dispatch one copied handler, spawn
/// a blast at part 1 every 32 counts and dirty the root. Running and paused
/// actors both update colour and optional limb shadows. Retains the draw-skip
/// flag set by the hidden mode or the completed shrink.
static void _madChaserShrinkDeathTick(Task* task)
{
    enum { MAD_CHASER_SHRINK_DEATH_BLAST_INTERVAL_FRAMES = 32 };
    TmdObject*     model  = task->extra.tmd;
    MadChaserWork* work   = task->work;
    GfxCoord*      root   = model->coords;
    TaskFuncTable7 states = _gMadChaserShrinkDeathStates;

    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_HIDDEN:
            model->flags |= TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
        case SCENE_COMBAT_ACTORS_RUNNING:
            work->frameCount++;
            states.funcs[(s16)work->state](task);
            if (!(work->frameCount & (MAD_CHASER_SHRINK_DEATH_BLAST_INTERVAL_FRAMES - 1))) {
                effectSpawnHit(EFFECT_HIT_KIND_BLAST, &task->extra.tmd->coords[1], NULL, &work->effectArg);
            }
            root->composeStamp = GRAPHICS_COORD_DIRTY;
            // Colour and shadows stay live while state progression is paused.
        case SCENE_COMBAT_ACTORS_PAUSED:
            _madChaserUpdateColor(task->spawnArg2.pointer, &task->extra.tmd->coords[1]);
            if (work->shadowHidden == 0) {
                _madChaserDrawLimbShadow(task, 2, 6, 0xC8, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 1, 7, 0x80, 0, 0xFF);
                _madChaserDrawLimbShadow(task, 7, 8, 0x80, 0, 0xFF);
            }
            return;
    }
}

/// Queues the Mad Chaser sound-bank bundle once for the current encounter.
///
/// Uses global-CDF group 10, suffix 44, file 1 by default; Shelter B3 dumping
/// hole and garbage incinerator variants 1 and 2 select files 2 and 3.
/// The scene's shared enemy-bank latch is set when queued, before load completion.
/// Requires a live session and room in the CD queue; argument bytes are copied
/// synchronously, so the local blocks need not survive the call.
static void _madChaserQueueSoundBank(void)
{
    enum {
        MAD_CHASER_SOUND_FILE_GROUP     = 10,
        MAD_CHASER_SOUND_FILE_SUFFIX    = 44,
        MAD_CHASER_SOUND_FILE_DEFAULT   = 1,
        MAD_CHASER_SOUND_FILE_VARIANT_1 = 2,
        MAD_CHASER_SOUND_FILE_VARIANT_2 = 3,
    };
    u8 fileKeyBytes[4];
    u8 loadArgs[sizeof(((CdCmdEntry*)0)->args.bytes)];

    if (gSceneCombatState.enemySoundBankQueued == 0) {
        // The queue copies each selected file key and its load options immediately.
        if (gGameSession->location.stage == GAME_STAGE_MINE_SHELTER && (u32)(gGameSession->location.area - GAME_AREA_SHELTER_B3_DUMPING_HOLE) < 2 && gGameSession->location.variant == 1) {
            fileKeyBytes[2] = MAD_CHASER_SOUND_FILE_GROUP;
            fileKeyBytes[0] = MAD_CHASER_SOUND_FILE_VARIANT_1;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = MAD_CHASER_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        } else if (gGameSession->location.stage == GAME_STAGE_MINE_SHELTER && (u32)(gGameSession->location.area - GAME_AREA_SHELTER_B3_DUMPING_HOLE) < 2 && gGameSession->location.variant == 2) {
            fileKeyBytes[2] = MAD_CHASER_SOUND_FILE_GROUP;
            fileKeyBytes[0] = MAD_CHASER_SOUND_FILE_VARIANT_2;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = MAD_CHASER_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        } else {
            fileKeyBytes[2] = MAD_CHASER_SOUND_FILE_GROUP;
            fileKeyBytes[0] = MAD_CHASER_SOUND_FILE_DEFAULT;
            fileKeyBytes[3] = 0;
            loadArgs[0]     = MAD_CHASER_SOUND_FILE_SUFFIX;
            loadArgs[3]     = 0;
            loadArgs[2]     = 0;
            loadArgs[1]     = CD_COMMAND_LOAD_DEFAULT;
            cdCmdEnqueue(CD_COMMAND_LOAD_FILE, fileKeyBytes, loadArgs);
        }
        gSceneCombatState.enemySoundBankQueued = 1;
    }
}

/// Dispatches the scripted vanish's detachment and timed teardown phases.
///
/// The hidden form's task state 7 requires live work/enemy/model storage and
/// work->state in 0..1: zero detaches and hides the enemy, one releases its
/// primitive buffer on frame 3 and destroys it from frame 36. Dispatch retains
/// the signed-halfword selector. The callback may begin teardown; no storage is
/// retained by this stack table. The separate despawn dispatcher uses task state 5.
static void _madChaserVanishState(Task* task)
{
    MadChaserWork* work               = task->work;
    TaskFunc       behaviorHandlers[] = {
        _madChaserVanish,
        _madChaserVanishFree,
    };

    behaviorHandlers[(s16)work->state](task);
}

/// Joins a claimed Mad Chaser alert and returns 1 when the task was switched to combat.
///
/// Any alert owner suffices, including this enemy. Combat starts at its alert
/// behavior with sub-state zero; an unclaimed alert returns 0 and retains the
/// current state. Requires a live Mad Chaser work block.
static s16 _madChaserJoinAlert(Task* task)
{
    if ((s8)gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED) {
        _madChaserEnterTaskState(task, MAD_CHASER_TASK_COMBAT);
        _madChaserSetBehaviorStateS16(task, MAD_CHASER_COMBAT_STATE_ALERT);
        return 1;
    }
    return 0;
}

/// Claims or releases the shared Mad Chaser alert for this enemy's placement index.
///
/// A nonzero low halfword of claim acquires only an unclaimed alert. Zero releases
/// an alert whose low nibble equals this enemy's index, even if the claim bit is
/// clear. The task's Enemy spawn argument must be live; its placement index is 0..15.
static void _madChaserSetAlertHold(Task* task, s32 claim)
{
    if ((s16)claim != 0) {
        if (!((s8)gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED)) {
            Enemy* enemy = task->spawnArg2.pointer;

            gSceneCombatState.madChaserAlertOwner = (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) | SCENE_COMBAT_MAD_CHASER_ALERT_CLAIMED;
        }
    } else {
        Enemy* enemy = task->spawnArg2.pointer;

        if ((gSceneCombatState.madChaserAlertOwner & SCENE_COMBAT_MAD_CHASER_OWNER_MASK) == (enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT)) {
            gSceneCombatState.madChaserAlertOwner = 0;
        }
    }
}

/// Consumes a pending combat hit reaction when the hit latch is set.
///
/// Requires live Mad Chaser work in the combat task state. Reactions 1..5
/// select light recoil, heavy recoil, status hold, heavy recoil and knockdown,
/// restarting the selected behavior at sub-state zero. Other reactions only
/// clear the request. Returns 1 exactly when hitTaken equals 1, including an
/// absent or unsupported reaction, and 0 otherwise. The hit latch is retained.
static s16 _madChaserTakeHitRequest(Task* task)
{
    return _madChaserTakeHitReaction(task);
}

/// Latches a supported room command for the Mad Chaser's state machine.
///
/// Accepts only context key 0x2C00 (synthetic stage 0, area 44) and kinds 1..5.
/// Copies the whole command halfword, retaining its entry-move and spot bits;
/// unsupported requests leave the pending command intact. Borrows request only
/// through dispatch. messageId and unusedSecondArg are ignored; no result is returned.
static void _madChaserQueueCommand(Task* task, s32 messageId, const ActorCommand* request, s32 unusedSecondArg)
{
    enum {
        MAD_CHASER_COMMAND_CONTEXT = 0x2C00,
    };
    MadChaserWork* work = task->work;

    if (request->context.key == MAD_CHASER_COMMAND_CONTEXT) {
        switch (request->command & MAD_CHASER_COMMAND_KIND_MASK) {
            case MAD_CHASER_COMMAND_EMERGE:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_PULL:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_VANISH:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_DROP_DEATH:
                work->command = request->command;
                break;
            case MAD_CHASER_COMMAND_SHRINK_DEATH:
                work->command = request->command;
                break;
        }
    }
}

/// Places the model root from the position of a borrowed actor transform.
///
/// Coordinates are whole units in the root's parent frame. Only placement.pos
/// is read; rotation, messageId and unusedSecondArg are ignored. Marks composition
/// dirty without updating work's anchor or saved position. Requires a live TMD
/// root and a readable placement through synchronous dispatch; returns no result.
static void _madChaserPlaceRoot(Task* task, s32 messageId, const ActorTransform* placement, s32 unusedSecondArg)
{
    GfxCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = placement->pos.vx;
    coord->coord.t[1]   = placement->pos.vy;
    coord->coord.t[2]   = placement->pos.vz;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Translates the model root to keep one part at a world-space anchor.
///
/// Requires a live model with 0 <= partIndex < partCount and its root parented
/// directly to the view coordinate. worldAnchor supplies signed-halfword XYZ
/// in whole game-coordinate units; only those six bytes are read, and pad is
/// ignored. Borrows the anchor for the call. Composes the part, removes the view
/// transform from both part and root, and subtracts their world-space offset
/// from the anchor. Writes full-width root XYZ and dirties the selected part;
/// the caller invalidates the root before its next composition.
static void _madChaserPinPart(Task* task, s16 partIndex, const SVECTOR* worldAnchor)
{
    MATRIX    rootToWorld;
    MATRIX    partToWorld;
    GfxCoord* partCoord;
    GfxCoord* coords;

    coords    = task->extra.tmd->coords;
    partCoord = &coords[partIndex];
    actorRenderComposeCoord(partCoord);
    // Remove the view so the offset and anchor share the root's parent frame.
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &coords->workm, &rootToWorld);
    gfxMakeRelativeTransform(&gGfxViewCoord.workm, &partCoord->workm, &partToWorld);
    coords->coord.t[0]      = worldAnchor->vx - (partToWorld.t[0] - rootToWorld.t[0]);
    coords->coord.t[1]      = worldAnchor->vy - (partToWorld.t[1] - rootToWorld.t[1]);
    coords->coord.t[2]      = worldAnchor->vz - (partToWorld.t[2] - rootToWorld.t[2]);
    partCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Scales a normal-rate movement distance by the task's animation rate.
///
/// Rate is in sixteenths of a frame. The result is a signed distance in the same
/// parent-coordinate units as distanceAtNormalRate, rounded down. The shifts keep
/// only the signed low 20 bits of the product before division by 16; inputs whose
/// product fits that range scale without wrap. Requires a live Mad Chaser work block.
static s32 _madChaserScaleByAnimRate(Task* task, s16 distanceAtNormalRate)
{
    enum {
        MAD_CHASER_ANIM_RATE_FRACTION_BITS = 4,
    };
    MadChaserWork* work = task->work;

    return (s32)((work->animRate * distanceAtNormalRate) << (16 - MAD_CHASER_ANIM_RATE_FRACTION_BITS)) >> 16;
}

/// Returns 1 when slot 1 reports an animation boundary, control jump, or held boundary.
///
/// Reads the latest tick/walk status without consuming it. A looping clip can
/// report a jump while continuing to play; this is also the walk's footstep gate.
/// Requires the task's initialized Mad Chaser animation storage.
static s16 _madChaserAnimHasBoundaryStatus(Task* task)
{
    MadChaserWork* work = task->work;

    if ((work->slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) ||
        (work->slots[1].status.word & (ANIMATION_SLOT_FOLLOWED_JUMP | ANIMATION_SLOT_SETTLED))) {
        return 1;
    }
    return 0;
}

/// Dispatches one task-state frame of the Mad Chaser form that can emerge.
///
/// `Task::state` must be in 0..9 (`MAD_CHASER_TASK_*`), with that handler's storage
/// initialized and all callbacks loaded. Copies the carrier's ten handlers
/// before calling the selected one once; the handler owns any state changes
/// and teardown. The task descriptor supplies the model and enemy instance.
static void _madChaserHiddenTask(Task* task)
{
    TaskFuncTable10 states;

    states = _gMadChaserHiddenTaskStates;
    states.funcs[task->state](task);
}

/// Dispatches one task-state frame of the ordinary Mad Chaser form.
///
/// Task::state must be in 0..5 (spawn through despawn), with that handler's
/// storage initialized and callbacks loaded. Copies the carrier's six handlers
/// before calling the selected one once; the handler owns state changes and
/// teardown. The descriptor supplies the live model and enemy spawn argument.
static void _madChaserTask(Task* task)
{
    TaskFuncTable6 states;

    states = _gMadChaserTaskStates;
    states.funcs[task->state](task);
}

/// Selects this carrier's static void(Task*) task-state-5 dispatcher.
#define MAD_CHASER_DESPAWN_STATE _madChaserDespawnState

// Bind MAD_CHASER_DESPAWN_STATE to a declared static void(Task*) handler before
// inclusion, then undefine it. It names only this definition; the callbacks are
// the Mad Chaser counter initializer and timed despawn handlers.

/// Runs task state 5: initializes the despawn counter, then waits for destruction.
///
/// Requires live Mad Chaser work with `state` in 0..1: 0 clears `stateFrames`
/// and advances to 1; 1 counts 36 subsequent ticks before destroying the enemy.
/// The task and enemy must remain live until destruction, after which neither
/// may be accessed. The signed-halfword selector is retained for dispatch.
static void MAD_CHASER_DESPAWN_STATE(Task* task)
{
    MadChaserWork* work               = task->work;
    TaskFunc       behaviorHandlers[] = {
        _madChaserAdvanceBehaviorState,
        _madChaserDespawn,
    };

    behaviorHandlers[(s16)work->state](task);
}

#undef MAD_CHASER_DESPAWN_STATE

/// Turns the root heading by a fixed step toward the tracked player.
///
/// Uses the XZ offset last stored by player tracking, in the root's parent frame.
/// Heading and turnStep use 4096 units per turn; a signed 12-bit error within
/// 256 units leaves the heading unchanged. Walk callers supply positive steps
/// 16..32. The step is not clamped to the error, and the stored halfword may wrap;
/// the rotation update normalizes it later. Requires live work and a tracked offset.
static void _madChaserTurnToPlayer(Task* task, s32 turnStep)
{
    enum {
        MAD_CHASER_HEADING_BITS   = 12,
        MAD_CHASER_TURN_DEAD_ZONE = ACTOR_TRANSFORM_ANGLE_TURN / 16,
    };
    MadChaserWork* work = task->work;
    SVECTOR        horizontalOffset;
    s32            yawError;
    u16            currentYaw;
    s32            targetYaw;

    horizontalOffset.vx = work->toPlayer.vx;
    horizontalOffset.vy = 0;
    horizontalOffset.vz = work->toPlayer.vz;
    VectorNormalSS(&horizontalOffset, &horizontalOffset);
    targetYaw  = ratan2(-horizontalOffset.vx, -horizontalOffset.vz);
    currentYaw = work->rotation.vy;
    // Wrap the error to a signed turn before choosing the rotation direction.
    yawError = ((currentYaw - targetYaw) << (32 - MAD_CHASER_HEADING_BITS)) >> (32 - MAD_CHASER_HEADING_BITS);
    if (yawError > MAD_CHASER_TURN_DEAD_ZONE) {
        work->rotation.vy = currentYaw - turnStep;
    } else if (yawError < -MAD_CHASER_TURN_DEAD_ZONE) {
        work->rotation.vy = currentYaw + turnStep;
    }
}
