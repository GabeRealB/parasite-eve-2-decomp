/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Waits two updating death frames before the blast burst step.
///
/// Requires live Mad Chaser work in ordinary-death behavior 7, with stateFrames
/// reset on entry. The counter wraps as u16 and its low halfword is compared
/// signed; reaching two advances to behavior 8. Does not tick animation.
static void _madChaserDeathPause(Task* task)
{
    enum { MAD_CHASER_DEATH_BLAST_PAUSE_FRAMES = 2 };
    u16            elapsedFrames;
    MadChaserWork* work;

    work              = task->work;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames >= MAD_CHASER_DEATH_BLAST_PAUSE_FRAMES) {
        work->state = work->state + 1;
    }
}

/// Unlinks the burst corpse's three collision bodies without releasing storage.
///
/// bodyWork must be live with valid links for any linked body. Detaches pair,
/// grid and attack bodies in order, clearing linked bodies' pass-enable flags
/// and list links. Already unlinked bodies are unchanged. Their shape kinds and
/// contact arrays stay owned by the task until despawn releases its work block.
static inline void _madChaserBurstUnlinkBodies(MadChaserWork* bodyWork)
{
    worldCollisionUnlinkBody(&bodyWork->pairBody);
    worldCollisionUnlinkBody(&bodyWork->gridBody);
    worldCollisionUnlinkBody(&bodyWork->attackBody);
}

/// Bursts the dead body into effects and hands its task to despawn.
///
/// Requires a live enemy/model/work block and an acquired battle reference.
/// Releases the primitive buffer and suppresses its automatic reallocation,
/// spawns gibs, then releases the battle reference with rewards. Detaches the
/// enemy's borrowed contacts and unlinks pair/grid/attack bodies in that order.
/// Enters despawn with behavior and sub-state zero; model and work allocations
/// remain task-owned for that later teardown.
static void _madChaserBurst(Task* task)
{
    MadChaserWork* work;
    MadChaserWork* resetWork;
    TmdObject*     model;
    Enemy*         enemy;

    model = task->extra.tmd;
    enemy = task->spawnArg2.pointer;
    tmdFreePrimitiveBuffer(model);
    model->flags |= TMD_OBJECT_SKIP_AUTO_BUFFER;
    _madChaserSpawnGibs(task);
    sceneReleaseBattleRefWithRewards(task, 0);
    enemy->recs = NULL;
    work        = task->work;
    _madChaserBurstUnlinkBodies(work);
    resetWork           = task->work;
    task->state         = MAD_CHASER_TASK_DESPAWN;
    resetWork->state    = 0;
    resetWork->subState = 0;
}

/// Starts the next behavior state with its frame counter reset.
///
/// Requires a live Mad Chaser work block and a next entry in the active behavior
/// table. The 16-bit state increment wraps; the task state and sub-state are retained.
static void _madChaserAdvanceBehaviorState(Task* task)
{
    MadChaserWork* work;

    work              = task->work;
    work->stateFrames = 0;
    work->state       = work->state + 1;
}

