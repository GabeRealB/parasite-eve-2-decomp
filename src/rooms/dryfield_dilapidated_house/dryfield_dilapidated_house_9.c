#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"

/// Exit callback `func_dryfield_dilapidated_house_80180B84` installs on its
/// task: it kills the task.
void func_dryfield_dilapidated_house_80180FB8(Task* task)
{
    taskKill(task);
}
