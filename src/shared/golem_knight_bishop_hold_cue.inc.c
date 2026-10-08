/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Advances the grab release cue and returns scripted control to the player.
///
/// With a live player task and nonzero grab stage, plays the cue at stored tick
/// 20, then increments the signed halfword counter. From incremented tick 95,
/// an animation-complete reply ends scripted control and clears the grab stage.
/// The sequence dispatcher skips the grab itself, whose release paths call this.
static void _golemKnightBishopTickGrabRelease(Task* task)
{
    enum {
        GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_CUE_TICK  = 20,
        GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_MIN_TICKS = 95,
    };
    GolemKnightBishopWork* work;
    s16                    releaseTimer;
    s32                    sound;
    s32                    audioPan;
    Task*                  player;
    GfxCoord*              root;

    work   = task->work;
    root   = task->extra.tmd->coords;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->grabStage != GOLEM_KNIGHT_BISHOP_GRAB_NONE) {
        if (work->grabReleaseTimer == GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_CUE_TICK) {
            sound    = gGolemKnightBishopHoldCue | ((((Enemy*)task->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            audioPan = (s8)worldCoordGetOriginAudioPan(root);
            sndEvtRequestScriptStart(sound, audioPan, (s8)worldCoordGetOriginAudioDepth(root));
        }
        releaseTimer           = work->grabReleaseTimer + 1;
        work->grabReleaseTimer = releaseTimer;
        if ((releaseTimer >= GOLEM_KNIGHT_BISHOP_GRAB_RELEASE_MIN_TICKS) && (taskMessageDispatch(player, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
            taskMessageDispatch(player, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->grabStage = GOLEM_KNIGHT_BISHOP_GRAB_NONE;
        }
    }
}
