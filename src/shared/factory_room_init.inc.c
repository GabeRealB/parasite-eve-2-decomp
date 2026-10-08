/* Part of the factory lift library; see factory_lift.h. */

/// Registers the factory room task and spawns its lift and barrier collision.
///
/// Owns one zeroed `Task*` slot at `Task::work`, also published as
/// `gFactoryPanelSlot`; room teardown releases it. The lift and panel poller
/// borrow that slot, which must outlive both. Runtime stage selects the day
/// spawn/panel tables for daytime Dryfield and night tables otherwise, then
/// power progress controls view 9's sprite. Allocation/spawning is unchecked.
/// Starts at task state 0 and advances to the idle room-message receiver.
static void _factoryRoomInit(Task* task)
{
    enum { FACTORY_ROOM_LIFT_SPAWN_INDEX    = 4,
           FACTORY_ROOM_BARRIER_SPAWN_INDEX = 5 };
    Task** slot;

    task->msgTable = gFactoryMsgTable;
    gameSetTaskSlot(task, GAME_TASK_SLOT_ROOM);
    slot       = (gFactoryPanelSlot = memCalloc(sizeof(*slot), 0));
    task->work = slot;
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
    taskSpawnFromTable(gFactorySpawnTable, FACTORY_ROOM_LIFT_SPAWN_INDEX, 0, gFactoryPanelSlot);
    taskSpawnFromTable(gFactorySpawnTable, FACTORY_ROOM_BARRIER_SPAWN_INDEX, 0, 0);
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        dryfieldFactorySetView9SpriteVisible(gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) & 0xFF);
    } else {
        dryfieldNightFactorySetView9SpriteVisible(gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) & 0xFF);
    }
    task->state++;
}
