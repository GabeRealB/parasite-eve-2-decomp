/* Part of the Mad Chaser library; see mad_chaser.h. */

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
