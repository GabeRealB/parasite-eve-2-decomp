#include "common.h"

#include "main/task.h"

extern TaskDesc RoomsShared801830f0Desc;

void RoomsShared801830f0Sub(s16 arg0, s16 arg1, s32 arg2);

void RoomsShared801830f0(s16 arg0, s16 arg1, s16 arg2)
{
    RoomsShared801830f0Sub(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&RoomsShared801830f0Desc, 0, arg2, 0);
}
