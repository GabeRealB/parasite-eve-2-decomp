/* Part of the factory lift library; see factory_lift.h. */

/// Opens an operator-panel session with a cursor and a power-dependent saved view.
///
/// Requires the room's live hotspot and cursor descriptor tables, initialized
/// session/save state and entry at FACTORY_PANEL_STATE_INIT. Owns zeroed panel
/// work and retains the cursor in spawnArg2 for teardown; cursor spawning must
/// succeed. Acquires the menu hold and hides ordinary play until panel exit.
/// Allocation failure kills the panel without acquiring the hold.
static void _factoryPanelInit(Task* task)
{
    enum {
        FACTORY_PANEL_CURSOR_PORT_0 = 1,
        FACTORY_PANEL_EVENT_ACTIVE  = 1
    };
    FactoryPanelWork*    work;
    ActionPromptHotspot* hotspot;

    work = memCalloc(sizeof(FactoryPanelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = taskSpawnFromTable(gFactoryPromptDesc, 0, FACTORY_PANEL_CURSOR_PORT_0, 0);
    task->work              = work;
    task->msgTable          = gFactoryPanelMsgTable;
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_VIEW_UNPOWERED;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = FACTORY_PANEL_VIEW_POWERED;
    }
    task->state++;
    displayAcquireMenuHold();
    // Discard the previous scan before the new cursor can confirm a hotspot.
    for (hotspot = gFactoryPanelHotspots; hotspot->id != ACTION_PROMPT_HOTSPOT_END; hotspot++) {
        hotspot->hit = 0;
    }
    gGameSession->cutsceneHold = true;
    gGameSession->hideHud      = true;
    gGameSession->eventState   = FACTORY_PANEL_EVENT_ACTIVE;
    work->scanDelay            = 0;
}
