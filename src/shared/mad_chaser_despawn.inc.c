/* Part of the Mad Chaser library; see mad_chaser.h. */

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
        if ((gGameSession->location.loc.stage == GAME_STAGE_MINE_SHELTER) && ((u32)(gGameSession->location.loc.area - GAME_AREA_SHELTER_B3_DUMPING_HOLE) < 2U) && (gGameSession->location.loc.variant == 1)) {
            taskMessageDispatch(sceneFindPlacedActor(0), ROOM_MESSAGE_ACTOR_EVENT, MAD_CHASER_DESPAWN_ROOM_EVENT, 0);
        }
        enemyDestroy(task->spawnArg2.pointer, task);
    }
}
