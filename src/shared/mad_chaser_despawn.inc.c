/* Part of the Mad Chaser library; see mad_chaser.h. */

/// After 0x24 frames destroys the enemy, first telling placed actor 0 with
/// message 0x13F4 when in place 1 of stage 4 areas 0x27/0x28.
void madChaserDespawn(Task* arg0)
{
    MadChaserWork* work;
    u16            ticks;

    work              = (MadChaserWork*)arg0->work;
    ticks             = work->stateFrames + 1;
    work->stateFrames = ticks;
    if ((s16)ticks >= 0x24) {
        if ((gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER) && ((u32)(gGameSession->location.loc.area - 0x27) < 2U) && (gGameSession->location.loc.variant == 1)) {
            taskMessageDispatch(sceneFindPlacedActor(0), ROOM_MESSAGE_ACTOR_EVENT, 1, 0);
        }
        enemyDestroy(arg0->spawnArg2.pointer, arg0);
    }
}
