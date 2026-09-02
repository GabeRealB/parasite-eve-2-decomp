#include "common.h"

#include "main/task.h"

extern TaskDesc RoomsShared8017e320Desc;
extern Task*    RoomsShared8017e320Task;

void RoomsShared8017e320(void)
{
    RoomsShared8017e320Task = Task_SpawnFromTable(&RoomsShared8017e320Desc, 1, 0, 0);
}
