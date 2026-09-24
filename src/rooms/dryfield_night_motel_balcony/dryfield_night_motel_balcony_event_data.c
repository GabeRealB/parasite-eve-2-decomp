#include "common.h"

#include "main/task.h"

void func_dryfield_night_motel_balcony_8017DC30(Task* task);
void func_dryfield_night_motel_balcony_8017DD0C(Task* task);

/// The room task's three states: setup, the per-tick balcony event check,
/// and exit.
const TaskFuncTable3 D_dryfield_night_motel_balcony_8017D5DC = {
    func_dryfield_night_motel_balcony_8017DC30,
    func_dryfield_night_motel_balcony_8017DD0C,
    taskKill,
};