/// Destroys the enemy after 36 despawn ticks and signals the applicable Shelter room.
///
/// The work counter must begin at zero; it is incremented before its signed
/// halfword is tested. In variant 1 of the Dumping Hole or Garbage Incinerator,
/// placed actor 0 receives the completion event before destruction. The task and
/// its Enemy spawn argument must remain live until this call releases them.
static void _madChaserDespawn(Task* task)
{
    enum {
        MAD_CHASER_DESPAWN_DELAY_FRAMES = 36,
        MAD_CHASER_DESPAWN_ROOM_EVENT   = 1,
    };
    MadChaserWork* work;
    u16            elapsedFrames;

    work              = task->work;
    elapsedFrames     = work->stateFrames + 1;
    work->stateFrames = elapsedFrames;
    if ((s16)elapsedFrames >= MAD_CHASER_DESPAWN_DELAY_FRAMES) {
        if ((gGameSession->location.stage == GAME_STAGE_MINE_SHELTER) && ((u32)(gGameSession->location.area - GAME_AREA_SHELTER_B3_DUMPING_HOLE) < 2U) && (gGameSession->location.variant == 1)) {
            taskMessageDispatch(sceneFindPlacedActor(0), ROOM_MESSAGE_ACTOR_EVENT, MAD_CHASER_DESPAWN_ROOM_EVENT, 0);
        }
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}

/// Requests the light-recoil clip with an eight-frame normal-rate blend.
///
/// Requires initialized task-owned animation storage. Reloads live request
/// work; the frame callback applies the request and ticks the animation.
static inline void _madChaserRecoilLightRequestClip(Task* task, s16 recoilClip)
{
    enum { MAD_CHASER_LIGHT_RECOIL_BLEND_FRAMES = 8 };
    MadChaserWork* requestWork = task->work;

    requestWork->animBlendFrames = MAD_CHASER_LIGHT_RECOIL_BLEND_FRAMES;
    requestWork->animRate        = ANIMATION_RATE_ONE;
    requestWork->animId          = recoilClip;
    requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
}

/// Starts light recoil and remembers the interrupted clip's stance for recovery.
///
/// Requires live enemy/model/work storage, initialized animation and a current
/// clip in 1..19. Upright stance blends to clip 11 and stops the instance-zero
/// alert cry while retaining voice release; other stances blend to clip 17.
/// Plays positional character-bank entry 3 at the root's already-composed origin
/// and advances subState to recovery. The blend lasts eight normal-rate frames;
/// playback belongs to the combat frame callback. Origin-audio scratch/projection
/// requirements apply, and all task/model storage stays live through the call.
static void _madChaserRecoilLight(Task* task)
{
    enum {
        MAD_CHASER_LIGHT_RECOIL_LOW_CLIP             = 17,
        MAD_CHASER_LIGHT_RECOIL_SOUND                = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3),
        MAD_CHASER_LIGHT_RECOIL_SOUND_INSTANCE_SHIFT = 8,
    };
    MadChaserWork* work;
    s32            soundId;
    s32            audioPan;

    work               = task->work;
    work->stateScratch = _gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
        _madChaserRecoilLightRequestClip(task, MAD_CHASER_LIGHT_RECOIL_UPRIGHT_CLIP);
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    } else {
        _madChaserRecoilLightRequestClip(task, MAD_CHASER_LIGHT_RECOIL_LOW_CLIP);
    }
    soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_LIGHT_RECOIL_SOUND_INSTANCE_SHIFT) | MAD_CHASER_LIGHT_RECOIL_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work->subState++;
}

/// Starts heavy recoil with an animation selected by the interrupted stance.
///
/// Requires live work, enemy/model storage and an animId with a stance-table entry.
/// Saves that stance in stateScratch for recovery. Upright recoil blends clip 12
/// over two normal-rate frames and stops the global alert cry retaining release;
/// low recoil blends clip 17 over eight frames. Plays the positional hurt/death
/// cue and advances the sub-state; animation playback belongs to the caller.
static void _madChaserRecoilHeavy(Task* task)
{
    enum {
        MAD_CHASER_HEAVY_RECOIL_UPRIGHT_CLIP         = 12,
        MAD_CHASER_HEAVY_RECOIL_UPRIGHT_BLEND_FRAMES = 2,
        MAD_CHASER_HEAVY_RECOIL_LOW_CLIP             = 17,
        MAD_CHASER_HEAVY_RECOIL_LOW_BLEND_FRAMES     = 8,
        MAD_CHASER_HEAVY_RECOIL_SOUND                = SOUND_CHARACTER(SOUND_BANK_MAD_CHASER, 3),
    };
    MadChaserWork* work;
    MadChaserWork* requestWork;
    s32            soundId;
    s32            audioPan;

    work               = task->work;
    work->stateScratch = _gMadChaserAnimStance[work->animId - 1];
    if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_HEAVY_RECOIL_UPRIGHT_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_HEAVY_RECOIL_UPRIGHT_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
        sndEvtRequestScriptStop(SOUND_MAD_CHASER_ALERT_CRY, SOUND_SCRIPT_STOP_KEEP_RELEASE);
    } else {
        requestWork                  = task->work;
        requestWork->animBlendFrames = MAD_CHASER_HEAVY_RECOIL_LOW_BLEND_FRAMES;
        requestWork->animRate        = ANIMATION_RATE_ONE;
        requestWork->animId          = MAD_CHASER_HEAVY_RECOIL_LOW_CLIP;
        requestWork->animRequest     = MAD_CHASER_ANIM_REQUEST_BLEND;
    }
    soundId  = ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAD_CHASER_HEAVY_RECOIL_SOUND;
    audioPan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
    work->subState++;
}

/// Returns heavy recoil to walking or alert when slot 1 reaches a boundary.
///
/// Requires live work/enemy storage and the interrupted stance in stateScratch.
/// Boundary, jump and held-pose status all qualify without consuming the status.
/// Upright stance selects combat walking; low stance claims alert ownership and
/// selects alert behavior. Both reset the sub-state and retain task/animation.
static void _madChaserRecoilHeavyEnd(Task* task)
{
    MadChaserWork* work;

    work = task->work;
    if (_madChaserAnimHasBoundaryStatusInline(task)) {
        if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
        } else {
            _madChaserSetAlertHold(task, 1);
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_ALERT);
        }
    }
}
