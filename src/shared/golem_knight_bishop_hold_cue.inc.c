/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// While field_718 is armed, it queues the vocal cue at tick 0x14. From tick
/// 0x5F on, once the player accepts message 0x3ED, it sends 0x3F1 to release
/// the player and disarms.
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
    if (work->field_718 != 0) {
        if (work->field_71A == 0x14) {
            sound = gGolemKnightBishopHoldCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            pan   = (s8)worldCoordGetOriginAudioPan(coord);
            SndEvt_EnqueueType6(sound, pan, (s8)worldCoordGetOriginAudioDepth(coord));
        }
        timer           = (u16)work->field_71A + 1;
        work->field_71A = timer;
        if ((timer >= 0x5F) && (taskMessageDispatch(slot, ANIMATION_MESSAGE_IS_PLAYING, 0, 0) == 0)) {
            taskMessageDispatch(slot, GAME_ACTOR_MESSAGE_END_SCRIPTED, 0, 0);
            work->field_718 = 0;
        }
    }
}
