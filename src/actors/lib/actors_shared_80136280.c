#include "common.h"
#include "main/task.h"

extern TaskDesc ActorsShared80136280Desc;

void ActorsShared80136280(s32 arg0)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 2, arg0, 0);
}
