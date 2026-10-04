/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// While `grabStage` is set, counts `grabReleaseTimer` up and queues the
/// vocal cue at tick 0x14. From tick 0x5F on, once the player's animation is
/// no longer playing, it ends the player's scripted mode and clears
/// `grabStage`.
void golemKnightBishopHoldCueTimer(Task* arg0)
{
    GolemKnightBishopWork* work;
    s16                    timer;
    s32                    sound;
    s32                    pan;
    Task*                  slot;
    GfxCoord*              coord;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    slot  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (work->grabStage != 0) {
        if (work->grabReleaseTimer == 0x14) {
            sound = gGolemKnightBishopHoldCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        timer                  = work->grabReleaseTimer + 1;
        work->grabReleaseTimer = timer;
        if ((timer >= 0x5F) && (taskMessageDispatch(slot, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
            taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->grabStage = 0;
        }
    }
}
