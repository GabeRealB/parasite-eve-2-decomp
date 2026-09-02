#include "common.h"
#include "main/task.h"

extern Task* RoomsShared8017e320Task;

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_4", func_dryfield_main_street_8017E354);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_4", func_dryfield_main_street_8017E3A8);

void func_dryfield_main_street_8017E4A4(void)
{
    RoomsShared8017e320Task = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_4", func_dryfield_main_street_8017E4B0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_main_street/dryfield_main_street_4", func_dryfield_main_street_8017E830);
