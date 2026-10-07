/* Part of the factory lift library; see factory_lift.h. */

/// Spawns the script task from the table the room entry task selected, parks it
/// in the entry task's slot, and kills itself once that task has gone.
void factoryPanelSpawn(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *gFactoryPanelSlot = taskSpawnFromTable(gFactoryPanelDesc, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (taskPollKill(*gFactoryPanelSlot, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}
