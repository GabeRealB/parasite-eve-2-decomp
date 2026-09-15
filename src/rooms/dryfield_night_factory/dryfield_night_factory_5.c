#include "common.h"

#include "main/task.h"

extern TaskDesc* D_dryfield_night_factory_8018A7E0;
extern Task**    D_dryfield_night_factory_8018A7E8;

/// `D_..._A7E0` / `D_..._A7E4` hold the address of whichever spawn table
/// `func_dryfield_night_factory_80180438` selected for the current session
/// (it stores `&D_..._80186E94` / `&D_..._80186E28` / ... into them), and
/// `D_..._A7E8` points at the `Mem_Calloc(4, 0)` slot the spawned task is
/// parked in. Both are read as values here, which is why the target loads
/// them (`lw $a0, %lo(...)`) rather than forming `&symbol`.
INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_5", func_dryfield_night_factory_80180438);

INCLUDE_ASM("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_5", func_dryfield_night_factory_80180574);

void func_dryfield_night_factory_8018076C(Task* task)
{
    s32 poll;

    switch (task->state) {
        case 0:
            *D_dryfield_night_factory_8018A7E8 = Task_SpawnFromTable(D_dryfield_night_factory_8018A7E0, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(*D_dryfield_night_factory_8018A7E8, &poll) != 0) {
                Task_Kill(task);
            }
            return;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_night_factory/dryfield_night_factory_5", RoomsShared8017fc38Table);
