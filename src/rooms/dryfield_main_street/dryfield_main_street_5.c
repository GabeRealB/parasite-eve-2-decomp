#include "common.h"
#include "main/task.h"

extern Task* RoomsShared8017e320Task;

void func_dryfield_main_street_8017E354(s32 arg0)
{
    Task* t = RoomsShared8017e320Task;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    Task_Kill(RoomsShared8017e320Task);
    RoomsShared8017e320Task = NULL;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_5", func_dryfield_main_street_8017E3A8);

void func_dryfield_main_street_8017E4A4(void)
{
    RoomsShared8017e320Task = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_5", func_dryfield_main_street_8017E4B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_5", func_dryfield_main_street_8017E830);
