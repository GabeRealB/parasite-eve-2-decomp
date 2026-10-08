/* Part of the factory lift library; see factory_lift.h. */

/// Room entry task: publishes the room's message table, claims game pointer
/// slot 7 and parks a fresh one-word slot at `Task::work` (also kept in
/// `gFactoryPanelSlot`) for the poller to fill. It then picks the spawn tables for
/// the session variant (`stage == 2` or not), spawns entries 4 and 5 of the
/// first, and passes progress nibble 0x48 to the variant's view-sprite helper.
void factoryRoomInit(Task* arg0)
{
    Task** slot;

    arg0->msgTable = gFactoryMsgTable;
    gameSetTaskSlot(arg0, GAME_TASK_SLOT_ROOM);
    slot       = (gFactoryPanelSlot = memCalloc(4, 0));
    arg0->work = slot;
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        gFactorySpawnTable = gFactoryDaySpawnTable;
    } else {
        gFactorySpawnTable = gFactoryNightSpawnTable;
    }
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        gFactoryPanelDesc = gFactoryDayPanelDesc;
    } else {
        gFactoryPanelDesc = gFactoryNightPanelDesc;
    }
    taskSpawnFromTable(gFactorySpawnTable, 4, 0, gFactoryPanelSlot);
    taskSpawnFromTable(gFactorySpawnTable, 5, 0, 0);
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        dryfieldFactorySetView9SpriteVisible(gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) & 0xFF);
    } else {
        dryfieldNightFactorySetView9SpriteVisible(gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) & 0xFF);
    }
    arg0->state++;
}
