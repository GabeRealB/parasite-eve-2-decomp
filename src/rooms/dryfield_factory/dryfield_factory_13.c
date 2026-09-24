#include "common.h"

#include "gameplay/3CD8.h"
#include "main/task.h"

extern TaskDesc* D_dryfield_factory_8018A3C0;
extern Task**    D_dryfield_factory_8018A3C8;

/// Spawns entry 0 of the table the entry task selected into the slot it
/// parked, then kills itself once that task has been killed.
void func_dryfield_factory_8017DD00(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *D_dryfield_factory_8018A3C8 = Task_SpawnFromTable(D_dryfield_factory_8018A3C0, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(*D_dryfield_factory_8018A3C8, &poll) != 0) {
                taskKill(task);
            }
            return;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_factory/dryfield_factory_13", D_dryfield_factory_8017D5DC);
