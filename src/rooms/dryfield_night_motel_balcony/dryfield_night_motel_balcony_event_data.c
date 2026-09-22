#include "common.h"

#include "main/task.h"

void func_dryfield_night_motel_balcony_8017DC30(Task* task);
void func_dryfield_night_motel_balcony_8017DD0C(Task* task);

/// Task entry points the shared room-event body dispatches through. Its own
/// translation unit: the next object's rodata opens with a compiler-generated
/// jump table, whose `.align 3` is measured from that object's section start,
/// so folding this table in ahead of it makes the assembler pad. The original
/// build has no such padding, so the two were separate objects there too.
const TaskFuncTable3 RoomsShared8017d878Table = {
    func_dryfield_night_motel_balcony_8017DC30,
    func_dryfield_night_motel_balcony_8017DD0C,
    taskKill,
};
