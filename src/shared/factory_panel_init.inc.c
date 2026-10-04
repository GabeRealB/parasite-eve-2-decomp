/* Part of the factory lift library; see factory_lift.h. */

/// Task callback of the descriptor at `gFactoryPromptDesc`:
/// allocates the script work block, spawns the room's child task, picks the
/// global mode byte from game flag 0x48, steps the task on one state and clears
/// the room's hotspot list.
void factoryPanelInit(Task* task)
{
    FactoryPanelWork*    work;
    ActionPromptHotspot* hs;

    work = memCalloc(sizeof(FactoryPanelWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->spawnArg2.pointer = Task_SpawnFromTable(gFactoryPromptDesc, 0, 1, 0);
    task->work              = work;
    task->msgTable          = gFactoryPanelMsgTable;
    if (gameFlagGetNibble(GAME_FLAG_FACTORY_POWER_ON) == 0) {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 0xC;
    } else {
        gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.location.loc.view = 5;
    }
    task->state++;
    Display_AcquireRef();
    for (hs = gFactoryPanelHotspots; hs->id != ACTION_PROMPT_HOTSPOT_END; hs++) {
        hs->hit = 0;
    }
    gGameSession->cutsceneHold = 1;
    gGameSession->hideHud      = 1;
    gGameSession->eventState   = 1;
    work->scanDelay            = 0;
}
