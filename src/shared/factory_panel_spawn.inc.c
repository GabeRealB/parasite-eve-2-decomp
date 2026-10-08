/* Part of the factory lift library; see factory_lift.h. */

void factoryPanelSpawn(Task* task)
{
    enum { FACTORY_PANEL_SPAWN_INIT = 0,
           FACTORY_PANEL_SPAWN_WAIT = 1 };
    s32 scriptResult;

    switch (task->state) {
        case FACTORY_PANEL_SPAWN_INIT:
            *gFactoryPanelSlot = taskSpawnFromTable(gFactoryPanelDesc, 0, 0, 0);
            task->state++;
            return;
        case FACTORY_PANEL_SPAWN_WAIT:
            if (taskPollKill(*gFactoryPanelSlot, &scriptResult) != 0) {
                taskKill(task);
            }
            return;
    }
}
